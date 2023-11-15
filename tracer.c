#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>

#define MAX_BUFFER (4 * 1024)

static void* (*malloc0)(size_t size);
static void (*free0)(void* ptr);
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int fd;

struct layout_t {
    struct layout_t* next;
    void* ret;
};

static void __attribute__((constructor))
init() {
    *(void**)&malloc0 = dlsym(RTLD_NEXT, "malloc");
    *(void**)&free0 = dlsym(RTLD_NEXT, "free");

    char* filename = getenv("MALLOC_TRACER_FILENAME");
    fd = open(filename ? filename : "malloc_trace.log",
              O_RDWR | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IWOTH);

    char* maps = getenv("MALLOC_TRACER_MAPS");
    if (maps && strlen(maps) > 0) {
        int src = open("/proc/self/maps", O_RDONLY);
        int dst = open(maps, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWOTH);

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
fin() {
    close(fd);
}

static char hex2char(int hex) {
    return hex < 10 ? hex + '0' : (hex - 10) + 'a';
}

static int hexdump(char* buf, const char* ptr, size_t length) {
    int idx = 0;
    buf[idx++] = '0';
    buf[idx++] = 'x';

    for (int i = 0; i < length; ++i) {
        const unsigned char c = ptr[i];
        buf[idx++] = hex2char(c / 16);
        buf[idx++] = hex2char(c % 16);
    }

    return idx;
}

static int __attribute__((always_inline))
dump_callstack(char* buf) {
    int pos = 0;
    struct layout_t* lo = __builtin_frame_address(0);

    while (lo) {
        void* caller = lo->ret;
        buf[pos++] = ' ';
        pos += hexdump(&buf[pos], (char*)&caller, sizeof(caller));
        lo = lo->next;
    }

    return pos;
}

void* malloc(size_t size) {
    void* ptr = malloc0(size);

    char buf[MAX_BUFFER];
    int pos = 0;

    buf[pos++] = 'm';
    buf[pos++] = ' ';

    pos += hexdump(&buf[pos], (char*)&ptr, sizeof(ptr));
    buf[pos++] = ' ';
    pos += hexdump(&buf[pos], (char*)&size, sizeof(size));

    pos += dump_callstack(&buf[pos]);
    buf[pos++] = '\n';

    pthread_mutex_lock(&mutex);
    write(fd, buf, pos);
    pthread_mutex_unlock(&mutex);

    return ptr;
}

void free(void* ptr) {
    free0(ptr);

    if (ptr == NULL) return;

    char buf[MAX_BUFFER];
    int pos = 0;

    buf[pos++] = 'f';
    buf[pos++] = ' ';

    pos += hexdump(&buf[pos], (char*)&ptr, sizeof(ptr));
    buf[pos++] = ' ';

    size_t dummy = 0;
    pos += hexdump(&buf[pos], (char*)&dummy, sizeof(dummy));

    pos += dump_callstack(&buf[pos]);
    buf[pos++] = '\n';

    pthread_mutex_lock(&mutex);
    write(fd, buf, pos);
    pthread_mutex_unlock(&mutex);
}