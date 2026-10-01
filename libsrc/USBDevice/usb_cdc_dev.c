/* SPDX-License-Identifier: Apache-2.0 */
/* CherryUSB first: Zephyr headers define __PACKED differently */
#include "usbd_core.h"
#include "usbd_cdc_acm.h"

#include <zephyr/kernel.h>

#include <string.h>

#include "usb_cdc_dev.h"

extern uintptr_t usb_sunxi_otg_base(void);

#define CDC_IN_EP 0x81
#define CDC_OUT_EP 0x02
#define CDC_INT_EP 0x83

#define USBD_MAX_POWER 100
#define CFG_SIZE (9 + CDC_ACM_DESCRIPTOR_LEN)

#define TX_CHUNK 512
#define RX_BUF 512
#define RX_RING 2048

static uint16_t usb_vid = 0xFFFF, usb_pid = 0xFFFF;
static const char *str_manufacturer = "F101 Arduino";
static const char *str_product = "F101 USB Serial";
static const char *str_serial = "F101-0001";

static uint8_t device_descriptor[18];

static uint8_t config_hs[] = {USB_CONFIG_DESCRIPTOR_INIT(CFG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED,
							 USBD_MAX_POWER),
			      CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP,
						      USB_BULK_EP_MPS_HS, 0x02)};
static uint8_t config_fs[] = {USB_CONFIG_DESCRIPTOR_INIT(CFG_SIZE, 0x02, 0x01, USB_CONFIG_BUS_POWERED,
							 USBD_MAX_POWER),
			      CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP,
						      USB_BULK_EP_MPS_FS, 0x02)};
static uint8_t other_hs[] = {USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(CFG_SIZE, 0x02, 0x01,
								    USB_CONFIG_BUS_POWERED,
								    USBD_MAX_POWER),
			     CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP,
						     USB_BULK_EP_MPS_FS, 0x02)};
static uint8_t other_fs[] = {USB_OTHER_SPEED_CONFIG_DESCRIPTOR_INIT(CFG_SIZE, 0x02, 0x01,
								    USB_CONFIG_BUS_POWERED,
								    USBD_MAX_POWER),
			     CDC_ACM_DESCRIPTOR_INIT(0x00, CDC_INT_EP, CDC_OUT_EP, CDC_IN_EP,
						     USB_BULK_EP_MPS_HS, 0x02)};
static const uint8_t qualifier[] = {USB_DEVICE_QUALIFIER_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, 0x01)};

static const uint8_t *cb_device(uint8_t speed)
{
	(void)speed;
	return device_descriptor;
}

static const uint8_t *cb_config(uint8_t speed)
{
	return speed == USB_SPEED_HIGH ? config_hs : speed == USB_SPEED_FULL ? config_fs : NULL;
}

static const uint8_t *cb_qualifier(uint8_t speed)
{
	(void)speed;
	return qualifier;
}

static const uint8_t *cb_other(uint8_t speed)
{
	return speed == USB_SPEED_HIGH ? other_hs : speed == USB_SPEED_FULL ? other_fs : NULL;
}

static const char *cb_string(uint8_t speed, uint8_t index)
{
	static const char langid[] = {0x09, 0x04};

	(void)speed;
	switch (index) {
	case 0: return langid;
	case 1: return str_manufacturer;
	case 2: return str_product;
	case 3: return str_serial;
	default: return NULL;
	}
}

static struct usb_descriptor descriptors = {
	.device_descriptor_callback = cb_device,
	.config_descriptor_callback = cb_config,
	.device_quality_descriptor_callback = cb_qualifier,
	.other_speed_descriptor_callback = cb_other,
	.string_descriptor_callback = cb_string,
};

static USB_MEM_ALIGNX uint8_t rx_buf[RX_BUF];
static USB_MEM_ALIGNX uint8_t tx_buf[TX_CHUNK];

static uint8_t ring[RX_RING];
static volatile uint16_t ring_head, ring_tail;

static K_SEM_DEFINE(tx_done, 0, 1);
static volatile bool tx_busy;
static volatile bool is_configured, dtr_set, started;
static volatile uint32_t line_baud = 115200;

static void ring_put(const uint8_t *p, uint32_t n)
{
	while (n--) {
		uint16_t next = (ring_head + 1) % RX_RING;
		if (next == ring_tail) {
			return; /* full: drop */
		}
		ring[ring_head] = *p++;
		ring_head = next;
	}
}

