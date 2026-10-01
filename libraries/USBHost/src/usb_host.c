/* SPDX-License-Identifier: Apache-2.0 */
/* CherryUSB first: Zephyr headers define __PACKED differently */
#include "usbh_core.h"
#include "usbh_msc.h"
#include "usbh_serial.h"

#include <string.h>
#include <zephyr/drivers/disk.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

#include "usb_host.h"

extern uintptr_t usb_sunxi_ehci_base(void);

static bool host_started;

bool usb_host_begin(void)
{
	if (host_started) {
		return true;
	}
	if (usbh_initialize(0, usb_sunxi_ehci_base(), NULL) != 0) {
		return false;
	}
	host_started = true;
	return true;
}

void usb_host_end(void)
{
	if (host_started) {
		usb_host_serial_close();
		usbh_deinitialize(0);
		host_started = false;
	}
}

static void fill_info(struct usb_host_info *info, struct usbh_hubport *hp)
{
	info->vid = hp->device_desc.idVendor;
	info->pid = hp->device_desc.idProduct;
	info->dev_class = hp->device_desc.bDeviceClass;
	info->speed = hp->speed;
	info->address = hp->dev_addr;
}

bool usb_host_port_connected(void)
{
	struct usbh_hubport *hp = host_started ? usbh_find_hubport(0, 1, 1) : NULL;

	return hp != NULL && hp->connected;
}

bool usb_host_port_info(struct usb_host_info *info)
{
	struct usbh_hubport *hp = host_started ? usbh_find_hubport(0, 1, 1) : NULL;

	if (hp == NULL || !hp->connected) {
		return false;
	}
	fill_info(info, hp);
	return true;
}

/* ---- mass storage ---------------------------------------------------------- */

#define MSC_CHUNK 8 /* sectors per transfer */

static USB_MEM_ALIGNX uint8_t msc_bounce[512 * MSC_CHUNK];
static struct usbh_msc *msc_cur;
static bool msc_ready;
static K_MUTEX_DEFINE(msc_lock);

static struct usbh_msc *msc_find(void)
{
	return host_started ? usbh_find_class_instance("/dev/sda") : NULL;
}

bool usb_host_msc_present(void)
{
	return msc_find() != NULL;
}

bool usb_host_msc_init(void)
{
	struct usbh_msc *m = msc_find();

	if (m == NULL) {
		msc_ready = false;
		return false;
	}
	if (m == msc_cur && msc_ready) {
		return true;
	}
	k_mutex_lock(&msc_lock, K_FOREVER);
	msc_ready = usbh_msc_scsi_init(m) == 0 && m->blocksize == 512;
	msc_cur = m;
	k_mutex_unlock(&msc_lock);
	return msc_ready;
}

bool usb_host_msc_info(struct usb_host_info *info)
{
	struct usbh_msc *m = msc_find();

	if (m == NULL) {
		return false;
	}
	fill_info(info, m->hport);
	return true;
}

uint32_t usb_host_msc_blocks(void)
{
	return msc_ready && msc_cur ? msc_cur->blocknum : 0;
}

uint32_t usb_host_msc_blocksize(void)
{
	return msc_ready && msc_cur ? msc_cur->blocksize : 0;
}

int usb_host_msc_read(uint32_t lba, uint8_t *buf, uint32_t count)
{
	struct usbh_msc *m = msc_find();
	int ret = 0;

	if (m == NULL || m != msc_cur || !msc_ready) {
		return -1;
	}
	k_mutex_lock(&msc_lock, K_FOREVER);
	while (count && ret == 0) {
		uint32_t n = count > MSC_CHUNK ? MSC_CHUNK : count;

		ret = usbh_msc_scsi_read10(m, lba, msc_bounce, n);
		if (ret == 0) {
			memcpy(buf, msc_bounce, n * 512);
			buf += n * 512;
			lba += n;
			count -= n;
		}
	}
	k_mutex_unlock(&msc_lock);
	return ret;
}

int usb_host_msc_write(uint32_t lba, const uint8_t *buf, uint32_t count)
{
	struct usbh_msc *m = msc_find();
	int ret = 0;

	if (m == NULL || m != msc_cur || !msc_ready) {
		return -1;
	}
	k_mutex_lock(&msc_lock, K_FOREVER);
	while (count && ret == 0) {
		uint32_t n = count > MSC_CHUNK ? MSC_CHUNK : count;

		memcpy(msc_bounce, buf, n * 512);
		ret = usbh_msc_scsi_write10(m, lba, msc_bounce, n);
		if (ret == 0) {
			buf += n * 512;
			lba += n;
			count -= n;
		}
	}
	k_mutex_unlock(&msc_lock);
	return ret;
}

static int disk_init(struct disk_info *d)
{
	(void)d;
	return usb_host_msc_init() ? 0 : -1;
}

