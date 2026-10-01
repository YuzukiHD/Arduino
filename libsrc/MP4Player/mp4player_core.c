/* SPDX-License-Identifier: Apache-2.0 */
/*
 * The audio is the master clock: a picture waits until the sound has reached
 * its time stamp, a picture that is too late is dropped.
 */

#include <errno.h>
#include <ff.h>
#include <pvmp4audiodecoder_api.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/audio/codec.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/display/display_sunxi.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/i2s_sunxi.h>
#include <zephyr/drivers/vdec.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/mp4.h>

#include "mp4player_core.h"

#define AAC_FRAMES	1024
#define PCM_BLOCK	(AAC_FRAMES * 2 * sizeof(int16_t))
#define PCM_BLOCKS	12
#define PREFILL		6
/* pictures later than this behind the sound are not shown */
#define DROP_US		(2 * 33367)
/* pictures are put up this much before their time, the display waits for a refresh */
#define LEAD_US		8000
#define VIDEO_WINDOW	(256 * 1024)
#define AUDIO_WINDOW	(64 * 1024)

static FATFS fat_fs;
static struct fs_mount_t mp = {
	.type = FS_FATFS,
	.fs_data = &fat_fs,
	.mnt_point = "/SD:",
};

/*
 * A file with a read window: the samples of a track are small and spread over
 * the file, a seek in FAT walks the cluster chain, so a window is read at a
 * time and the samples are cut out of it.
 */
struct reader {
	struct fs_file_t f;
	uint8_t *win;
	size_t win_size;
	uint64_t win_off;
	size_t win_len;
	bool open;
};

K_MEM_SLAB_DEFINE_STATIC(pcm_slab, PCM_BLOCK, PCM_BLOCKS, 4);
K_THREAD_STACK_DEFINE(audio_stack, 8192);
static struct k_thread audio_thread;

static const struct device *const codec_i2s = DEVICE_DT_GET(DT_NODELABEL(audio_codec));
static const struct device *const codec_ctl = DEVICE_DT_GET(DT_NODELABEL(codec_analog));
static const struct device *const vdec_dev = DEVICE_DT_GET(DT_NODELABEL(ve));
static const struct device *const disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

/* state of the playback in progress */
static struct {
	bool initialised;
	bool active;
	bool audio_thread_started;
	struct mp4 mp4;
	struct reader vfile, afile;
	struct vdec_stream *stream;
	struct mp4_iter vit;
	struct mp4p_info info;
	struct vdec_frame shown, older;
	uint8_t *raw, *ab;
	size_t ab_len, ab_used;
	int64_t pending_pts;
	int64_t start_ms;
	int64_t last_offset_us;
	int64_t position_us;
	uint32_t shown_count, dropped_count;
	bool eof, flushed, first;
	uint8_t annexb_head[512];
	size_t annexb_head_len;
	int nal_len_size;
	char err[48];
} P;

static volatile bool audio_running;
static volatile bool audio_done;
static volatile bool audio_stop;
static volatile uint32_t audio_underruns;

static void set_err(const char *what, int code)
{
	snprintf(P.err, sizeof(P.err), "%s (%d)", what, code);
}

static int file_read(void *ctx, uint64_t off, void *buf, size_t len)
{
	struct reader *r = ctx;
	int ret;
	ssize_t n;

	if (len > r->win_size) {
		ret = fs_seek(&r->f, (off_t)off, FS_SEEK_SET);
		n = ret == 0 ? fs_read(&r->f, buf, len) : ret;
		return n == (ssize_t)len ? 0 : (n < 0 ? (int)n : -EIO);
	}
	if (off < r->win_off || off + len > r->win_off + r->win_len) {
		ret = fs_seek(&r->f, (off_t)off, FS_SEEK_SET);
		if (ret != 0) {
			return ret;
		}
		n = fs_read(&r->f, r->win, r->win_size);
		if (n < (ssize_t)len) {
			r->win_len = 0;
			return n < 0 ? (int)n : -EIO;
		}
		r->win_off = off;
		r->win_len = n;
	}
	memcpy(buf, r->win + (off - r->win_off), len);

	return 0;
}