static void event_handler(uint8_t busid, uint8_t event)
{
	switch (event) {
	case USBD_EVENT_RESET:
	case USBD_EVENT_DISCONNECTED:
		is_configured = false;
		dtr_set = false;
		if (tx_busy) {
			tx_busy = false;
			k_sem_give(&tx_done);
		}
		break;
	case USBD_EVENT_CONFIGURED:
		is_configured = true;
		tx_busy = false;
		usbd_ep_start_read(busid, CDC_OUT_EP, rx_buf, RX_BUF);
		break;
	default:
		break;
	}
}

void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	(void)ep;
	ring_put(rx_buf, nbytes);
	usbd_ep_start_read(busid, CDC_OUT_EP, rx_buf, RX_BUF);
}

void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
	if (nbytes && (nbytes % usbd_get_ep_mps(busid, ep)) == 0) {
		usbd_ep_start_write(busid, CDC_IN_EP, NULL, 0); /* zero length packet ends the transfer */
	} else {
		tx_busy = false;
		k_sem_give(&tx_done);
	}
}

void usbd_cdc_acm_set_dtr(uint8_t busid, uint8_t intf, bool dtr)
{
	(void)busid;
	(void)intf;
	dtr_set = dtr;
}

void usbd_cdc_acm_set_line_coding(uint8_t busid, uint8_t intf, struct cdc_line_coding *lc)
{
	(void)busid;
	(void)intf;
	line_baud = lc->dwDTERate;
}


static struct usbd_endpoint out_ep = {.ep_addr = CDC_OUT_EP, .ep_cb = usbd_cdc_acm_bulk_out};
static struct usbd_endpoint in_ep = {.ep_addr = CDC_IN_EP, .ep_cb = usbd_cdc_acm_bulk_in};
static struct usbd_interface intf0, intf1;

void usb_cdc_dev_set_id(uint16_t vid, uint16_t pid)
{
	usb_vid = vid;
	usb_pid = pid;
}

void usb_cdc_dev_set_strings(const char *manufacturer, const char *product, const char *serial)
{
	if (manufacturer) str_manufacturer = manufacturer;
	if (product) str_product = product;
	if (serial) str_serial = serial;
}

bool usb_cdc_dev_begin(void)
{
	if (started) {
		return true;
	}
	const uint8_t dd[] = {USB_DEVICE_DESCRIPTOR_INIT(USB_2_0, 0xEF, 0x02, 0x01, (uint16_t)usb_vid,
							 (uint16_t)usb_pid, 0x0100, 0x01)};
	memcpy(device_descriptor, dd, sizeof(dd));

	ring_head = ring_tail = 0;
	usbd_desc_register(0, &descriptors);
	usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf0));
	usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &intf1));
	usbd_add_endpoint(0, &out_ep);
	usbd_add_endpoint(0, &in_ep);
	if (usbd_initialize(0, usb_sunxi_otg_base(), event_handler) != 0) {
		return false;
	}
	started = true;
	return true;
}

void usb_cdc_dev_end(void)
{
	if (started) {
		usbd_deinitialize(0);
		started = false;
		is_configured = dtr_set = false;
	}
}

bool usb_cdc_dev_configured(void) { return started && is_configured; }
bool usb_cdc_dev_connected(void) { return started && is_configured && dtr_set; }
uint32_t usb_cdc_dev_baud(void) { return line_baud; }
int usb_cdc_dev_available(void) { return (RX_RING + ring_head - ring_tail) % RX_RING; }
int usb_cdc_dev_peek(void) { return ring_head == ring_tail ? -1 : ring[ring_tail]; }

int usb_cdc_dev_read(void)
{
	if (ring_head == ring_tail) {
		return -1;
	}
	uint8_t c = ring[ring_tail];
	ring_tail = (ring_tail + 1) % RX_RING;
	return c;
}

void usb_cdc_dev_flush(uint32_t timeout_ms)
{
	if (tx_busy) {
		k_sem_take(&tx_done, K_MSEC(timeout_ms ? timeout_ms : 1));
	}
}

size_t usb_cdc_dev_write(const uint8_t *buf, size_t size, uint32_t timeout_ms)
{
	size_t sent = 0;

	while (sent < size && usb_cdc_dev_connected()) {
		size_t n = size - sent > TX_CHUNK ? TX_CHUNK : size - sent;

		k_sem_reset(&tx_done);
		memcpy(tx_buf, buf + sent, n);
		tx_busy = true;
		if (usbd_ep_start_write(0, CDC_IN_EP, tx_buf, n) != 0) {
			tx_busy = false;
			break;
		}
		if (k_sem_take(&tx_done, K_MSEC(timeout_ms ? timeout_ms : 1)) != 0) {
			tx_busy = false;
			break; /* the PC does not read */
		}
		sent += n;
	}
	return sent;
}
