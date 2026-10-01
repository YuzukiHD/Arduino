/* SPDX-License-Identifier: Apache-2.0 */
#include <errno.h>
#include <string.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/kernel.h>

#include "Arduino.h"
#include "AudioEngine.h"

#define BLOCK_MS 20
#define TX_BLOCKS 6
#define RX_BLOCKS 8
#define STACK_SIZE 4096
#define PRIO K_PRIO_COOP(2)

struct Ring {
	uint8_t *buf = nullptr;
	size_t size = 0, head = 0, tail = 0, count = 0;
};

struct AudioEngineImpl {
	const struct device *dev = nullptr;
	uint32_t rate = 0;
	uint8_t bits = 16, width = 2, txCh = 0, rxCh = 0;
	bool open = false, run = false;
	bool loopback = false;
	uint8_t fmt = 0;

	struct k_mem_slab txSlab, rxSlab;
	void *txMem = nullptr, *rxMem = nullptr;
	size_t txBlock = 0, rxBlock = 0;
	Ring txRing, rxRing;
	struct k_mutex lock;

	struct k_thread txThread, rxThread;
	void *txStack = nullptr, *rxStack = nullptr;
	bool txStarted = false, rxStarted = false;

	volatile int32_t peak = 0;
	volatile uint32_t txBlocks = 0, overruns = 0, txRec = 0, rxRec = 0;
};

static size_t ring_push(Ring &r, const uint8_t *src, size_t n)
{
	size_t room = r.size - r.count;
	if (n > room) n = room;
	size_t first = r.size - r.head;
	if (first > n) first = n;
	memcpy(r.buf + r.head, src, first);
	memcpy(r.buf, src + first, n - first);
	r.head = (r.head + n) % r.size;
	r.count += n;
	return n;
}

static size_t ring_peek(const Ring &r, uint8_t *dst, size_t n)
{
	if (n > r.count) n = r.count;
	size_t first = r.size - r.tail;
	if (first > n) first = n;
	memcpy(dst, r.buf + r.tail, first);
	memcpy(dst + first, r.buf, n - first);
	return n;
}

static size_t ring_pop(Ring &r, uint8_t *dst, size_t n)
{
	n = ring_peek(r, dst, n);
	r.tail = (r.tail + n) % r.size;
	r.count -= n;
	return n;
}

static int32_t block_peak(const uint8_t *p, size_t bytes, uint8_t width)
{
	int32_t peak = 0;
	if (width == 2) {
		const int16_t *s = (const int16_t *)p;
		for (size_t i = 0; i < bytes / 2; i++) {
			int32_t v = s[i] < 0 ? -(int32_t)s[i] : s[i];
			if (v > peak) peak = v;
		}
	} else {
		const int32_t *s = (const int32_t *)p;
		for (size_t i = 0; i < bytes / 4; i++) {
			int32_t v = s[i] >> 16;
			v = v < 0 ? -v : v;
			if (v > peak) peak = v;
		}
	}
	return peak > 32767 ? 32767 : peak;
}

static struct i2s_config make_cfg(AudioEngineImpl *d, uint8_t ch, struct k_mem_slab *slab,
				  size_t block)
{
	struct i2s_config c = {};
	c.word_size = d->bits;
	c.channels = ch;
	switch (d->fmt) {
	case 1: c.format = I2S_FMT_DATA_FORMAT_LEFT_JUSTIFIED; break;
	case 2: c.format = I2S_FMT_DATA_FORMAT_RIGHT_JUSTIFIED; break;
	default: c.format = I2S_FMT_DATA_FORMAT_I2S; break;
	}
	c.options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER;
	if (d->loopback) c.options |= I2S_OPT_LOOPBACK;
	c.frame_clk_freq = d->rate;
	c.mem_slab = slab;
	c.block_size = block;
	c.timeout = 1000;
	return c;
}

