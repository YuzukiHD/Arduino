/* SPDX-License-Identifier: Apache-2.0 */
#include "Arduino.h"
#include "SD.h"
#include "USBHost.h"
#include "usb_host.h"

USBHostClass USBHost;
USBHostMSCClass USBHostMSC;
USBHostSerialClass USBHostSerial;

static bool copy_info(bool ok, const usb_host_info &i, USBHostDeviceInfo &out)
{
	if (ok) {
		out.vid = i.vid;
		out.pid = i.pid;
		out.deviceClass = i.dev_class;
		out.speed = i.speed;
		out.address = i.address;
	}
	return ok;
}

bool USBHostClass::begin() { return usb_host_begin(); }
void USBHostClass::end() { usb_host_end(); }
bool USBHostClass::connected() { return usb_host_port_connected(); }
bool USBHostClass::info(USBHostDeviceInfo &out)
{
	usb_host_info i;
	return copy_info(usb_host_port_info(&i), i, out);
}

bool USBHostMSCClass::begin(uint32_t timeoutMs)
{
	uint32_t t0 = millis();
	do {
		if (usb_host_msc_present() && usb_host_msc_init()) {
			return true;
		}
		delay(50);
	} while (millis() - t0 < timeoutMs);
	return false;
}

bool USBHostMSCClass::present() { return usb_host_msc_present(); }
uint32_t USBHostMSCClass::blockCount() { return usb_host_msc_blocks(); }
uint32_t USBHostMSCClass::blockSize() { return usb_host_msc_blocksize(); }
bool USBHostMSCClass::readBlocks(uint32_t lba, void *buf, uint32_t count)
{
	return usb_host_msc_read(lba, (uint8_t *)buf, count) == 0;
}
bool USBHostMSCClass::writeBlocks(uint32_t lba, const void *buf, uint32_t count)
{
	return usb_host_msc_write(lba, (const uint8_t *)buf, count) == 0;
}
bool USBHostMSCClass::info(USBHostDeviceInfo &out)
{
	usb_host_info i;
	return copy_info(usb_host_msc_info(&i), i, out);
}

bool USBHostMSCClass::mount()
{
	return usb_host_msc_init() && usb_host_msc_register_disk() && USBDrive.begin();
}

void USBHostMSCClass::unmount() { USBDrive.end(); }

bool USBHostSerialClass::begin(unsigned long baud, uint16_t config)
{
	uint8_t bits = 5 + ((config >> 1) & 3);
	uint8_t parity = (config & 0x30) == 0x20 ? 2 : (config & 0x30) == 0x30 ? 1 : 0;
	uint8_t stop = (config & 0x08) ? 2 : 0;
	_head = _tail = 0;
	return usb_host_serial_open(baud, bits < 6 ? 8 : bits, parity, stop);
}

void USBHostSerialClass::end() { usb_host_serial_close(); }
bool USBHostSerialClass::present() { return usb_host_serial_present(); }
bool USBHostSerialClass::info(USBHostDeviceInfo &out)
{
	usb_host_info i;
	return copy_info(usb_host_serial_info(&i), i, out);
}

bool USBHostSerialClass::fill()
{
	if (_head != _tail) {
		return true;
	}
	int n = usb_host_serial_read(_buf, sizeof(_buf));
	if (n <= 0) {
		return false;
	}
	_head = 0;
	_tail = n % 256; /* n <= 128 */
	return true;
}

int USBHostSerialClass::available() { return fill() ? _tail - _head : 0; }
int USBHostSerialClass::peek() { return fill() ? _buf[_head] : -1; }
int USBHostSerialClass::read() { return fill() ? _buf[_head++] : -1; }
size_t USBHostSerialClass::write(const uint8_t *buf, size_t size)
{
	return usb_host_serial_write(buf, size);
}
