/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <ff.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/fs/fs.h>
#include <zephyr/mp4.h>

#include "Arduino.h"
#include "MP4.h"

/* A file with a read window: a seek in FAT walks the cluster chain, so a window is read at a
 * time and the packets are cut out of it. */
struct Reader {
	struct fs_file_t f;
	uint8_t *win = nullptr;
	size_t winSize = 0;
	uint64_t winOff = 0;
	size_t winLen = 0;
	bool open = false;
};

static int reader_read(void *ctx, uint64_t off, void *buf, size_t len)
{
	Reader *r = static_cast<Reader *>(ctx);
	ssize_t n;
	int ret;

	if (len > r->winSize) {
		ret = fs_seek(&r->f, (off_t)off, FS_SEEK_SET);
		n = ret == 0 ? fs_read(&r->f, buf, len) : ret;
		return n == (ssize_t)len ? 0 : (n < 0 ? (int)n : -EIO);
	}
	if (off < r->winOff || off + len > r->winOff + r->winLen) {
		ret = fs_seek(&r->f, (off_t)off, FS_SEEK_SET);
		if (ret != 0) {
			return ret;
		}
		n = fs_read(&r->f, r->win, r->winSize);
		if (n < (ssize_t)len) {
			r->winLen = 0;
			return n < 0 ? (int)n : -EIO;
		}
		r->winOff = off;
		r->winLen = n;
	}
	memcpy(buf, r->win + (off - r->winOff), len);
	return 0;
}

static bool reader_open(Reader &r, const char *path, size_t win)
{
	fs_file_t_init(&r.f);
	r.winSize = win;
	r.win = (uint8_t *)aligned_alloc(64, win);
	if (!r.win || fs_open(&r.f, path, FS_O_READ) != 0) {
		return false;
	}
	r.open = true;
	return true;
}

static void reader_close(Reader &r)
{
	if (r.open) {
		fs_close(&r.f);
	}
	free(r.win);
	r.win = nullptr;
	r.open = false;
}

static FATFS fat_fs;
static struct fs_mount_t sd_mount = {};

static bool sd_mount_once(void)
{
	sd_mount.type = FS_FATFS;
	sd_mount.fs_data = &fat_fs;
	sd_mount.mnt_point = "/SD:";
	int ret = fs_mount(&sd_mount);
	return ret == 0 || ret == -EBUSY; /* -EBUSY: already mounted (by a library or the sketch) */
}

struct MP4Impl {
	struct mp4 m;
	struct mp4_iter vit, ait;
	Reader vr, ar; /* separate windows for the video and the audio cursor */
	bool fromFile = false;
	const uint8_t *mem = nullptr;
	MP4ReadFn userRead = nullptr;
	void *userCtx = nullptr;
	int nalLen = 4;
	uint8_t head[512];
	size_t headLen = 0;
	bool first = true;
};

struct MemCtx {
	const uint8_t *p;
	uint64_t size;
};
static int mem_read(void *ctx, uint64_t off, void *buf, size_t len)
{
	MemCtx *c = static_cast<MemCtx *>(ctx);
	if (off + len > c->size) {
		return -EIO;
	}
	memcpy(buf, c->p + off, len);
	return 0;
}

static bool parse_avcc(MP4Impl *i)
{
	const uint8_t *e = i->m.video.extra;
	uint32_t len = i->m.video.extra_len;
	size_t o = 5, w = 0;

	if (!e || len < 7 || e[0] != 1) {
		return false;
	}
	i->nalLen = (e[4] & 3) + 1;
	for (int pass = 0; pass < 2; pass++) {
		if (o >= len) {
			return false;
		}
		int n = pass == 0 ? (e[o] & 0x1f) : e[o];
		o++;
		for (int k = 0; k < n; k++) {
			if (o + 2 > len) {
				return false;
			}
			size_t l = (e[o] << 8) | e[o + 1];
			o += 2;
			if (o + l > len || w + 4 + l > sizeof(i->head)) {
				return false;
			}
			i->head[w++] = 0;
			i->head[w++] = 0;
			i->head[w++] = 0;
			i->head[w++] = 1;
			memcpy(i->head + w, e + o, l);
			w += l;
			o += l;
		}
	}
	i->headLen = w;
	return true;
}

