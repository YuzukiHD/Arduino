/* SPDX-License-Identifier: Apache-2.0 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Arduino.h"

static char *utoa_base(unsigned long long v, char *end, unsigned char base)
{
	*--end = 0;
	if (base < 2) {
		base = 10;
	}
	do {
		unsigned d = v % base;
		*--end = d < 10 ? '0' + d : 'a' + d - 10;
		v /= base;
	} while (v);
	return end;
}

static String fromInt(long long v, unsigned char base)
{
	char tmp[68];
	char *p;
	if (v < 0 && base == 10) {
		p = utoa_base((unsigned long long)(-v), tmp + sizeof(tmp), base);
		*--p = '-';
	} else {
		p = utoa_base((unsigned long long)v, tmp + sizeof(tmp), base);
	}
	return String(p);
}

String::String(const char *cstr) { if (cstr) copy(cstr, strlen(cstr)); }
String::String(const String &s) { copy(s.buf ? s.buf : "", s.len); if (!s.buf) invalidate(); }
String::String(const __FlashStringHelper *s) : String(reinterpret_cast<const char *>(s)) {}
String::String(String &&s) noexcept : buf(s.buf), cap(s.cap), len(s.len)
{
	s.buf = nullptr; s.cap = s.len = 0;
}
String::String(char c) { char t[2] = {c, 0}; copy(t, 1); }
String::String(unsigned char v, unsigned char b) { *this = fromInt(v, b); }
String::String(int v, unsigned char b) { *this = fromInt(v, b); }
String::String(unsigned int v, unsigned char b) { *this = fromInt(v, b); }
String::String(long v, unsigned char b) { *this = fromInt(v, b); }
String::String(unsigned long v, unsigned char b) { *this = fromInt((long long)v, b); }
String::String(long long v, unsigned char b) { *this = fromInt(v, b); }
String::String(unsigned long long v, unsigned char b)
{
	char tmp[68];
	*this = String(utoa_base(v, tmp + sizeof(tmp), b));
}
String::String(float v, unsigned char d) : String((double)v, d) {}
String::String(double v, unsigned char d)
{
	char fmt[8], out[48];
	snprintf(fmt, sizeof(fmt), "%%.%uf", d);
	snprintf(out, sizeof(out), fmt, v);
	copy(out, strlen(out));
}
String::~String() { free(buf); }

void String::invalidate()
{
	free(buf);
	buf = nullptr;
	cap = len = 0;
}

bool String::reserve(unsigned int size)
{
	if (buf && cap >= size) {
		return true;
	}
	return changeBuffer(size);
}

bool String::changeBuffer(unsigned int maxStrLen)
{
	char *nb = (char *)realloc(buf, maxStrLen + 1);
	if (!nb) {
		return false;
	}
	if (!buf) {
		nb[0] = 0;
	}
	buf = nb;
	cap = maxStrLen;
	return true;
}

String &String::copy(const char *cstr, unsigned int length)
{
	if (!reserve(length)) {
		invalidate();
		return *this;
	}
	len = length;
	memcpy(buf, cstr, length);
	buf[length] = 0;
	return *this;
}

String &String::operator=(const String &rhs)
{
	if (this == &rhs) {
		return *this;
	}
	if (rhs.buf) {
		copy(rhs.buf, rhs.len);
	} else {
		invalidate();
	}
	return *this;
}
String &String::operator=(String &&rhs) noexcept
{
	if (this != &rhs) {
		free(buf);
		buf = rhs.buf; cap = rhs.cap; len = rhs.len;
		rhs.buf = nullptr; rhs.cap = rhs.len = 0;
	}
	return *this;
}
String &String::operator=(const char *cstr)
{
	if (cstr) {
		copy(cstr, strlen(cstr));
	} else {
		invalidate();
	}
	return *this;
}

bool String::concat(const char *cstr, unsigned int length)
{
	if (!cstr) return false;
	if (length == 0) return true;
	unsigned int n = len + length;
	/* cstr may point into our own buffer */
	if (!reserve(n)) return false;
	memmove(buf + len, cstr, length);
	len = n;
	buf[len] = 0;
	return true;
}
bool String::concat(const String &s)
{
	if (this == &s) { String t(s); return concat(t.buf, t.len); }
	return concat(s.buf, s.len);
}
bool String::concat(const char *cstr) { return cstr ? concat(cstr, strlen(cstr)) : false; }
bool String::concat(char c) { return concat(&c, 1); }
bool String::concat(unsigned char n) { return concat(String(n)); }
bool String::concat(int n) { return concat(String(n)); }
bool String::concat(unsigned int n) { return concat(String(n)); }
bool String::concat(long n) { return concat(String(n)); }
bool String::concat(unsigned long n) { return concat(String(n)); }
bool String::concat(float n) { return concat(String(n)); }
bool String::concat(double n) { return concat(String(n)); }

StringSumHelper &operator+(const StringSumHelper &lhs, const String &rhs)
{
	StringSumHelper &a = const_cast<StringSumHelper &>(lhs);
	if (!a.concat(rhs)) a = StringSumHelper("");
	return a;
}
#define SUM_IMPL(T) \
	StringSumHelper &operator+(const StringSumHelper &lhs, T v) \
	{ \
		StringSumHelper &a = const_cast<StringSumHelper &>(lhs); \
		a.concat(v); \
		return a; \
	}
SUM_IMPL(const char *)
SUM_IMPL(char)
SUM_IMPL(unsigned char)
SUM_IMPL(int)
SUM_IMPL(unsigned int)
SUM_IMPL(long)
SUM_IMPL(unsigned long)
SUM_IMPL(float)
SUM_IMPL(double)

