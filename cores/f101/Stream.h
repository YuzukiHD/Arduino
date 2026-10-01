/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Stream_h
#define Stream_h

#include <stdint.h>
#include "Print.h"

class Stream : public Print {
public:
	Stream() {}
	virtual int available() = 0;
	virtual int read() = 0;
	virtual int peek() = 0;

	void setTimeout(unsigned long timeout) { _timeout = timeout; }
	unsigned long getTimeout() const { return _timeout; }

	bool find(const char *target);
	bool findUntil(const char *target, const char *terminator);
	size_t readBytes(char *buffer, size_t length);
	size_t readBytes(uint8_t *buffer, size_t length) { return readBytes((char *)buffer, length); }
	size_t readBytesUntil(char terminator, char *buffer, size_t length);
	String readString();
	String readStringUntil(char terminator);
	long parseInt();
	float parseFloat();

protected:
	unsigned long _timeout = 1000;
	int timedRead();
	int timedPeek();
	int peekNextDigit(bool detectDecimal);
};

#endif
