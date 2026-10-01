/* SPDX-License-Identifier: Apache-2.0 */
/* C side of USBSerial: the CherryUSB headers are C only, they stay in usb_cdc_dev.c */
#ifndef USB_CDC_DEV_H
#define USB_CDC_DEV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_cdc_dev_set_id(uint16_t vid, uint16_t pid);
void usb_cdc_dev_set_strings(const char *manufacturer, const char *product, const char *serial);
bool usb_cdc_dev_begin(void);
void usb_cdc_dev_end(void);
bool usb_cdc_dev_configured(void);
bool usb_cdc_dev_connected(void);
uint32_t usb_cdc_dev_baud(void);
int usb_cdc_dev_available(void);
int usb_cdc_dev_peek(void);
int usb_cdc_dev_read(void);
void usb_cdc_dev_flush(uint32_t timeout_ms);
size_t usb_cdc_dev_write(const uint8_t *buf, size_t size, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif
