// SPDX-License-Identifier: GPL-3.0-or-later
// This C++20-owned resource boundary preserves Linux storage's C ABI.
#define _POSIX_C_SOURCE 200809L
#include "platform/linux_storage_reader.h"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
class FileDescriptor final {
public:
    explicit FileDescriptor(int fd) noexcept : fd_(fd) {}
    ~FileDescriptor() { if (fd_ >= 0) (void)::close(fd_); }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    int get() const noexcept { return fd_; }
private:
    int fd_;
};

void erase_bytes(void* memory, std::size_t length) noexcept {
    volatile unsigned char* out = static_cast<volatile unsigned char*>(memory);
    while (length-- > 0U) *out++ = 0U;
}

class ReadBuffer final {
public:
    explicit ReadBuffer(std::size_t capacity)
        : data_(static_cast<char*>(g_malloc(capacity))), capacity_(capacity) {}
    ~ReadBuffer() {
        if (data_ != nullptr) {
            erase_bytes(data_, capacity_);
            g_free(data_);
        }
    }
    ReadBuffer(const ReadBuffer&) = delete;
    ReadBuffer& operator=(const ReadBuffer&) = delete;
    char* data() const noexcept { return data_; }
    char* release() noexcept {
        char* out = data_;
        data_ = nullptr;
        return out;
    }
private:
    char* data_;
    std::size_t capacity_;
};

void invalid_input(GError** error) {
    g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
        "Saved data is unreadable, oversized or invalid. The original file has been kept.");
}
} // namespace

extern "C" char* runner_linux_read_text(const char* path,
    std::size_t limit, gboolean* missing, GError** error)
{
    if (missing != nullptr) *missing = FALSE;
    if (path == nullptr ||
        limit > std::numeric_limits<std::size_t>::max() - 2U ||
        limit > static_cast<std::size_t>(G_MAXSSIZE)) {
        invalid_input(error);
        return nullptr;
    }

    const FileDescriptor fd(::open(path, O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW));
    if (fd.get() < 0) {
        const int failure = errno;
        if (failure == ENOENT) {
            if (missing != nullptr) *missing = TRUE;
            return g_strdup("");
        }
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(failure),
            "Unable to read saved data: %s. The original file has been kept.",
            g_strerror(failure));
        return nullptr;
    }

    struct stat state {};
    bool valid = ::fstat(fd.get(), &state) == 0 &&
        S_ISREG(state.st_mode) && state.st_size >= 0 &&
        static_cast<std::uint64_t>(state.st_size) <= limit;

    // Retain the open descriptor to prevent pathname-replacement races.
    // One extra byte detects files that grow beyond the size cap after fstat.
    ReadBuffer bytes(limit + 2U);
    std::size_t used = 0U;
    while (valid && used <= limit) {
        const ssize_t count = ::read(fd.get(), bytes.data() + used, limit + 1U - used);
        if (count < 0 && errno == EINTR) continue;
        if (count < 0) { valid = false; break; }
        if (count == 0) break;
        used += static_cast<std::size_t>(count);
    }
    if (!valid || used > limit || std::memchr(bytes.data(), '\0', used) != nullptr ||
        !g_utf8_validate(bytes.data(), static_cast<gssize>(used), nullptr)) {
        invalid_input(error);
        return nullptr;
    }
    bytes.data()[used] = '\0';
    return bytes.release();
}
