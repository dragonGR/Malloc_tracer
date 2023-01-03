/*  Malloc_Tracer | a malloc tracer Tool
    Copyright (C) 2023 Alexander Tsanis

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#define _GNU_SOURCE
#include <dlfcn.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pthread.h>

#define MAX_BUFFER (4 * 1024)

static void* (*malloc0)(size_t size);
static void (*free0)(void* ptr);

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int fd;

static void __attribute__((constructor))
init()
{
    * (void**)&malloc0 = dlsym(RTLD_NEXT, "malloc");
    * (void**)&free0 = dlsym(RTLD_NEXT, "free");

    char* filename = getenv("MALLOC_TRACER_FILENAME");
    fd = open(filename ? filename : "malloc_trace.log",
	      O_RDWR | O_CREAT | O_TRUNC,
	      S_IRUSR | S_IWUSR | S_IRGRP | S_IWOTH);

    char* maps = getenv("MALLOC_TRACER_MAPS");
    if (maps && strlen(maps) > 0) {
	int src = open("/proc/self/maps", O_RDONLY);
	int dst = open(maps,
		       O_RDWR | O_CREAT | O_TRUNC,
		       S_IRUSR | S_IWUSR | S_IRGRP | S_IWOTH);

	char buf[MAX_BUFFER];
	ssize_t b;
	while ((b = read(src, buf, sizeof(buf))) > 0) {
	    write(dst, buf, b);
	}

	close(src);
	close(dst);
    }
}

static void __attribute__((destructor))
fin()
{
    close(fd);
}

struct layout_t {
#if (defined __x86_64) || (defined __x86)
    struct layout *next;
    void *ret;
#elif (defined __PPC64__)
    struct layout *__unbounded next;
    long condition_register;
    void *__unbounded ret;
#elif (defined __PPC32__)
    struct layout *__unbounded next;
    void *__unbounded ret;
#endif
};

static char hex2char(int hex)
{
    if (hex < 10) {
	return hex + '0';
    }
    return (hex - 10) + 'a';
}

static int hexdump(char* buf, const char* ptr, size_t length)
{
    int idx = 0;
    buf[idx++] = '0';
    buf[idx++] = 'x';

    for (int i = 0; i < length; ++i) {
#if (defined __BIG_ENDIAN__)
	const unsigned char c = ptr[i];
#else
	const unsigned char c = ptr[length - 1 - i];
#endif
	buf[idx++] = hex2char(c / 16);
	buf[idx++] = hex2char(c % 16);
    }

    return idx;
}

static int __attribute__((always_inline))
dump_callstack(char* buf)
{
    int pos = 0;
    struct layout_t* lo = __builtin_frame_address(0);
    while (lo) {
	void* caller = lo->ret;
	buf[pos++] = ' ';
	pos += hexdump(&buf[pos], (char*) &caller, sizeof(caller));

	lo = (struct layout_t*) lo->next;
    }

    return pos;
}

void* malloc(size_t size)
{
    void* ptr = malloc0(size);

    char buf[MAX_BUFFER];

    int pos = 0;
    buf[pos++] = 'm';
    buf[pos++] = ' ';

    pos += hexdump(&buf[pos], (char*) &ptr, sizeof(ptr));
    buf[pos++] = ' ';
    pos += hexdump(&buf[pos], (char*) &size, sizeof(size));

    pos += dump_callstack(&buf[pos]);
#if 0
    struct layout_t* lo = __builtin_frame_address(0);
    while (lo) {
	void* caller = lo->ret;
	buf[pos++] = ' ';
	pos += hexdump(&buf[pos], (char*) &caller, sizeof(caller));

	lo = (struct layout_t*) lo->next;
    }
#endif

    buf[pos++] = '\n';

    pthread_mutex_lock(&mutex);
    write(fd, buf, pos);
    pthread_mutex_unlock(&mutex);

    return ptr;
}

void free(void* ptr)
{
    free0(ptr);

    if (ptr == NULL) return;

    char buf[MAX_BUFFER];

    int pos = 0;
    buf[pos++] = 'f';
    buf[pos++] = ' ';

    pos += hexdump(&buf[pos], (char*) &ptr, sizeof(ptr));
    buf[pos++] = ' ';

    size_t dummy = 0;
    pos += hexdump(&buf[pos], (char*) &dummy, sizeof(dummy));

    pos += dump_callstack(&buf[pos]);

    buf[pos++] = '\n';

    pthread_mutex_lock(&mutex);
    write(fd, buf, pos);
    pthread_mutex_unlock(&mutex);
}