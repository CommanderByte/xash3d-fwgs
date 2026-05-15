#pragma once
// xash3dpp — RAII native file-descriptor wrapper
// Legacy reference: filesystem/filesystem_internal.h (int handle in file_t)
//
// OsFd owns exactly one OS file descriptor (a POSIX int fd or a Win32 CRT fd
// obtained via _open_osfhandle).  The close() method is defined in the
// platform-specific os_io implementation file so that <windows.h> / <unistd.h>
// are never included in this header.
//
// Rules:
//   • Not copyable — ownership is exclusive.
//   • Movable — transfer is O(1) and leaves the source invalid.
//   • ~OsFd() calls close(), which is a no-op on an already-invalid descriptor.

namespace xash::platform {

class OsFd
{
public:
    OsFd() noexcept = default;
    explicit OsFd( int fd ) noexcept : fd_{ fd } {}
    ~OsFd() noexcept { close(); }

    OsFd( const OsFd & )             = delete;
    OsFd &operator=( const OsFd & )  = delete;

    OsFd( OsFd &&o ) noexcept : fd_{ o.fd_ } { o.fd_ = -1; }
    OsFd &operator=( OsFd &&o ) noexcept
    {
        close();
        fd_   = o.fd_;
        o.fd_ = -1;
        return *this;
    }

    [[nodiscard]] int  get()    const noexcept { return fd_; }
    [[nodiscard]] bool valid()  const noexcept { return fd_ >= 0; }

    // Relinquish ownership without closing.  Caller becomes responsible for
    // the descriptor's lifetime.
    int release() noexcept { int f = fd_; fd_ = -1; return f; }

    // Close the descriptor.  Defined in the platform os_io translation unit.
    void close() noexcept;

private:
    int fd_ = -1;
};

} // namespace xash::platform