int String::compareTo(const String &s) const { return strcmp(c_str(), s.c_str()); }
bool String::equals(const String &s) const { return len == s.len && compareTo(s) == 0; }
bool String::equals(const char *cstr) const { return strcmp(c_str(), cstr ? cstr : "") == 0; }
bool String::equalsIgnoreCase(const String &s) const
{
	if (this == &s) return true;
	if (len != s.len) return false;
	for (unsigned i = 0; i < len; i++) {
		if (tolower((unsigned char)buf[i]) != tolower((unsigned char)s.buf[i])) return false;
	}
	return true;
}
bool String::startsWith(const String &p) const { return startsWith(p, 0); }
bool String::startsWith(const String &p, unsigned int off) const
{
	if (off > len || p.len > len - off) return false;
	return memcmp(buf + off, p.c_str(), p.len) == 0;
}
bool String::endsWith(const String &s) const
{
	if (s.len > len) return false;
	return memcmp(c_str() + len - s.len, s.c_str(), s.len) == 0;
}

char String::charAt(unsigned int i) const { return i < len ? buf[i] : 0; }
void String::setCharAt(unsigned int i, char c) { if (i < len) buf[i] = c; }
char &String::operator[](unsigned int i)
{
	static char dummy;
	if (i >= len || !buf) { dummy = 0; return dummy; }
	return buf[i];
}
void String::getBytes(unsigned char *out, unsigned int bufsize, unsigned int index) const
{
	if (!bufsize || !out) return;
	if (index >= len) { out[0] = 0; return; }
	unsigned n = len - index;
	if (n > bufsize - 1) n = bufsize - 1;
	memcpy(out, buf + index, n);
	out[n] = 0;
}

int String::indexOf(char c) const { return indexOf(c, 0); }
int String::indexOf(char c, unsigned int from) const
{
	if (from >= len) return -1;
	const char *p = (const char *)memchr(buf + from, c, len - from);
	return p ? (int)(p - buf) : -1;
}
int String::indexOf(const String &s) const { return indexOf(s, 0); }
int String::indexOf(const String &s, unsigned int from) const
{
	if (from >= len) return -1;
	const char *p = strstr(buf + from, s.c_str());
	return p ? (int)(p - buf) : -1;
}
int String::lastIndexOf(char c) const { return len ? lastIndexOf(c, len - 1) : -1; }
int String::lastIndexOf(char c, unsigned int from) const
{
	if (!len) return -1;
	if (from >= len) from = len - 1;
	for (int i = from; i >= 0; i--) if (buf[i] == c) return i;
	return -1;
}
int String::lastIndexOf(const String &s) const { return lastIndexOf(s, len); }
int String::lastIndexOf(const String &s, unsigned int from) const
{
	if (s.len == 0 || len == 0 || s.len > len) return -1;
	if (from > len - s.len) from = len - s.len;
	for (int i = from; i >= 0; i--) if (memcmp(buf + i, s.buf, s.len) == 0) return i;
	return -1;
}

String String::substring(unsigned int from, unsigned int to) const
{
	if (from > to) { unsigned t = from; from = to; to = t; }
	if (from >= len) return String();
	if (to > len) to = len;
	String r;
	r.copy(buf + from, to - from);
	return r;
}

void String::replace(char find, char repl)
{
	for (unsigned i = 0; i < len; i++) if (buf[i] == find) buf[i] = repl;
}
void String::replace(const String &find, const String &repl)
{
	if (!len || !find.len) return;
	String out;
	unsigned i = 0;
	while (i < len) {
		if (i + find.len <= len && memcmp(buf + i, find.buf, find.len) == 0) {
			out.concat(repl);
			i += find.len;
		} else {
			out.concat(buf[i++]);
		}
	}
	*this = out;
}
void String::remove(unsigned int index) { remove(index, len); }
void String::remove(unsigned int index, unsigned int count)
{
	if (index >= len || !count) return;
	if (count > len - index) count = len - index;
	memmove(buf + index, buf + index + count, len - index - count);
	len -= count;
	buf[len] = 0;
}
void String::toLowerCase() { for (unsigned i = 0; i < len; i++) buf[i] = tolower((unsigned char)buf[i]); }
void String::toUpperCase() { for (unsigned i = 0; i < len; i++) buf[i] = toupper((unsigned char)buf[i]); }
void String::trim()
{
	if (!buf || !len) return;
	char *b = buf;
	while (isspace((unsigned char)*b)) b++;
	char *e = buf + len - 1;
	while (e >= b && isspace((unsigned char)*e)) e--;
	len = (e >= b) ? (unsigned)(e - b + 1) : 0;
	if (b != buf) memmove(buf, b, len);
	buf[len] = 0;
}

long String::toInt() const { return buf ? strtol(buf, nullptr, 10) : 0; }
double String::toDouble() const
{
	if (!buf) return 0;
	const char *p = buf;
	while (isspace((unsigned char)*p)) p++;
	bool neg = false;
	if (*p == '-' || *p == '+') neg = *p++ == '-';
	double v = 0, scale = 1;
	for (; isdigit((unsigned char)*p); p++) v = v * 10 + (*p - '0');
	if (*p == '.') {
		for (p++; isdigit((unsigned char)*p); p++) { v = v * 10 + (*p - '0'); scale *= 10; }
	}
	v /= scale;
	if (*p == 'e' || *p == 'E') {
		int e = (int)strtol(p + 1, nullptr, 10);
		while (e > 0) { v *= 10; e--; }
		while (e < 0) { v /= 10; e++; }
	}
	return neg ? -v : v;
}
float String::toFloat() const { return (float)toDouble(); }