static int reader_open(struct reader *r, const char *path, size_t win)
{
	int ret;

	fs_file_t_init(&r->f);
	r->win_size = win;
	r->win_off = 0;
	r->win_len = 0;
	r->win = aligned_alloc(64, win);
	if (r->win == NULL) {
		return -ENOMEM;
	}
	ret = fs_open(&r->f, path, FS_O_READ);
	r->open = ret == 0;

	return ret;
}

static void reader_close(struct reader *r)
{
	if (r->open) {
		fs_close(&r->f);
	}
	free(r->win);
	memset(r, 0, sizeof(*r));
}

/* ---- audio ---------------------------------------------------------------------------- */

/* Position of the sound in microseconds, -1 before it started */
static int64_t audio_clock_us(void)
{
	uint32_t blocks, cycle;

	if (!audio_running) {
		return -1;
	}
	i2s_sunxi_codec_tx_position(codec_i2s, &blocks, &cycle);

	return (int64_t)blocks * AAC_FRAMES * 1000000 / P.mp4.audio.sample_rate +
	       (int64_t)k_cyc_to_us_floor64(k_cycle_get_32() - cycle);
}

static void audio_main(void *a, void *b, void *c)
{
	const struct mp4_track *t = &P.mp4.audio;
	struct i2s_config cfg = {
		.word_size = 16,
		.channels = 2,
		.format = I2S_FMT_DATA_FORMAT_I2S,
		.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
		.frame_clk_freq = t->sample_rate,
		.mem_slab = &pcm_slab,
		.block_size = PCM_BLOCK,
		.timeout = 2000,
	};
	tPVMP4AudioDecoderExternal ext = {0};
	void *dec = malloc(PVMP4AudioDecoderGetMemRequirements());
	int16_t *pcm = malloc(AAC_FRAMES * 2 * 2 * sizeof(int16_t));
	struct mp4_iter it;
	struct mp4_sample s;
	uint8_t *buf = malloc(t->max_sample);
	uint32_t skip = t->media_start / AAC_FRAMES;
	int queued = 0, ret;
	bool started = false;

	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);
	ext.desiredChannels = 2;
	ext.outputFormat = OUTPUTFORMAT_16PCM_INTERLEAVED;
	ext.aacPlusEnabled = false;
	ext.pOutputBuffer = pcm;
	if (pcm != NULL) {
		ext.pOutputBuffer_plus = pcm + AAC_FRAMES * 2;
	}
	if (dec == NULL || buf == NULL || pcm == NULL ||
	    PVMP4AudioDecoderInitLibrary(&ext, dec) != 0) {
		goto out;
	}
	/* the AudioSpecificConfig of the track tells the decoder the stream layout */
	ext.pInputBuffer = t->extra;
	ext.inputBufferCurrentLength = t->extra_len;
	ext.inputBufferUsedLength = 0;
	ext.remainderBits = 0;
	if (PVMP4AudioDecoderConfig(&ext, dec) != MP4AUDEC_SUCCESS) {
		goto out;
	}
	if (i2s_configure(codec_i2s, I2S_DIR_TX, &cfg) != 0) {
		goto out;
	}
	audio_codec_start_output(codec_ctl);
	started = true;

	mp4_iter_init(t, &it);
	while (!audio_stop && mp4_next(t, &it, &s) == 0) {
		void *blk;

		if (file_read(&P.afile, s.offset, buf, s.size) != 0) {
			break;
		}
		ext.pInputBuffer = buf;
		ext.inputBufferCurrentLength = s.size;
		ext.inputBufferUsedLength = 0;
		ext.remainderBits = 0;
		ret = PVMP4AudioDecodeFrame(&ext, dec);
		if (ret != MP4AUDEC_SUCCESS) {
			continue;
		}
		if (skip > 0U) {
			/* the encoder delay is not part of the programme */
			skip--;
			continue;
		}
		if (k_mem_slab_alloc(&pcm_slab, &blk, K_SECONDS(2)) != 0) {
			break;
		}
		/* desiredChannels is 2: a mono stream comes out duplicated */
		memcpy(blk, pcm, PCM_BLOCK);
		ret = i2s_write(codec_i2s, blk, PCM_BLOCK);
		if (ret == -EIO) {
			/* the stream ran dry: start it again */
			audio_underruns++;
			audio_running = false;
			k_mem_slab_free(&pcm_slab, blk);
			i2s_trigger(codec_i2s, I2S_DIR_TX, I2S_TRIGGER_PREPARE);
			queued = 0;
			continue;
		} else if (ret != 0) {
			k_mem_slab_free(&pcm_slab, blk);
			break;
		}
		if (!audio_running && ++queued >= PREFILL) {
			if (i2s_trigger(codec_i2s, I2S_DIR_TX, I2S_TRIGGER_START) == 0) {
				audio_running = true;
			}
		}
	}
	if (started) {
		if (!audio_running && queued > 0) {
			/* a short clip: play what was queued */
			i2s_trigger(codec_i2s, I2S_DIR_TX, I2S_TRIGGER_START);
		}
		if (audio_stop) {
			i2s_trigger(codec_i2s, I2S_DIR_TX, I2S_TRIGGER_DROP);
		} else {
			i2s_trigger(codec_i2s, I2S_DIR_TX, I2S_TRIGGER_DRAIN);
			k_sleep(K_MSEC(300));
		}
		audio_codec_stop_output(codec_ctl);
	}
