/* SPDX-License-Identifier: Apache-2.0 */
#include "Arduino.h"

int Stream::timedRead()
{
	unsigned long start = millis();
	do {
		int c = read();
		if (c >= 0) return c;
		yield();
	} while (millis() - start < _timeout);
	return -1;
}

int Stream::timedPeek()
{
	unsigned long start = millis();
	do {
		int c = peek();
		if (c >= 0) return c;
		yield();
	} while (millis() - start < _timeout);
	return -1;
}

int Stream::peekNextDigit(bool detectDecimal)
{
	for (;;) {
		int c = timedPeek();
		if (c < 0) return c;
		if (c == '-' || (c >= '0' && c <= '9') || (detectDecimal && c == '.')) return c;
		read();
	}
}

bool Stream::find(const char *target) { return findUntil(target, nullptr); }
bool Stream::findUntil(const char *target, const char *terminator)
{
	size_t tlen = strlen(target), xlen = terminator ? strlen(terminator) : 0;
	size_t ti = 0, xi = 0;
	if (!tlen) return true;
	int c;
	while ((c = timedRead()) >= 0) {
		ti = (c == target[ti]) ? ti + 1 : (c == target[0] ? 1 : 0);
		if (ti == tlen) return true;
		if (xlen) {
			xi = (c == terminator[xi]) ? xi + 1 : (c == terminator[0] ? 1 : 0);
			if (xi == xlen) return false;
		}
	}
	return false;
}

size_t Stream::readBytes(char *buffer, size_t length)
{
	size_t n = 0;
	while (n < length) {
		int c = timedRead();
		if (c < 0) break;
		*buffer++ = (char)c;
		n++;
	}
	return n;
}

size_t Stream::readBytesUntil(char terminator, char *buffer, size_t length)
{
	size_t n = 0;
	while (n < length) {
		int c = timedRead();
		if (c < 0 || c == terminator) break;
		*buffer++ = (char)c;
		n++;
	}
	return n;
}

String Stream::readString()
{
	String ret;
	int c;
	while ((c = timedRead()) >= 0) ret += (char)c;
	return ret;
}

String Stream::readStringUntil(char terminator)
{
	String ret;
	int c;
	while ((c = timedRead()) >= 0 && c != terminator) ret += (char)c;
	return ret;
}

long Stream::parseInt()
{
	bool neg = false;
	long v = 0;
	int c = peekNextDigit(false);
	if (c < 0) return 0;
	do {
		if (c == '-') neg = true;
		else if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
		read();
		c = timedPeek();
	} while ((c >= '0' && c <= '9') || c == ',');
	return neg ? -v : v;
}

float Stream::parseFloat()
{
	bool neg = false, frac = false;
	double v = 0, scale = 1;
	int c = peekNextDigit(true);
	if (c < 0) return 0;
	do {
		if (c == '-') neg = true;
		else if (c == '.') frac = true;
		else if (c >= '0' && c <= '9') {
			v = v * 10 + (c - '0');
			if (frac) scale *= 10;
		}
		read();
		c = timedPeek();
	} while ((c >= '0' && c <= '9') || (c == '.' && !frac));
	v /= scale;
	return (float)(neg ? -v : v);
}
