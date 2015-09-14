#include "mmap.hpp"
#include "autofree.hpp"
#include "throw.hpp"
#include <stdio.h>
#include <string.h>
#include <stdexcept>

#ifdef _MSC_VER
	#include <io.h>
	#include <Windows.h>
#else
	#include <sys/mman.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
#endif
#include <fcntl.h>

#if !defined(MAP_POPULATE)
	#define  MAP_POPULATE 0
#endif

namespace nark {

void mmap_close(void* base, size_t size) {
#ifdef _MSC_VER
	::UnmapViewOfFile(base);
#else
	::munmap(base, size);
#endif
}

void* mmap_load(const char* fname, size_t* fsize) {
#ifdef _MSC_VER
	int fd = ::_open(fname, _O_RDONLY);
	if (fd < 0) {
		DWORD err = GetLastError();
		THROW_STD(logic_error, "open(fname=%s,O_RDONLY) = %d(%X)"
			, fname, err, err);
	}
	LARGE_INTEGER lsize;
	HANDLE hFile = (HANDLE)_get_osfhandle(fd);
	if (!GetFileSizeEx(hFile, &lsize)) {
		DWORD err = GetLastError();
		::_close(fd);
		THROW_STD(logic_error, "GetFileSizeEx(fname=%s).Err=%d(%X)"
			, fname, err, err);
	}
	if (lsize.QuadPart > size_t(-1)) {
		::_close(fd);
		THROW_STD(logic_error, "fname=%s fsize=%I64u(%I64X) too large"
			, fname, lsize.QuadPart, lsize.QuadPart);
	}
	*fsize = size_t(lsize.QuadPart);
	HANDLE hMmap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (NULL == hMmap) {
		DWORD err = GetLastError();
		::_close(fd);
		THROW_STD(runtime_error, "CreateFileMapping(fname=%s).Err=%d(0x%X)"
			, fname, err, err);
	}
	void* base = MapViewOfFile(hMmap, FILE_MAP_READ, 0, 0, 0);
	if (NULL == base) {
		DWORD err = GetLastError();
		::CloseHandle(hMmap);
		::_close(fd);
		THROW_STD(runtime_error, "MapViewOfFile(fname=%s).Err=%d(0x%X)"
			, fname, err, err);
	}
	::CloseHandle(hMmap); // close before UnmapViewOfFile is OK
	::_close(fd);
#else
	int fd = ::open(fname, O_RDONLY);
	if (fd < 0) {
		THROW_STD(logic_error, "open(fname=%s,O_RDONLY) = %s", fname, strerror(errno));
	}
	struct stat st;
	if (::fstat(fd, &st) < 0) {
		THROW_STD(logic_error, "stat(fname=%s) = %s", fname, strerror(errno));
	}
	*fsize = st.st_size;
	void* base = ::mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (MAP_FAILED == base) {
		::close(fd);
		THROW_STD(logic_error, "mmap(fname=%s,READ SHARED,size=%lld) = %s"
			, fname, (long long)st.st_size, strerror(errno));
	}
	::close(fd);
#endif
	return base;
}

} // namespace nark

