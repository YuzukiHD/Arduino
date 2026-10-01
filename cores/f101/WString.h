/* SPDX-License-Identifier: Apache-2.0 */
#ifndef WString_h
#define WString_h

#include <stddef.h>
#include <stdint.h>

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(PSTR(s)))

class String {
public:
	String(const char *cstr = "");
	String(const String &s);
	String(const __FlashStringHelper *s);
	String(String &&s) noexcept;
	explicit String(char c);
	explicit String(unsigned char v, unsigned char base = 10);
	explicit String(int v, unsigned char base = 10);
	explicit String(unsigned int v, unsigned char base = 10);
	explicit String(long v, unsigned char base = 10);
	explicit String(unsigned long v, unsigned char base = 10);
	explicit String(long long v, unsigned char base = 10);
	explicit String(unsigned long long v, unsigned char base = 10);
	explicit String(float v, unsigned char decimals = 2);
	explicit String(double v, unsigned char decimals = 2);
	~String();

	bool reserve(unsigned int size);
	unsigned int length() const { return len; }

	String &operator=(const String &rhs);
	String &operator=(const char *cstr);
	String &operator=(String &&rhs) noexcept;

	bool concat(const String &s);
	bool concat(const char *cstr);
	bool concat(const char *cstr, unsigned int length);
	bool concat(char c);
	bool concat(unsigned char n);
	bool concat(int n);
	bool concat(unsigned int n);
	bool concat(long n);
	bool concat(unsigned long n);
	bool concat(float n);
	bool concat(double n);

	String &operator+=(const String &rhs) { concat(rhs); return *this; }
	String &operator+=(const char *cstr) { concat(cstr); return *this; }
	String &operator+=(char c) { concat(c); return *this; }
	String &operator+=(unsigned char n) { concat(n); return *this; }
	String &operator+=(int n) { concat(n); return *this; }
	String &operator+=(unsigned int n) { concat(n); return *this; }
	String &operator+=(long n) { concat(n); return *this; }
	String &operator+=(unsigned long n) { concat(n); return *this; }
	String &operator+=(float n) { concat(n); return *this; }
	String &operator+=(double n) { concat(n); return *this; }

	int compareTo(const String &s) const;
	bool equals(const String &s) const;
	bool equals(const char *cstr) const;
	bool operator==(const String &rhs) const { return equals(rhs); }
	bool operator==(const char *cstr) const { return equals(cstr); }
	bool operator!=(const String &rhs) const { return !equals(rhs); }
	bool operator!=(const char *cstr) const { return !equals(cstr); }
	bool operator<(const String &rhs) const { return compareTo(rhs) < 0; }
	bool operator>(const String &rhs) const { return compareTo(rhs) > 0; }
	bool operator<=(const String &rhs) const { return compareTo(rhs) <= 0; }
	bool operator>=(const String &rhs) const { return compareTo(rhs) >= 0; }
	bool equalsIgnoreCase(const String &s) const;
	bool startsWith(const String &prefix) const;
	bool startsWith(const String &prefix, unsigned int offset) const;
	bool endsWith(const String &suffix) const;

	explicit operator bool() const { return buf != nullptr; }
	char charAt(unsigned int index) const;
	void setCharAt(unsigned int index, char c);
	char operator[](unsigned int index) const { return charAt(index); }
	char &operator[](unsigned int index);
	void getBytes(unsigned char *out, unsigned int bufsize, unsigned int index = 0) const;
	void toCharArray(char *out, unsigned int bufsize, unsigned int index = 0) const
	{
		getBytes((unsigned char *)out, bufsize, index);
	}
	const char *c_str() const { return buf ? buf : ""; }
	char *begin() { return buf; }
	char *end() { return buf + len; }
	const char *begin() const { return c_str(); }
	const char *end() const { return c_str() + len; }

	int indexOf(char c) const;
	int indexOf(char c, unsigned int from) const;
	int indexOf(const String &s) const;
	int indexOf(const String &s, unsigned int from) const;
	int lastIndexOf(char c) const;
	int lastIndexOf(char c, unsigned int from) const;
	int lastIndexOf(const String &s) const;
	int lastIndexOf(const String &s, unsigned int from) const;
	String substring(unsigned int from) const { return substring(from, len); }
	String substring(unsigned int from, unsigned int to) const;

	void replace(char find, char repl);
	void replace(const String &find, const String &repl);
	void remove(unsigned int index);
	void remove(unsigned int index, unsigned int count);
	void toLowerCase();
	void toUpperCase();
	void trim();

	long toInt() const;
	float toFloat() const;
	double toDouble() const;

protected:
	char *buf = nullptr;
	unsigned int cap = 0;
	unsigned int len = 0;

	void invalidate();
	bool changeBuffer(unsigned int maxStrLen);
	String &copy(const char *cstr, unsigned int length);
};

class StringSumHelper : public String {
public:
	StringSumHelper(const String &s) : String(s) {}
	StringSumHelper(const char *p) : String(p) {}
	StringSumHelper(char c) : String(c) {}
	StringSumHelper(unsigned char n) : String(n) {}
	StringSumHelper(int n) : String(n) {}
	StringSumHelper(unsigned int n) : String(n) {}
	StringSumHelper(long n) : String(n) {}
	StringSumHelper(unsigned long n) : String(n) {}
	StringSumHelper(float n) : String(n) {}
	StringSumHelper(double n) : String(n) {}
};

StringSumHelper &operator+(const StringSumHelper &lhs, const String &rhs);
StringSumHelper &operator+(const StringSumHelper &lhs, const char *cstr);
StringSumHelper &operator+(const StringSumHelper &lhs, char c);
StringSumHelper &operator+(const StringSumHelper &lhs, unsigned char n);
StringSumHelper &operator+(const StringSumHelper &lhs, int n);
StringSumHelper &operator+(const StringSumHelper &lhs, unsigned int n);
StringSumHelper &operator+(const StringSumHelper &lhs, long n);
StringSumHelper &operator+(const StringSumHelper &lhs, unsigned long n);
StringSumHelper &operator+(const StringSumHelper &lhs, float n);
StringSumHelper &operator+(const StringSumHelper &lhs, double n);

#endif
