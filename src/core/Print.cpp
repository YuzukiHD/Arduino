/* SPDX-License-Identifier: Apache-2.0 */
#include <math.h>
#include <stdio.h>
#include "Arduino.h"

size_t Print::write(const uint8_t *buffer, size_t size)
{
	size_t n = 0;
	while (size--) {
		if (write(*buffer++)) n++; else break;
	}
	return n;
}

size_t Print::print(const __FlashStringHelper *f) { return write(reinterpret_cast<const char *>(f)); }
size_t Print::print(const String &s) { return write(s.c_str(), s.length()); }
size_t Print::print(const char str[]) { return write(str); }
size_t Print::print(char c) { return write(c); }
size_t Print::print(unsigned char b, int base) { return print((unsigned long long)b, base); }
size_t Print::print(int n, int base) { return print((long long)n, base); }
size_t Print::print(unsigned int n, int base) { return print((unsigned long long)n, base); }
size_t Print::print(long n, int base) { return print((long long)n, base); }
size_t Print::print(unsigned long n, int base) { return print((unsigned long long)n, base); }
size_t Print::print(long long n, int base)
{
	if (base == 0) return write((uint8_t)n);
	if (base == 10 && n < 0) {
		size_t t = print('-');
		return t + printNumber((unsigned long long)(-n), 10);
	}
	return printNumber((unsigned long long)n, base);
}
size_t Print::print(unsigned long long n, int base)
{
	return base == 0 ? write((uint8_t)n) : printNumber(n, base);
}
size_t Print::print(double n, int digits) { return printFloat(n, digits); }
size_t Print::print(const Printable &x) { return x.printTo(*this); }

size_t Print::println(void) { return write("\r\n"); }
#define PRINTLN(...) size_t n = print(__VA_ARGS__); return n + println()
size_t Print::println(const __FlashStringHelper *f) { PRINTLN(f); }
size_t Print::println(const String &s) { PRINTLN(s); }
size_t Print::println(const char c[]) { PRINTLN(c); }
size_t Print::println(char c) { PRINTLN(c); }
size_t Print::println(unsigned char b, int base) { PRINTLN(b, base); }
size_t Print::println(int num, int base) { PRINTLN(num, base); }
size_t Print::println(unsigned int num, int base) { PRINTLN(num, base); }
size_t Print::println(long num, int base) { PRINTLN(num, base); }
size_t Print::println(unsigned long num, int base) { PRINTLN(num, base); }
size_t Print::println(long long num, int base) { PRINTLN(num, base); }
size_t Print::println(unsigned long long num, int base) { PRINTLN(num, base); }
size_t Print::println(double num, int digits) { PRINTLN(num, digits); }
size_t Print::println(const Printable &x) { PRINTLN(x); }

size_t Print::printNumber(unsigned long long n, uint8_t base)
{
	char buf[8 * sizeof(n) + 1];
	char *str = &buf[sizeof(buf) - 1];
	*str = '\0';
	if (base < 2) base = 10;
	do {
		unsigned c = n % base;
		n /= base;
		*--str = c < 10 ? c + '0' : c + 'A' - 10;
	} while (n);
	return write(str);
}

size_t Print::printFloat(double number, int digits)
{
	size_t n = 0;
	if ((number != number)) return print("nan");
	if ((number > 1.7976931348623157e308 || number < -1.7976931348623157e308)) return print("inf");
	if (number > 4294967040.0 || number < -4294967040.0) return print("ovf");
	if (number < 0.0) {
		n += print('-');
		number = -number;
	}
	double rounding = 0.5;
	for (int i = 0; i < digits; ++i) rounding /= 10.0;
	number += rounding;
	unsigned long int_part = (unsigned long)number;
	double remainder = number - (double)int_part;
	n += print(int_part);
	if (digits > 0) n += print('.');
	while (digits-- > 0) {
		remainder *= 10.0;
		unsigned d = (unsigned)remainder;
		n += print(d);
		remainder -= d;
	}
	return n;
}

size_t Print::printf(const char *format, ...)
{
	char tmp[128];
	va_list ap;
	va_start(ap, format);
	int n = vsnprintf(tmp, sizeof(tmp), format, ap);
	va_end(ap);
	if (n < 0) return 0;
	if ((size_t)n < sizeof(tmp)) return write((const uint8_t *)tmp, n);
	char *big = (char *)malloc(n + 1);
	if (!big) return 0;
	va_start(ap, format);
	vsnprintf(big, n + 1, format, ap);
	va_end(ap);
	size_t r = write((const uint8_t *)big, n);
	free(big);
	return r;
}
