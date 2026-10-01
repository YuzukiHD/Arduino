/* SPDX-License-Identifier: Apache-2.0 */
#include <ff.h>
#include <zephyr/fs/fs.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/disk_access.h>

#include <stdlib.h>
#include <string.h>

#include "SD.h"

#define PATH_MAX_LEN 352

struct File::Impl {
	int refs;
	bool dir;
	struct fs_file_t f;
	struct fs_dir_t d;
	char path[PATH_MAX_LEN];
	char name[MAX_FILE_NAME + 1];
};

/* one FATFS object and mount record per volume */
struct Volume {
	FATFS fat;
	struct fs_mount_t mp;
};
static Volume vol_sd, vol_usb;

SDClass SD("SD", "/SD:");
SDClass USBDrive("USB", "/USB:");

static Volume *volume_of(const SDClass *c)
{
	return c == &SD ? &vol_sd : &vol_usb;
}

bool SDClass::begin(uint8_t csPin)
{
	(void)csPin;
	if (_mounted) {
		return true;
	}
	if (disk_access_init(_disk) != 0) {
		return false;
	}
	Volume *v = volume_of(this);
	memset(&v->mp, 0, sizeof(v->mp));
	v->mp.type = FS_FATFS;
	v->mp.fs_data = &v->fat;
	v->mp.mnt_point = _mount;
	if (fs_mount(&v->mp) != 0) {
		return false;
	}
	_mounted = true;
	return true;
}

void SDClass::end()
{
	if (_mounted) {
		fs_unmount(&volume_of(this)->mp);
		_mounted = false;
	}
}

bool SDClass::full(const char *path, char *out, size_t n)
{
	if (!_mounted || !path) {
		return false;
	}
	while (*path == '/') {
		path++;
	}
	int r = snprintf(out, n, "%s/%s", _mount, path);
	if (r < 0 || (size_t)r >= n) {
		return false;
	}
	/* no trailing slash except for the root */
	size_t l = strlen(out);
	while (l > strlen(_mount) + 1 && out[l - 1] == '/') {
		out[--l] = 0;
	}
	return true;
}

static const char *base_name(const char *path)
{
	const char *s = strrchr(path, '/');
	return s ? s + 1 : path;
}

static File::Impl *impl_new(const char *path)
{
	File::Impl *p = (File::Impl *)calloc(1, sizeof(File::Impl));
	if (p) {
		p->refs = 1;
		strncpy(p->path, path, sizeof(p->path) - 1);
		strncpy(p->name, base_name(path), sizeof(p->name) - 1);
	}
	return p;
}

File SDClass::open(const char *path, uint8_t mode)
{
	char fp[PATH_MAX_LEN];
	struct fs_dirent st;

	if (!full(path, fp, sizeof(fp))) {
		return File();
	}
	/* the root of a volume cannot be stat'ed on FatFS: it is a directory that always exists */
	size_t ml = strlen(_mount);
	bool root = strncmp(fp, _mount, ml) == 0 && (fp[ml] == 0 || (fp[ml] == '/' && fp[ml + 1] == 0));
	if (root) {
		fp[ml] = 0; /* "/SD:" is the path fs_opendir() takes for the root */
		st.type = FS_DIR_ENTRY_DIR;
	}
	bool exists = root || fs_stat(fp, &st) == 0;
	File::Impl *p = impl_new(fp);
	if (!p) {
		return File();
	}
	if (exists && st.type == FS_DIR_ENTRY_DIR) {
		p->dir = true;
		fs_dir_t_init(&p->d);
		if (fs_opendir(&p->d, fp) != 0) {
			free(p);
			return File();
		}
		return File(p);
	}
	if (!exists && !(mode & O_CREAT)) {
		free(p);
		return File();
	}
	fs_mode_t fm = 0;
	if (mode & O_READ) fm |= FS_O_READ;
	if (mode & O_WRITE) fm |= FS_O_WRITE;
	if (mode & O_CREAT) fm |= FS_O_CREATE;
	if (mode & O_APPEND) fm |= FS_O_APPEND;
	if (!(fm & (FS_O_READ | FS_O_WRITE))) fm |= FS_O_READ;
	fs_file_t_init(&p->f);
	if (fs_open(&p->f, fp, fm) != 0) {
		free(p);
		return File();
	}
	if (mode & O_TRUNC) {
		fs_truncate(&p->f, 0);
	}
	return File(p);
}

bool SDClass::exists(const char *path)
{
	char fp[PATH_MAX_LEN];
	struct fs_dirent st;
	if (!full(path, fp, sizeof(fp))) {
		return false;
	}
	size_t ml = strlen(_mount);
	if (strncmp(fp, _mount, ml) == 0 && (fp[ml] == 0 || (fp[ml] == '/' && fp[ml + 1] == 0))) {
		return true; /* the root of the volume */
	}
	return fs_stat(fp, &st) == 0;
}

bool SDClass::mkdir(const char *path)
{
	char fp[PATH_MAX_LEN];
	if (!full(path, fp, sizeof(fp))) {
		return false;
	}
	size_t root = strlen(_mount) + 1;
	for (size_t i = root; ; i++) {
		if (fp[i] == '/' || fp[i] == 0) {
			char c = fp[i];
			fp[i] = 0;
			struct fs_dirent st;
			if (fs_stat(fp, &st) != 0 && fs_mkdir(fp) != 0) {
				return false;
			}
			fp[i] = c;
			if (c == 0) {
				break;
			}
		}
	}
	return true;
}