out:
	free(dec);
	free(pcm);
	free(buf);
	audio_running = false;
	audio_done = true;
}

/* ---- video ---------------------------------------------------------------------------- */

/* SPS and PPS of the avcC record as Annex B */
static int avcc_parameter_sets(const uint8_t *e, uint32_t len)
{
	size_t o = 5, w = 0;
	int n;

	if (len < 7U || e[0] != 1) {
		return -EINVAL;
	}
	P.nal_len_size = (e[4] & 3) + 1;
	for (int pass = 0; pass < 2; pass++) {
		if (o >= len) {
			return -EINVAL;
		}
		n = pass == 0 ? (e[o] & 0x1f) : e[o];
		o++;
		for (int i = 0; i < n; i++) {
			size_t l;

			if (o + 2 > len) {
				return -EINVAL;
			}
			l = (e[o] << 8) | e[o + 1];
			o += 2;
			if (o + l > len || w + 4 + l > sizeof(P.annexb_head)) {
				return -EINVAL;
			}
			P.annexb_head[w++] = 0;
			P.annexb_head[w++] = 0;
			P.annexb_head[w++] = 0;
			P.annexb_head[w++] = 1;
			memcpy(P.annexb_head + w, e + o, l);
			w += l;
			o += l;
		}
	}
	P.annexb_head_len = w;

	return 0;
}

/* Length prefixed NAL units to start code prefixed ones; returns the new size */
static size_t avcc_to_annexb(const uint8_t *in, size_t len, uint8_t *out, bool with_head)
{
	size_t w = 0;

	if (with_head) {
		memcpy(out, P.annexb_head, P.annexb_head_len);
		w = P.annexb_head_len;
	}
	while (len > (size_t)P.nal_len_size) {
		size_t l = 0;

		for (int i = 0; i < P.nal_len_size; i++) {
			l = (l << 8) | *in++;
		}
		len -= P.nal_len_size;
		if (l > len) {
			break;
		}
		out[w++] = 0;
		out[w++] = 0;
		out[w++] = 0;
		out[w++] = 1;
		memcpy(out + w, in, l);
		w += l;
		in += l;
		len -= l;
	}

	return w;
}

static void show(const struct vdec_frame *f)
{
	struct display_sunxi_yuv yuv = {
		.y = f->plane[0],
		.uv = f->plane[1],
		.width = f->width,
		.height = f->height,
		.stride_y = f->stride[0],
		.stride_uv = f->stride[1],
		.bt709 = true,
		/* the picture is up at the next refresh, the decoder does not wait for it */
		.nonblock = true,
	};

	display_sunxi_show_yuv(disp, &yuv);
}

/*
 * Waits for the time of a picture; false when it is too late to show it.
 * Without sound the clock is the time since the start.
 */
static bool wait_for(int64_t pts_us)
{
	for (;;) {
		int64_t now = audio_running || !audio_done ? audio_clock_us() : -1;
		int64_t diff;

		if (now < 0) {
			if (audio_done) {
				now = (k_uptime_get() - P.start_ms) * 1000;
			} else {
				k_msleep(2);
				continue;
			}
		}
		diff = pts_us - now;
		P.last_offset_us = -diff;
		if (diff < -DROP_US) {
			return false;
		}
		if (diff <= LEAD_US) {
			return true;
		}
		k_msleep(MAX(1, (int)((diff - LEAD_US) / 1000)));
	}
}

/* ---- control -------------------------------------------------------------------------- */