static int disk_status(struct disk_info *d)
{
	(void)d;
	return usb_host_msc_present() && msc_ready ? DISK_STATUS_OK : DISK_STATUS_NOMEDIA;
}

static int disk_read(struct disk_info *d, uint8_t *buf, uint32_t sector, uint32_t count)
{
	(void)d;
	return usb_host_msc_read(sector, buf, count);
}

static int disk_write(struct disk_info *d, const uint8_t *buf, uint32_t sector, uint32_t count)
{
	(void)d;
	return usb_host_msc_write(sector, buf, count);
}

static int disk_ioctl(struct disk_info *d, uint8_t cmd, void *buf)
{
	(void)d;
	switch (cmd) {
	case DISK_IOCTL_CTRL_SYNC:
		return 0;
	case DISK_IOCTL_GET_SECTOR_COUNT:
		*(uint32_t *)buf = usb_host_msc_blocks();
		return 0;
	case DISK_IOCTL_GET_SECTOR_SIZE:
		*(uint32_t *)buf = usb_host_msc_blocksize();
		return 0;
	case DISK_IOCTL_GET_ERASE_BLOCK_SZ:
		*(uint32_t *)buf = 1;
		return 0;
	default:
		return -1;
	}
}

static const struct disk_operations usb_disk_ops = {
	.init = disk_init,
	.status = disk_status,
	.read = disk_read,
	.write = disk_write,
	.ioctl = disk_ioctl,
};

static struct disk_info usb_disk = {
	.name = "USB",
	.ops = &usb_disk_ops,
};

bool usb_host_msc_register_disk(void)
{
	static bool registered;

	if (!registered) {
		if (disk_access_register(&usb_disk) != 0) {
			return false;
		}
		registered = true;
	}
	return true;
}

/* ---- serial ---------------------------------------------------------------- */

#define SERIAL_CHUNK 512

static USB_MEM_ALIGNX uint8_t serial_tx[SERIAL_CHUNK];
static USB_MEM_ALIGNX uint8_t serial_rx[SERIAL_CHUNK];
static struct usbh_serial *serial_cur;

static const char *serial_name(void)
{
	static const char *const names[] = {"/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2",
					    "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2"};

	if (!host_started) {
		return NULL;
	}
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		if (usbh_find_class_instance(names[i]) != NULL) {
			return names[i];
		}
	}
	return NULL;
}

bool usb_host_serial_present(void)
{
	return serial_name() != NULL;
}

bool usb_host_serial_info(struct usb_host_info *info)
{
	const char *n = serial_name();

	if (n == NULL) {
		return false;
	}
	struct usbh_serial *s = usbh_find_class_instance(n);

	fill_info(info, s->hport);
	return true;
}

bool usb_host_serial_open(uint32_t baud, uint8_t databits, uint8_t parity, uint8_t stopbits)
{
	const char *n = serial_name();

	if (n == NULL) {
		return false;
	}
	usb_host_serial_close();
	struct usbh_serial *s = usbh_serial_open(n, USBH_SERIAL_O_RDWR | USBH_SERIAL_O_NONBLOCK);

	if (s == NULL) {
		return false;
	}
	struct usbh_serial_termios t = {
		.baudrate = baud,
		.databits = databits,
		.parity = parity,
		.stopbits = stopbits,
		.rtscts = false,
		.rx_timeout = 0,
	};

	if (usbh_serial_control(s, USBH_SERIAL_CMD_SET_ATTR, &t) < 0) {
		usbh_serial_close(s);
		return false;
	}
	serial_cur = s;
	return true;
}

void usb_host_serial_close(void)
{
	if (serial_cur != NULL) {
		usbh_serial_close(serial_cur);
		serial_cur = NULL;
	}
}

bool usb_host_serial_is_open(void)
{
	return serial_cur != NULL && serial_name() != NULL;
}

int usb_host_serial_read(uint8_t *buf, size_t size)
{
	if (!usb_host_serial_is_open()) {
		return -1;
	}
	if (size > SERIAL_CHUNK) {
		size = SERIAL_CHUNK;
	}
	int r = usbh_serial_read(serial_cur, serial_rx, size);

	if (r > 0) {
		memcpy(buf, serial_rx, r);
		return r;
	}
	return r < 0 ? 0 : 0;
}

int usb_host_serial_write(const uint8_t *buf, size_t size)
{
	size_t sent = 0;

	if (!usb_host_serial_is_open()) {
		return 0;
	}
	while (sent < size) {
		size_t n = size - sent > SERIAL_CHUNK ? SERIAL_CHUNK : size - sent;

		memcpy(serial_tx, buf + sent, n);
		int r = usbh_serial_write(serial_cur, serial_tx, n);

		if (r <= 0) {
			break;
		}
		sent += r;
	}
	return sent;
}
