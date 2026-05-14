#pragma once
// xash3dpp — RAII native file descriptor wrapper  (internal)
// Legacy reference: filesystem/filesystem_internal.h  (int handle in file_t)
// Modernization finding H-4: eliminates fd leaks on early-return paths.
//
// OsFd::close() is defined in platform/posix.cpp or platform/win32.cpp.

namespace xash::filesystem {

class OsFd {
public:
    OsFd() noexcept = default;
    explicit OsFd(int fd) noexcept : fd_{fd} {}
    ~OsFd() noexcept { close(); }

    OsFd(const OsFd&)            = delete;
    OsFd& operator=(const OsFd&) = delete;

    OsFd(OsFd&& o) noexcept : fd_{o.fd_} { o.fd_ = -1; }
    OsFd& operator=(OsFd&& o) noexcept {
        close();
        fd_   = o.fd_;
        o.fd_ = -1;
        return *this;
    }

    [[nodiscard]] int  get()     const noexcept { return fd_; }
    [[nodiscard]] bool valid()   const noexcept { return fd_ >= 0; }
    int                release() noexcept       { int f = fd_; fd_ = -1; return f; }

    void close() noexcept;

private:
    int fd_ = -1;
};

} // namespace xash::filesystem
