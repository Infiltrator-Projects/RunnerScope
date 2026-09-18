// SPDX-License-Identifier: GPL-3.0-or-later
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

    struct stat st;
    if (stat(path, &st) != 0) return false;
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        errno = EACCES;
        return false;
    }

    size_t used = 0U;
    const InfiltratrIoResult result =
        infiltratr_read_text_file_ex(path, buffer, buffer_size, &used);
    if (result != INFILTRATR_IO_OK) {
        errno = result == INFILTRATR_IO_TRUNCATED ? EOVERFLOW : EIO;
        return false;
    }
    if (length) *length = used;
    return true;
}
