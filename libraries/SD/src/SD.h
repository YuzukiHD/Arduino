/* SPDX-License-Identifier: Apache-2.0 */
#ifndef SD_h
#define SD_h

#include <stdint.h>
#include "Arduino.h"

#ifndef FILE_READ
#define FILE_READ 0x01
#endif
/* the Arduino convention: FILE_WRITE opens for read/write, creates the file and appends */
#ifndef FILE_WRITE
#define FILE_WRITE 0x1A
#endif
#ifndef O_READ
#define O_READ 0x01
#endif
#ifndef O_WRITE
#define O_WRITE 0x02
#endif
#ifndef O_RDWR
#define O_RDWR 0x03
#endif
#ifndef O_CREAT
#define O_CREAT 0x08
#endif
#ifndef O_APPEND
#define O_APPEND 0x10
#endif
#ifndef O_TRUNC
#define O_TRUNC 0x20
#endif

/* An open file or directory; copies share the same open handle. */
class File : public Stream {
public:
	File() : _p(nullptr) {}
	File(const File &f);
	File &operator=(const File &f);
	~File();

	size_t write(uint8_t c) override;
	size_t write(const uint8_t *buf, size_t size) override;
	using Print::write;
	int read() override;
	int read(void *buf, size_t size);
	int peek() override;
	int available() override;
	void flush() override;

	bool seek(uint32_t pos);
	uint32_t position();
	uint32_t size();
	void close();
	operator bool() const { return _p != nullptr; }
	const char *name();
	bool isDirectory();
	File openNextFile(uint8_t mode = FILE_READ);
	void rewindDirectory();

	struct Impl; /* opaque */
	explicit File(Impl *p) : _p(p) {}

private:
	Impl *_p;
};

/* A FAT volume. `SD` is the card slot, `USBDrive` an USB mass storage device (see USBHost). */
class SDClass {
public:
	SDClass(const char *disk, const char *mount) : _disk(disk), _mount(mount) {}

	/* The chip select argument of the classic API is ignored */
	bool begin(uint8_t csPin = 0);
	void end();

	File open(const char *path, uint8_t mode = FILE_READ);
	File open(const String &path, uint8_t mode = FILE_READ) { return open(path.c_str(), mode); }
	bool exists(const char *path);
	bool exists(const String &path) { return exists(path.c_str()); }
	/* creates the missing parent directories as well */
	bool mkdir(const char *path);
	bool mkdir(const String &path) { return mkdir(path.c_str()); }
	bool remove(const char *path);
	bool remove(const String &path) { return remove(path.c_str()); }
	bool rmdir(const char *path);
	bool rmdir(const String &path) { return rmdir(path.c_str()); }
	bool rename(const char *from, const char *to);

	uint64_t totalBytes();
	uint64_t freeBytes();
	uint64_t cardSize();
	bool mounted() const { return _mounted; }

private:
	const char *_disk;
	const char *_mount;
	bool _mounted = false;
	bool full(const char *path, char *out, size_t n);
};

extern SDClass SD;
extern SDClass USBDrive;

#endif