int mp4p_begin(void)
{
	int ret;

	if (P.initialised) {
		return 0;
	}
	(void)device_init(disp); /* not initialized at boot */
	if (!device_is_ready(vdec_dev) || !device_is_ready(disp) || !device_is_ready(codec_i2s) ||
	    !device_is_ready(codec_ctl)) {
		set_err("device not ready", -ENODEV);
		return -ENODEV;
	}
	ret = fs_mount(&mp);
	if (ret != 0 && ret != -EBUSY) {
		set_err("SD mount", ret);
		return ret;
	}
	P.initialised = true;

	return 0;
}

static void finish(void)
{
	if (P.shown.priv != NULL || P.older.priv != NULL) {
		display_sunxi_hide_yuv(disp);
	}
	audio_stop = true;
	if (P.audio_thread_started) {
		k_thread_join(&audio_thread, K_SECONDS(5));
		P.audio_thread_started = false;
	}
	if (P.older.priv != NULL) {
		vdec_frame_release(vdec_dev, &P.older);
	}
	if (P.shown.priv != NULL) {
		vdec_frame_release(vdec_dev, &P.shown);
	}
	if (P.stream != NULL) {
		vdec_stream_close(vdec_dev, P.stream);
		P.stream = NULL;
	}
	free(P.raw);
	free(P.ab);
	P.raw = P.ab = NULL;
	if (P.mp4.video.sizes != NULL || P.mp4.audio.sizes != NULL || P.mp4.size != 0) {
		mp4_close(&P.mp4);
	}
	reader_close(&P.vfile);
	reader_close(&P.afile);
	memset(&P.mp4, 0, sizeof(P.mp4));
	memset(&P.shown, 0, sizeof(P.shown));
	memset(&P.older, 0, sizeof(P.older));
	P.active = false;
}

void mp4p_stop(void)
{
	if (P.active) {
		finish();
	}
}

int mp4p_playing(void)
{
	return P.active;
}

int mp4p_start(const char *path)
{
	const struct vdec_stream_config scfg = {
		.codec = VDEC_CODEC_H264,
		.format = VDEC_FORMAT_NV12,
		.buffer_size = 1024 * 1024,
	};
	const struct mp4_track *v;
	struct fs_dirent st;
	int ret;

	P.err[0] = 0;
	if (!P.initialised) {
		ret = mp4p_begin();
		if (ret != 0) {
			return ret;
		}
	}
	mp4p_stop();
	P.shown_count = P.dropped_count = 0;
	P.ab_len = P.ab_used = 0;
	P.pending_pts = -1;
	P.last_offset_us = 0;
	P.position_us = 0;
	P.eof = P.flushed = false;
	P.first = true;
	audio_running = false;
	audio_done = false;
	audio_stop = false;
	audio_underruns = 0;
	P.active = true;

	ret = fs_stat(path, &st);
	if (ret != 0) {
		set_err("no such file", ret);
		goto fail;
	}
	ret = reader_open(&P.vfile, path, VIDEO_WINDOW);
	if (ret == 0) {
		ret = reader_open(&P.afile, path, AUDIO_WINDOW);
	}
	if (ret != 0) {
		set_err("cannot open", ret);
		goto fail;
	}
	ret = mp4_open(&P.mp4, file_read, &P.vfile, st.size);
	if (ret != 0 || P.mp4.video.codec != MP4_CODEC_H264) {
		set_err("no H.264 video track", ret);
		ret = ret ? ret : -ENOTSUP;
		goto fail;
	}
	v = &P.mp4.video;
	ret = avcc_parameter_sets(v->extra, v->extra_len);
	if (ret != 0) {
		set_err("bad avcC record", ret);
		goto fail;
	}
	ret = vdec_stream_open(vdec_dev, &scfg, &P.stream);
	if (ret != 0) {
		set_err("cannot open the decoder", ret);
		goto fail;
	}
	P.raw = malloc(v->max_sample);
	P.ab = malloc(v->max_sample + 1024);
	if (P.raw == NULL || P.ab == NULL) {
		set_err("no memory", -ENOMEM);
		ret = -ENOMEM;
		goto fail;
	}
	mp4_iter_init(v, &P.vit);
	memset(&P.info, 0, sizeof(P.info));
	P.info.width = v->width;
	P.info.height = v->height;
	P.info.frames = v->sample_count;
	P.info.duration_ms = v->timescale ? (uint32_t)(v->duration * 1000 / v->timescale) : 0;
	P.start_ms = k_uptime_get();

	if (P.mp4.audio.codec == MP4_CODEC_AAC &&
	    (P.mp4.audio.sample_rate == 48000U || P.mp4.audio.sample_rate == 44100U)) {
		P.info.sample_rate = P.mp4.audio.sample_rate;
		k_thread_create(&audio_thread, audio_stack, K_THREAD_STACK_SIZEOF(audio_stack),
				audio_main, NULL, NULL, NULL, 3, 0, K_NO_WAIT);
		P.audio_thread_started = true;
	} else {
		/* no sound, or a rate that is not played: the picture runs on the system clock */
		audio_done = true;
	}

	return 0;
fail:
	finish();
	return ret;
}

