/* SPDX-License-Identifier: Apache-2.0 */
/* C side of the USB host classes: the CherryUSB headers are C only, they stay in usb_host.c */
#ifndef USB_HOST_H
#define USB_HOST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct usb_host_info {
	uint16_t vid, pid;
	uint8_t dev_class;
	uint8_t speed; /* 1 low, 2 full, 3 high */
	uint8_t address;
};

bool usb_host_begin(void);
void usb_host_end(void);
bool usb_host_port_connected(void);
bool usb_host_port_info(struct usb_host_info *info);

/* mass storage (first drive, /dev/sda) */
bool usb_host_msc_present(void);
bool usb_host_msc_init(void); /* SCSI init, capacity */
bool usb_host_msc_info(struct usb_host_info *info);
uint32_t usb_host_msc_blocks(void);
uint32_t usb_host_msc_blocksize(void);
int usb_host_msc_read(uint32_t lba, uint8_t *buf, uint32_t count);
int usb_host_msc_write(uint32_t lba, const uint8_t *buf, uint32_t count);
/* makes the drive available as disk "USB" for the FAT file system */
bool usb_host_msc_register_disk(void);

/* serial (first /dev/ttyACMx or /dev/ttyUSBx) */
bool usb_host_serial_present(void);
bool usb_host_serial_open(uint32_t baud, uint8_t databits, uint8_t parity, uint8_t stopbits);
void usb_host_serial_close(void);
bool usb_host_serial_is_open(void);
int usb_host_serial_read(uint8_t *buf, size_t size);
int usb_host_serial_write(const uint8_t *buf, size_t size);
bool usb_host_serial_info(struct usb_host_info *info);

#ifdef __cplusplus
}
#endif

#endif