/* one block of silence, so that the stream is running before the thread takes over */
static int tx_put(AudioEngineImpl *d, bool fromRing, k_timeout_t wait)
{
	void *b;
	if (k_mem_slab_alloc(&d->txSlab, &b, wait) != 0) {
		return -ENOMEM;
	}
	memset(b, 0, d->txBlock);
	if (fromRing) {
		k_mutex_lock(&d->lock, K_FOREVER);
		ring_pop(d->txRing, (uint8_t *)b, d->txBlock);
		k_mutex_unlock(&d->lock);
	}
	int r = i2s_write(d->dev, b, d->txBlock);
	if (r != 0) {
		k_mem_slab_free(&d->txSlab, b);
	}
	return r;
}

static int tx_restart(AudioEngineImpl *d)
{
	i2s_trigger(d->dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
	for (int i = 0; i < 3; i++) {
		if (tx_put(d, false, K_MSEC(50)) != 0) return -EIO;
	}
	return i2s_trigger(d->dev, I2S_DIR_TX, I2S_TRIGGER_START);
}

static void tx_entry(void *p1, void *, void *)
{
	AudioEngineImpl *d = (AudioEngineImpl *)p1;
	while (d->run) {
		int r = tx_put(d, true, K_MSEC(100));
		if (r == -ENOMEM) {
			continue; /* all blocks queued, wait for the driver to play one */
		}
		if (r != 0) {
			d->txRec++;
			if (tx_restart(d) != 0) {
				k_msleep(20);
			}
			continue;
		}
		d->txBlocks++;
	}
}

static void rx_entry(void *p1, void *, void *)
{
	AudioEngineImpl *d = (AudioEngineImpl *)p1;
	while (d->run) {
		void *b;
		size_t size;
		int r = i2s_read(d->dev, &b, &size);
		if (r == -EAGAIN) {
			continue;
		}
		if (r != 0) {
			d->rxRec++;
			i2s_trigger(d->dev, I2S_DIR_RX, I2S_TRIGGER_DROP);
			k_msleep(10);
			if (d->run) {
				i2s_trigger(d->dev, I2S_DIR_RX, I2S_TRIGGER_START);
			}
			continue;
		}
		d->peak = block_peak((const uint8_t *)b, size, d->width);
		k_mutex_lock(&d->lock, K_FOREVER);
		size_t room = d->rxRing.size - d->rxRing.count;
		if (size > room) {
			/* the sketch is slow: drop the oldest data */
			size_t drop = size - room;
			d->rxRing.tail = (d->rxRing.tail + drop) % d->rxRing.size;
			d->rxRing.count -= drop;
			d->overruns++;
		}
		ring_push(d->rxRing, (const uint8_t *)b, size);
		k_mutex_unlock(&d->lock);
		k_mem_slab_free(&d->rxSlab, b);
	}
}

AudioEngine::AudioEngine() : d(new AudioEngineImpl) {}

AudioEngine::~AudioEngine()
{
	close();
	delete d;
}

int AudioEngine::open(const void *dev, uint32_t rate, uint8_t bits, uint8_t txChannels,
		      uint8_t rxChannels, bool loopback, Format format, size_t ringBytes)
{
	if (d->open) return -EBUSY;
	if (!dev || !device_is_ready((const struct device *)dev)) return -ENODEV;
	if (!rate || (!txChannels && !rxChannels)) return -EINVAL;
	if (bits != 16 && bits != 24 && bits != 32) return -EINVAL;

	d->dev = (const struct device *)dev;
	d->rate = rate;
	d->bits = bits;
	d->width = bits == 16 ? 2 : 4;
	d->txCh = txChannels;
	d->rxCh = rxChannels;
	d->loopback = loopback;
	d->fmt = (uint8_t)format;
	k_mutex_init(&d->lock);

	uint32_t frames = (rate / (1000 / BLOCK_MS)) & ~3U;
	int ret = 0;

	if (txChannels) {
		d->txBlock = frames * txChannels * d->width;
		size_t stride = (d->txBlock + 63) & ~63U;
		d->txMem = k_aligned_alloc(64, stride * TX_BLOCKS);
		d->txRing.buf = (uint8_t *)malloc(ringBytes);
		if (!d->txMem || !d->txRing.buf) { ret = -ENOMEM; goto fail; }
		d->txRing.size = ringBytes - ringBytes % (txChannels * d->width);
		k_mem_slab_init(&d->txSlab, d->txMem, stride, TX_BLOCKS);
		struct i2s_config c = make_cfg(d, txChannels, &d->txSlab, d->txBlock);
		ret = i2s_configure(d->dev, I2S_DIR_TX, &c);
		if (ret) goto fail;
	}
	if (rxChannels) {
		d->rxBlock = frames * rxChannels * d->width;
		size_t stride = (d->rxBlock + 63) & ~63U;
		d->rxMem = k_aligned_alloc(64, stride * RX_BLOCKS);
		d->rxRing.buf = (uint8_t *)malloc(ringBytes);
		if (!d->rxMem || !d->rxRing.buf) { ret = -ENOMEM; goto fail; }
		d->rxRing.size = ringBytes - ringBytes % (rxChannels * d->width);
		k_mem_slab_init(&d->rxSlab, d->rxMem, stride, RX_BLOCKS);
		struct i2s_config c = make_cfg(d, rxChannels, &d->rxSlab, d->rxBlock);
		ret = i2s_configure(d->dev, I2S_DIR_RX, &c);
		if (ret) goto fail;
	}

	d->run = true;
	d->open = true;
	if (rxChannels) {
		/* start the receiver first: in loopback it has to see the first frame */
		ret = i2s_trigger(d->dev, I2S_DIR_RX, I2S_TRIGGER_START);
		if (ret) goto fail_started;
		d->rxStack = k_aligned_alloc(16, STACK_SIZE);
		if (!d->rxStack) { ret = -ENOMEM; goto fail_started; }
		k_thread_create(&d->rxThread, (k_thread_stack_t *)d->rxStack, STACK_SIZE, rx_entry, d,
				NULL, NULL, PRIO, 0, K_NO_WAIT);
		d->rxStarted = true;
	}
	if (txChannels) {
		ret = tx_restart(d);
		if (ret) goto fail_started;
		d->txStack = k_aligned_alloc(16, STACK_SIZE);
		if (!d->txStack) { ret = -ENOMEM; goto fail_started; }
		k_thread_create(&d->txThread, (k_thread_stack_t *)d->txStack, STACK_SIZE, tx_entry, d,
				NULL, NULL, PRIO, 0, K_NO_WAIT);
		d->txStarted = true;
	}
	return 0;

fail_started:
	close();
	return ret;
fail:
	{
		struct i2s_config off = {};
		if (txChannels) i2s_configure(d->dev, I2S_DIR_TX, &off);
		if (rxChannels) i2s_configure(d->dev, I2S_DIR_RX, &off);
	}
	k_free(d->txMem); free(d->txRing.buf); k_free(d->rxMem); free(d->rxRing.buf);
	d->txMem = d->rxMem = nullptr;
	d->txRing.buf = d->rxRing.buf = nullptr;
	d->txCh = d->rxCh = 0;
	return ret;
}

void AudioEngine::close()
{
	if (!d->open) return;
	d->run = false;
	if (d->txStarted) { k_thread_join(&d->txThread, K_SECONDS(2)); d->txStarted = false; }
	if (d->rxStarted) { k_thread_join(&d->rxThread, K_SECONDS(2)); d->rxStarted = false; }
	struct i2s_config off = {};
	if (d->txCh) {
		i2s_trigger(d->dev, I2S_DIR_TX, I2S_TRIGGER_DROP);
		i2s_configure(d->dev, I2S_DIR_TX, &off);
	}
	if (d->rxCh) {
		i2s_trigger(d->dev, I2S_DIR_RX, I2S_TRIGGER_DROP);
		i2s_configure(d->dev, I2S_DIR_RX, &off);
	}
	k_free(d->txStack); k_free(d->rxStack);
	k_free(d->txMem); k_free(d->rxMem);
	free(d->txRing.buf); free(d->rxRing.buf);
	d->txStack = d->rxStack = d->txMem = d->rxMem = nullptr;
	d->txRing = Ring();
	d->rxRing = Ring();
	d->txCh = d->rxCh = 0;
	d->open = false;
}

bool AudioEngine::isOpen() const { return d->open; }
uint32_t AudioEngine::rate() const { return d->rate; }
uint8_t AudioEngine::sampleBytes() const { return d->width; }
uint8_t AudioEngine::txFrameBytes() const { return d->txCh * d->width; }
uint8_t AudioEngine::rxFrameBytes() const { return d->rxCh * d->width; }

size_t AudioEngine::write(const void *data, size_t bytes, bool block)
{
	if (!d->open || !d->txCh) return 0;
	size_t fb = txFrameBytes();
	bytes -= bytes % fb;
	const uint8_t *p = (const uint8_t *)data;
	size_t done = 0;
	while (done < bytes) {
		k_mutex_lock(&d->lock, K_FOREVER);
		size_t room = (d->txRing.size - d->txRing.count);
		room -= room % fb;
		size_t n = bytes - done < room ? bytes - done : room;
		ring_push(d->txRing, p + done, n);
		k_mutex_unlock(&d->lock);
		done += n;
		if (done < bytes) {
			if (!block || !d->run) break;
			k_msleep(2);
		}
	}
	return done;
}

size_t AudioEngine::writeSpace() const
{
	if (!d->open || !d->txCh) return 0;
	return d->txRing.size - d->txRing.count;
}

void AudioEngine::flush()
{
	if (!d->open || !d->txCh) return;
	while (d->run && d->txRing.count) {
		k_msleep(5);
	}
	/* the blocks already queued in the driver */
	k_msleep(BLOCK_MS * (TX_BLOCKS + 2));
}

size_t AudioEngine::read(void *dst, size_t bytes, uint32_t timeoutMs)
{
	if (!d->open || !d->rxCh) return 0;
	uint8_t *p = (uint8_t *)dst;
	size_t done = 0;
	int64_t end = k_uptime_get() + timeoutMs;
	while (done < bytes) {
		k_mutex_lock(&d->lock, K_FOREVER);
		size_t n = ring_pop(d->rxRing, p + done, bytes - done);
		k_mutex_unlock(&d->lock);
		done += n;
		if (done >= bytes || k_uptime_get() >= end || !d->run) break;
		k_msleep(2);
	}
	return done;
}

size_t AudioEngine::readAvailable() const
{
	return d->open ? d->rxRing.count : 0;
}

size_t AudioEngine::peek(void *dst, size_t bytes) const
{
	if (!d->open || !d->rxCh) return 0;
	k_mutex_lock(&d->lock, K_FOREVER);
	size_t n = ring_peek(d->rxRing, (uint8_t *)dst, bytes);
	k_mutex_unlock(&d->lock);
	return n;
}

void AudioEngine::dropInput()
{
	if (!d->open) return;
	k_mutex_lock(&d->lock, K_FOREVER);
	d->rxRing.tail = d->rxRing.head;
	d->rxRing.count = 0;
	k_mutex_unlock(&d->lock);
}

int32_t AudioEngine::rxPeak() const { return d->peak; }
uint32_t AudioEngine::txBlocksPlayed() const { return d->txBlocks; }
uint32_t AudioEngine::rxOverruns() const { return d->overruns; }
uint32_t AudioEngine::txRecoveries() const { return d->txRec; }
uint32_t AudioEngine::rxRecoveries() const { return d->rxRec; }