int mp4p_update(void)
{
	const struct mp4_track *v = &P.mp4.video;
	struct vdec_frame frame;
	struct mp4_sample s;
	uint32_t used_before;
	int ret;

	if (!P.active) {
		return 0;
	}
	ret = vdec_stream_get_frame(vdec_dev, P.stream, &frame);
	if (ret == 0) {
		int64_t pts_us = frame.pts >= 0 ? frame.pts : 0;

		if (wait_for(pts_us)) {
			show(&frame);
			/* the picture before the last may still be scanned out */
			if (P.older.priv != NULL) {
				vdec_frame_release(vdec_dev, &P.older);
			}
			P.older = P.shown;
			P.shown = frame;
			P.shown_count++;
			P.position_us = pts_us;
		} else {
			vdec_frame_release(vdec_dev, &frame);
			P.dropped_count++;
		}
		return 1;
	}
	if (ret == -ENODATA) {
		finish();
		return 0;
	}
	if (ret == -EBUSY) {
		k_msleep(2);
		return 1;
	}
	if (ret != -EAGAIN) {
		set_err("decode error", ret);
		finish();
		return ret;
	}

	/* the decoder wants data */
	if (P.ab_used >= P.ab_len && !P.eof) {
		if (mp4_next(v, &P.vit, &s) != 0) {
			P.eof = true;
		} else {
			if (file_read(&P.vfile, s.offset, P.raw, s.size) != 0) {
				set_err("video read error", -EIO);
				finish();
				return -EIO;
			}
			P.ab_len = avcc_to_annexb(P.raw, s.size, P.ab, P.first || s.sync);
			P.ab_used = 0;
			P.first = false;
			P.pending_pts = (s.pts - v->media_start) * 1000000 / v->timescale;
			if (P.pending_pts < 0) {
				P.pending_pts = 0;
			}
		}
	}
	if (P.ab_used < P.ab_len) {
		size_t used = 0;

		used_before = P.ab_used;
		ret = vdec_stream_feed(vdec_dev, P.stream, P.ab + P.ab_used, P.ab_len - P.ab_used,
				       P.pending_pts, &used);
		if (ret != 0 && ret != -EAGAIN) {
			/* a damaged sample is skipped */
			P.ab_used = P.ab_len;
		} else {
			P.ab_used = used_before + used;
		}
	} else if (P.eof && !P.flushed) {
		vdec_stream_flush(vdec_dev, P.stream);
		P.flushed = true;
	}

	return 1;
}

void mp4p_info(struct mp4p_info *info)
{
	*info = P.info;
	info->shown = P.shown_count;
	info->dropped = P.dropped_count;
	info->underruns = audio_underruns;
	info->av_offset_ms = (int32_t)(P.last_offset_us / 1000);
	info->position_ms = (uint32_t)(P.position_us / 1000);
}

static int set_prop(audio_property_t prop, audio_property_value_t val)
{
	return audio_codec_set_property(codec_ctl, prop, AUDIO_CHANNEL_ALL, val);
}

int mp4p_set_volume(int volume)
{
	audio_property_value_t v = {.vol = CLAMP(volume, 0, 255)};

	return set_prop(AUDIO_PROPERTY_OUTPUT_VOLUME, v);
}

int mp4p_set_mute(int mute)
{
	audio_property_value_t v = {.mute = mute != 0};

	return set_prop(AUDIO_PROPERTY_OUTPUT_MUTE, v);
}

const char *mp4p_error(void)
{
	return P.err;
}