/* the source of the demuxer: file window, memory or the user's function */
static int source_read(void *ctx, uint64_t off, void *buf, size_t len)
{
	MP4Impl *i = static_cast<MP4Impl *>(ctx);
	if (i->fromFile) {
		return reader_read(&i->vr, off, buf, len);
	}
	if (i->mem) {
		MemCtx c = {i->mem, i->m.size};
		return mem_read(&c, off, buf, len);
	}
	return i->userRead(i->userCtx, off, buf, len);
}

static bool finish_open(MP4Impl *i, uint64_t size)
{
	if (mp4_open(&i->m, source_read, i, size) != 0) {
		return false;
	}
	mp4_iter_init(&i->m.video, &i->vit);
	mp4_iter_init(&i->m.audio, &i->ait);
	if (i->m.video.codec == MP4_CODEC_H264) {
		parse_avcc(i);
	}
	return true;
}

bool MP4File::open(const char *path)
{
	close();
	if (!sd_mount_once()) {
		return false;
	}
	struct fs_dirent st;
	if (fs_stat(path, &st) != 0) {
		return false;
	}
	_impl = new MP4Impl();
	memset(&_impl->m, 0, sizeof(_impl->m));
	_impl->fromFile = true;
	if (!reader_open(_impl->vr, path, 256 * 1024) || !reader_open(_impl->ar, path, 64 * 1024) ||
	    !(_impl->m.size = st.size, finish_open(_impl, st.size))) {
		close();
		return false;
	}
	return true;
}

bool MP4File::open(const uint8_t *data, size_t size)
{
	close();
	_impl = new MP4Impl();
	memset(&_impl->m, 0, sizeof(_impl->m));
	_impl->mem = data;
	_impl->m.size = size;
	if (!finish_open(_impl, size)) {
		close();
		return false;
	}
	return true;
}

bool MP4File::open(MP4ReadFn read, void *ctx, uint64_t size)
{
	close();
	_impl = new MP4Impl();
	memset(&_impl->m, 0, sizeof(_impl->m));
	_impl->userRead = read;
	_impl->userCtx = ctx;
	_impl->m.size = size;
	if (!finish_open(_impl, size)) {
		close();
		return false;
	}
	return true;
}

void MP4File::close()
{
	if (!_impl) {
		return;
	}
	mp4_close(&_impl->m);
	reader_close(_impl->vr);
	reader_close(_impl->ar);
	delete _impl;
	_impl = nullptr;
}

#define V (_impl->m.video)
#define A (_impl->m.audio)

bool MP4File::hasVideo() const { return _impl && V.codec == MP4_CODEC_H264; }
bool MP4File::hasAudio() const { return _impl && A.codec == MP4_CODEC_AAC; }
uint16_t MP4File::width() const { return hasVideo() ? V.width : 0; }
uint16_t MP4File::height() const { return hasVideo() ? V.height : 0; }
uint32_t MP4File::videoFrames() const { return hasVideo() ? V.sample_count : 0; }
uint32_t MP4File::videoDurationMs() const
{
	return hasVideo() && V.timescale ? (uint32_t)(V.duration * 1000 / V.timescale) : 0;
}
uint32_t MP4File::maxVideoPacket() const { return hasVideo() ? V.max_sample : 0; }
const uint8_t *MP4File::videoExtra(size_t *len) const
{
	if (len) *len = hasVideo() ? V.extra_len : 0;
	return hasVideo() ? V.extra : nullptr;
}
uint32_t MP4File::audioSampleRate() const { return hasAudio() ? A.sample_rate : 0; }
uint16_t MP4File::audioChannels() const { return hasAudio() ? A.channels : 0; }
uint32_t MP4File::audioFrames() const { return hasAudio() ? A.sample_count : 0; }
uint32_t MP4File::audioDurationMs() const
{
	return hasAudio() && A.timescale ? (uint32_t)(A.duration * 1000 / A.timescale) : 0;
}
uint32_t MP4File::maxAudioPacket() const { return hasAudio() ? A.max_sample : 0; }
const uint8_t *MP4File::audioExtra(size_t *len) const
{
	if (len) *len = hasAudio() ? A.extra_len : 0;
	return hasAudio() ? A.extra : nullptr;
}
uint32_t MP4File::audioSkipFrames() const
{
	return hasAudio() && A.media_start > 0 ? (uint32_t)(A.media_start / 1024) : 0;
}

void MP4File::rewindVideo()
{
	if (hasVideo()) {
		mp4_iter_init(&V, &_impl->vit);
		_impl->first = true;
	}
}

void MP4File::rewindAudio()
{
	if (hasAudio()) {
		mp4_iter_init(&A, &_impl->ait);
	}
}

static bool next_packet(const struct mp4_track *t, struct mp4_iter *it, MP4Packet &p)
{
	struct mp4_sample s;
	if (mp4_next(t, it, &s) != 0) {
		return false;
	}
	p.offset = s.offset;
	p.size = s.size;
	p.index = s.index;
	p.key = s.sync != 0;
	p.ptsUs = (s.pts - t->media_start) * 1000000LL / t->timescale;
	p.dtsUs = (s.dts - t->media_start) * 1000000LL / t->timescale;
	return true;
}

bool MP4File::nextVideo(MP4Packet &p) { return hasVideo() && next_packet(&V, &_impl->vit, p); }
bool MP4File::nextAudio(MP4Packet &p) { return hasAudio() && next_packet(&A, &_impl->ait, p); }

bool MP4File::seekVideo(uint32_t index)
{
	if (!hasVideo() || mp4_seek_sync(&V, &_impl->vit, index) != 0) {
		return false;
	}
	_impl->first = true;
	return true;
}

int MP4File::readVideo(const MP4Packet &p, void *buf)
{
	if (!hasVideo()) return -ENODEV;
	return _impl->fromFile ? reader_read(&_impl->vr, p.offset, buf, p.size)
			       : source_read(_impl, p.offset, buf, p.size);
}

int MP4File::readAudio(const MP4Packet &p, void *buf)
{
	if (!hasAudio()) return -ENODEV;
	return _impl->fromFile ? reader_read(&_impl->ar, p.offset, buf, p.size)
			       : source_read(_impl, p.offset, buf, p.size);
}

int MP4File::readVideoAnnexB(const MP4Packet &p, uint8_t *out)
{
	if (!hasVideo() || _impl->headLen == 0) return -ENODEV;
	uint8_t *raw = (uint8_t *)malloc(p.size);
	if (!raw) return -ENOMEM;
	int ret = readVideo(p, raw);
	if (ret != 0) {
		free(raw);
		return ret;
	}
	size_t w = 0;
	if (p.key || _impl->first) {
		memcpy(out, _impl->head, _impl->headLen);
		w = _impl->headLen;
	}
	_impl->first = false;
	const uint8_t *in = raw;
	size_t len = p.size;
	while (len > (size_t)_impl->nalLen) {
		size_t l = 0;
		for (int k = 0; k < _impl->nalLen; k++) {
			l = (l << 8) | *in++;
		}
		len -= _impl->nalLen;
		if (l > len) break;
		out[w++] = 0;
		out[w++] = 0;
		out[w++] = 0;
		out[w++] = 1;
		memcpy(out + w, in, l);
		w += l;
		in += l;
		len -= l;
	}
	free(raw);
	return (int)w;
}
