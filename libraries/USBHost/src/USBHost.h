/* SPDX-License-Identifier: Apache-2.0 */
#ifndef USBHost_h
#define USBHost_h

#include <stdint.h>
#include "Arduino.h"
#include "HardwareSerial.h"

/* identification of an attached device */
struct USBHostDeviceInfo {
	uint16_t vid = 0, pid = 0;
	uint8_t deviceClass = 0;
	uint8_t speed = 0; /* 1 low, 2 full, 3 high */
	uint8_t address = 0;
	const char *speedName() const { return speed == 3 ? "high" : speed == 2 ? "full" : speed == 1 ? "low" : "?"; }
};

/*
 * USB host on the OTG connector (Type-A on the EVB, VBUS switched by PE2).
 * One role at a time: do not use USBDevice in the same sketch.
 */
class USBHostClass {
public:
	/* starts the host controllers; devices are enumerated in the background */
	bool begin();
	void end();
	/* something is plugged into the root port and enumerated */
	bool connected();
	bool info(USBHostDeviceInfo &out);
};

/* USB mass storage drive (flash stick, card reader) */
class USBHostMSCClass {
public:
	/* true when a drive is present and ready; waits up to timeoutMs for it */
	bool begin(uint32_t timeoutMs = 0);
	bool present();
	uint32_t blockCount();
	uint32_t blockSize();
	uint64_t capacity() { return (uint64_t)blockCount() * blockSize(); }
	/* raw 512 byte block access */
	bool readBlocks(uint32_t lba, void *buf, uint32_t count);
	bool writeBlocks(uint32_t lba, const void *buf, uint32_t count);
	bool info(USBHostDeviceInfo &out);
	/* mounts the FAT volume of the drive: afterwards use USBDrive (see SD.h), e.g.
	 * USBDrive.open("/readme.txt") */
	bool mount();
	void unmount();
};

/* CDC ACM device (Arduino, modem, another F101) or USB serial adapter (CH34x), as a Stream */
class USBHostSerialClass : public Stream {
public:
	/* true when a device is present and open; config is SERIAL_8N1 etc. */
	bool begin(unsigned long baud = 115200, uint16_t config = SERIAL_8N1);
	void end();
	bool present();
	bool info(USBHostDeviceInfo &out);

	int available() override;
	int read() override;
	int peek() override;
	void flush() override {}
	size_t write(uint8_t c) override { return write(&c, 1); }
	size_t write(const uint8_t *buf, size_t size) override;
	using Print::write;
	operator bool() { return present(); }

private:
	uint8_t _buf[128];
	uint8_t _head = 0, _tail = 0;
	bool fill();
};

extern USBHostClass USBHost;
extern USBHostMSCClass USBHostMSC;
extern USBHostSerialClass USBHostSerial;

#endif
