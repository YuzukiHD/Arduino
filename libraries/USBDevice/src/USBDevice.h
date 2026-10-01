/* SPDX-License-Identifier: Apache-2.0 */
#ifndef USBDevice_h
#define USBDevice_h

#include <stdint.h>
#include "Arduino.h"

/*
 * USB CDC ACM serial port (a COM port on the PC) on the OTG controller:
 *
 *   USBSerial.begin();
 *   while (!USBSerial) delay(10);   // the PC opened the port
 *   USBSerial.println("hello");
 *
 * Independent from Serial (the UART console). The USB port of the EVB is also
 * the download port: flash with the PC cable, then plug the cable again.
 */
class USBSerialClass : public Stream {
public:
	/* Identity shown to the PC; call before begin(). The strings are not copied. */
	void setID(uint16_t vid, uint16_t pid);
	void setStrings(const char *manufacturer, const char *product, const char *serial);

	/* Starts the device controller; returns false when it cannot be started */
	bool begin();
	/* The baud rate argument is ignored (virtual port) */
	bool begin(unsigned long baud) { (void)baud; return begin(); }
	void end();

	/* true when the PC configured the device */
	bool configured();
	/* true when the PC configured the device and opened the port (DTR) */
	bool connected();
	operator bool() { return connected(); }
	/* the baud rate the PC selected on its side */
	uint32_t baud();

	int available() override;
	int read() override;
	int peek() override;
	int availableForWrite() override { return 512; }
	void flush() override;
	size_t write(uint8_t c) override;
	size_t write(const uint8_t *buf, size_t size) override;
	using Print::write;

	/* writes wait up to this long (ms) for the PC to take the data, 0 = drop at once */
	void setWriteTimeout(uint32_t ms) { _writeTimeout = ms; }

private:
	uint32_t _writeTimeout = 200;
};

extern USBSerialClass USBSerial;

#endif
