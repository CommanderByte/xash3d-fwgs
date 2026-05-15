#pragma once
// xash3dpp — virtual file handle for streaming I/O
// Legacy reference: filesystem/filesystem_internal.h  (file_t)
//
// File is the public abstract base.  Callers always hold std::unique_ptr<File>.
// The concrete OsFile class lives entirely inside src/filesystem/file.cpp.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace xash::filesystem {

using FsOffset = std::int64_t;

// Seek origin — typed replacement for the SEEK_SET / SEEK_CUR / SEEK_END macros.
enum class SeekOrigin : int {
    Begin   = 0,   // SEEK_SET
    Current = 1,   // SEEK_CUR
    End     = 2,   // SEEK_END
};

// Non-copyable, non-movable streaming file handle.
// Transparent decompression is handled internally for archive-backed files.
class File {
public:
    File()                       = default;
    virtual ~File()              = default;

    // Pool-aware deallocation — called by std::unique_ptr<File>'s default
    // deleter when *this was created via pool_new.  mem_free reads back the
    // 8-byte header prepended by pool_new to locate the owning pool.
    static void operator delete(void* p) noexcept;
    static void operator delete(void* p, std::size_t) noexcept;

    File(const File&)            = delete;
    File& operator=(const File&) = delete;

    [[nodiscard]] virtual FsOffset Read(std::span<std::byte> buf)        = 0;
    [[nodiscard]] virtual FsOffset Write(std::span<const std::byte> buf) = 0;
    [[nodiscard]] virtual FsOffset Seek(FsOffset offset, SeekOrigin origin) = 0;
    [[nodiscard]] virtual FsOffset Tell()   const                        = 0;
    [[nodiscard]] virtual FsOffset Length() const                        = 0;  // uncompressed size
    [[nodiscard]] virtual bool     Eof()    const                        = 0;
    virtual void     Flush()                               = 0;

    // Text helpers — return owned values; no caller-provided buffer needed.
    [[nodiscard]] virtual std::optional<std::string> Gets()        = 0;
    [[nodiscard]] virtual int                        Getc()        = 0;
    virtual void                       UnGetc(int c) = 0;
};

} // namespace xash::filesystem
