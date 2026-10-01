/* SPDX-License-Identifier: Apache-2.0 */
#ifndef Print_h
#define Print_h

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "WString.h"

#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

class Print;
class Printable {
public:
	virtual ~Printable() {}
	virtual size_t printTo(Print &p) const = 0;
};

class Print {
public:
	virtual ~Print() {}
	virtual size_t write(uint8_t) = 0;
	virtual size_t write(const uint8_t *buffer, size_t size);
	size_t write(const char *str) { return str ? write((const uint8_t *)str, strlen(str)) : 0; }
	size_t write(const char *buffer, size_t size) { return write((const uint8_t *)buffer, size); }
	virtual int availableForWrite() { return 0; }
	virtual void flush() {}

	int getWriteError() const { return write_error; }
	void clearWriteError() { write_error = 0; }

	size_t print(const __FlashStringHelper *);
	size_t print(const String &);
	size_t print(const char[]);
	size_t print(char);
	size_t print(unsigned char, int = DEC);
	size_t print(int, int = DEC);
	size_t print(unsigned int, int = DEC);
	size_t print(long, int = DEC);
	size_t print(unsigned long, int = DEC);
	size_t print(long long, int = DEC);
	size_t print(unsigned long long, int = DEC);
	size_t print(double, int = 2);
	size_t print(const Printable &);

	size_t println(const __FlashStringHelper *);
	size_t println(const String &s);
	size_t println(const char[]);
	size_t println(char);
	size_t println(unsigned char, int = DEC);
	size_t println(int, int = DEC);
	size_t println(unsigned int, int = DEC);
	size_t println(long, int = DEC);
	size_t println(unsigned long, int = DEC);
	size_t println(long long, int = DEC);
	size_t println(unsigned long long, int = DEC);
	size_t println(double, int = 2);
	size_t println(const Printable &);
	size_t println(void);

	size_t printf(const char *format, ...) __attribute__((format(printf, 2, 3)));

protected:
	void setWriteError(int err = 1) { write_error = err; }

private:
	int write_error = 0;
	size_t printNumber(unsigned long long, uint8_t);
	size_t printFloat(double, int);
};

#endif
