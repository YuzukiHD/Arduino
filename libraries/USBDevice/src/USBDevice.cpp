/* SPDX-License-Identifier: Apache-2.0 */
#include "Arduino.h"
#include "USBDevice.h"
#include "usb_cdc_dev.h"

USBSerialClass USBSerial;

void USBSerialClass::setID(uint16_t vid, uint16_t pid) { usb_cdc_dev_set_id(vid, pid); }

void USBSerialClass::setStrings(const char *manufacturer, const char *product, const char *serial)
{
	usb_cdc_dev_set_strings(manufacturer, product, serial);
}

bool USBSerialClass::begin() { return usb_cdc_dev_begin(); }
void USBSerialClass::end() { usb_cdc_dev_end(); }
bool USBSerialClass::configured() { return usb_cdc_dev_configured(); }
bool USBSerialClass::connected() { return usb_cdc_dev_connected(); }
uint32_t USBSerialClass::baud() { return usb_cdc_dev_baud(); }
int USBSerialClass::available() { return usb_cdc_dev_available(); }
int USBSerialClass::peek() { return usb_cdc_dev_peek(); }
int USBSerialClass::read() { return usb_cdc_dev_read(); }
void USBSerialClass::flush() { usb_cdc_dev_flush(_writeTimeout); }
size_t USBSerialClass::write(uint8_t c) { return write(&c, 1); }
size_t USBSerialClass::write(const uint8_t *buf, size_t size)
{
	return usb_cdc_dev_write(buf, size, _writeTimeout);
}