bool SDClass::remove(const char *path)
{
	char fp[PATH_MAX_LEN];
	return full(path, fp, sizeof(fp)) && fs_unlink(fp) == 0;
}

bool SDClass::rmdir(const char *path)
{
	return remove(path);
}

bool SDClass::rename(const char *from, const char *to)
{
	char a[PATH_MAX_LEN], b[PATH_MAX_LEN];
	return full(from, a, sizeof(a)) && full(to, b, sizeof(b)) && fs_rename(a, b) == 0;
}

uint64_t SDClass::totalBytes()
{
	struct fs_statvfs sv;
	if (!_mounted || fs_statvfs(_mount, &sv) != 0) {
		return 0;
	}
	return (uint64_t)sv.f_frsize * sv.f_blocks;
}

uint64_t SDClass::freeBytes()
{
	struct fs_statvfs sv;
	if (!_mounted || fs_statvfs(_mount, &sv) != 0) {
		return 0;
	}
	return (uint64_t)sv.f_frsize * sv.f_bfree;
}

uint64_t SDClass::cardSize()
{
	uint32_t n = 0, ss = 0;
	if (disk_access_ioctl(_disk, DISK_IOCTL_GET_SECTOR_COUNT, &n) != 0 ||
	    disk_access_ioctl(_disk, DISK_IOCTL_GET_SECTOR_SIZE, &ss) != 0) {
		return 0;
	}
	return (uint64_t)n * ss;
}

/* ---- File ---------------------------------------------------------------- */

File::File(const File &f) : _p(f._p)
{
	if (_p) {
		_p->refs++;
	}
}

File &File::operator=(const File &f)
{
	if (this != &f) {
		close();
		_p = f._p;
		if (_p) {
			_p->refs++;
		}
	}
	return *this;
}

File::~File()
{
	close();
}

void File::close()
{
	if (!_p) {
		return;
	}
	if (--_p->refs == 0) {
		if (_p->dir) {
			fs_closedir(&_p->d);
		} else {
			fs_close(&_p->f);
		}
		free(_p);
	}
	_p = nullptr;
}

size_t File::write(uint8_t c)
{
	return write(&c, 1);
}

size_t File::write(const uint8_t *buf, size_t size)
{
	if (!_p || _p->dir) {
		return 0;
	}
	ssize_t r = fs_write(&_p->f, buf, size);
	return r < 0 ? 0 : (size_t)r;
}

int File::read(void *buf, size_t size)
{
	if (!_p || _p->dir) {
		return -1;
	}
	return (int)fs_read(&_p->f, buf, size);
}

int File::read()
{
	uint8_t c;
	return read(&c, 1) == 1 ? c : -1;
}

int File::peek()
{
	if (!_p || _p->dir) {
		return -1;
	}
	uint8_t c;
	if (fs_read(&_p->f, &c, 1) != 1) {
		return -1;
	}
	fs_seek(&_p->f, -1, FS_SEEK_CUR);
	return c;
}

int File::available()
{
	if (!_p || _p->dir) {
		return 0;
	}
	off_t pos = fs_tell(&_p->f);
	fs_seek(&_p->f, 0, FS_SEEK_END);
	off_t end = fs_tell(&_p->f);
	fs_seek(&_p->f, pos, FS_SEEK_SET);
	return end > pos ? (int)MIN(end - pos, (off_t)0x7fff) : 0;
}

void File::flush()
{
	if (_p && !_p->dir) {
		fs_sync(&_p->f);
	}
}

bool File::seek(uint32_t pos)
{
	return _p && !_p->dir && fs_seek(&_p->f, pos, FS_SEEK_SET) == 0;
}

uint32_t File::position()
{
	return (_p && !_p->dir) ? (uint32_t)fs_tell(&_p->f) : 0;
}

uint32_t File::size()
{
	if (!_p || _p->dir) {
		return 0;
	}
	off_t pos = fs_tell(&_p->f);
	fs_seek(&_p->f, 0, FS_SEEK_END);
	off_t end = fs_tell(&_p->f);
	fs_seek(&_p->f, pos, FS_SEEK_SET);
	return (uint32_t)end;
}

const char *File::name()
{
	return _p ? _p->name : "";
}

bool File::isDirectory()
{
	return _p && _p->dir;
}

File File::openNextFile(uint8_t mode)
{
	if (!_p || !_p->dir) {
		return File();
	}
	struct fs_dirent ent;
	if (fs_readdir(&_p->d, &ent) != 0 || ent.name[0] == 0) {
		return File();
	}
	char fp[PATH_MAX_LEN];
	if (snprintf(fp, sizeof(fp), "%s/%s", _p->path, ent.name) >= (int)sizeof(fp)) {
		return File();
	}
	/* path is already absolute, with the mount point: open it directly */
	File::Impl *p = impl_new(fp);
	if (!p) {
		return File();
	}
	if (ent.type == FS_DIR_ENTRY_DIR) {
		p->dir = true;
		fs_dir_t_init(&p->d);
		if (fs_opendir(&p->d, fp) != 0) {
			free(p);
			return File();
		}
		return File(p);
	}
	fs_file_t_init(&p->f);
	fs_mode_t fm = (mode & O_WRITE) ? (FS_O_READ | FS_O_WRITE) : FS_O_READ;
	if (fs_open(&p->f, fp, fm) != 0) {
		free(p);
		return File();
	}
	return File(p);
}

void File::rewindDirectory()
{
	if (_p && _p->dir) {
		fs_closedir(&_p->d);
		fs_dir_t_init(&_p->d);
		fs_opendir(&_p->d, _p->path);
	}
}
