// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "secure_config.h"

#include <infiltratr/posix.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <unistd.h>

bool rs_secure_write_private_text(const char *path,
                                  const char *text,
                                  size_t length)
{
    if (!path || !text) return false;
    const int result = infiltratr_atomic_file_write_bytes(
        path, INFILTRATR_ATOMIC_FILE_PRIVATE, text, length);
    if (result != 0) {
        errno = result;
        return false;
    }
    return chmod(path, S_IRUSR | S_IWUSR) == 0;
}

bool rs_secure_read_bounded_text(const char *path,
                                 char *buffer,
                                 size_t buffer_size,
                                 size_t *length)
{
    if (length) *length = 0U;
    if (!path || !buffer || buffer_size < 2U) {
        errno = EINVAL;
        return false;
    }

    const int fd = open(path, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;

    struct stat st;
    if (fstat(fd, &st) != 0) {
        const int saved = errno;
        (void)close(fd);
        errno = saved;
        return false;
    }
    if (!S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
        (st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        (void)close(fd);
        errno = EACCES;
        return false;
    }

    size_t used = 0U;
    while (used + 1U < buffer_size) {
        const ssize_t amount = read(fd, buffer + used, buffer_size - used - 1U);
        if (amount > 0) {
            used += (size_t)amount;
            continue;
        }
        if (amount == 0) break;
        if (errno == EINTR) continue;
        const int saved = errno;
        (void)close(fd);
        errno = saved;
        return false;
    }

    if (used + 1U == buffer_size) {
        unsigned char extra;
        ssize_t amount;
        do {
            amount = read(fd, &extra, 1U);
        } while (amount < 0 && errno == EINTR);
        if (amount > 0) {
            (void)close(fd);
            errno = EOVERFLOW;
            return false;
        }
        if (amount < 0) {
            const int saved = errno;
            (void)close(fd);
            errno = saved;
            return false;
        }
    }

    if (close(fd) != 0) return false;
    buffer[used] = '\0';
    if (length) *length = used;
    return true;
}
