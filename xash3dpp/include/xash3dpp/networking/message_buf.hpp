#pragma once
// xash3dpp — MessageBuf: bit/byte codec over a caller-owned buffer
// Legacy reference: engine/common/net_buffer.{c,h}
//
// MessageBuf replaces the legacy `sizebuf_t` POD plus its MSG_* free function
// family with a value-semantic class that owns no storage and is safe to
// instantiate per call site (including on the stack).  No globals.
//
// Endianness: all multi-byte primitives are little-endian on the wire, which
// matches every GoldSrc/Xash target platform.  The class makes no concession
// for big-endian hosts at this layer; if a port to a BE platform ever
// happens, swap inline in the implementation, not at call sites.
//
// Overflow handling: writes past the end set a sticky overflow flag and
// silently drop subsequent bytes.  Reads past the end set the flag and
// return zero / empty.  Callers check `overflowed()` before trusting any
// I/O sequence.  No exceptions.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace xash::networking {

// ---------------------------------------------------------------------------
// SeekOrigin — equivalent to SEEK_SET / SEEK_CUR / SEEK_END
// ---------------------------------------------------------------------------

enum class SeekOrigin : std::uint8_t
{
    Begin,
    Current,
    End,
};

// ---------------------------------------------------------------------------
// MessageBuf — bit-level reader/writer over an externally-owned buffer.
//
// The same instance can be used for both read and write; the legacy engine
// aliased `MSG_StartReading` onto `MSG_StartWriting` for that reason.  Mixing
// reads and writes is the caller's responsibility.
// ---------------------------------------------------------------------------

class MessageBuf
{
public:
    constexpr MessageBuf() noexcept = default;

    // Construct over caller-owned storage.  `data` must outlive *this*.
    // `name` is a const debug label for diagnostics; not copied.
    MessageBuf( std::span<std::byte> data, const char *name = "unnamed" ) noexcept;

    // ---- Reset / re-target ----------------------------------------------

    void reset() noexcept;                                   // rewind to bit 0, clear overflow
    void rebind( std::span<std::byte> data, const char *name = "unnamed" ) noexcept;

    // ---- Accessors -------------------------------------------------------

    [[nodiscard]] const char         *name()               const noexcept { return name_; }
    [[nodiscard]] bool                overflowed()         const noexcept { return overflow_; }
    [[nodiscard]] std::size_t         num_bits_written()   const noexcept { return cur_bit_; }
    [[nodiscard]] std::size_t         num_bytes_written()  const noexcept; // bit count padded up to a byte
    [[nodiscard]] std::size_t         real_bytes_written() const noexcept { return cur_bit_ >> 3; }
    [[nodiscard]] std::size_t         num_bits_left()      const noexcept;
    [[nodiscard]] std::size_t         num_bytes_left()     const noexcept { return num_bits_left() >> 3; }
    [[nodiscard]] std::size_t         max_bits()           const noexcept { return num_bits_; }
    [[nodiscard]] std::size_t         max_bytes()          const noexcept { return num_bits_ >> 3; }
    [[nodiscard]] std::span<std::byte>data()                     noexcept { return { data_, num_bits_ >> 3 }; }
    [[nodiscard]] std::span<const std::byte> data()        const noexcept { return { data_, num_bits_ >> 3 }; }

    // ---- Seek ------------------------------------------------------------

    // Returns true on success, false on out-of-range request.
    [[nodiscard]] bool seek_to_bit( std::ptrdiff_t bit, SeekOrigin origin ) noexcept;
    [[nodiscard]] std::size_t tell_bit() const noexcept { return cur_bit_; }

    // ---- Bit writes ------------------------------------------------------

    void write_one_bit ( int value )                           noexcept;
    void write_ubit_long( std::uint32_t value, int num_bits )  noexcept;
    void write_sbit_long( std::int32_t  value, int num_bits )  noexcept;
    [[nodiscard]] bool write_bits( std::span<const std::byte> src, std::size_t num_bits ) noexcept;

    // ---- Byte writes -----------------------------------------------------

    void write_byte ( std::uint8_t  v ) noexcept;
    void write_char ( std::int8_t   v ) noexcept;
    void write_word ( std::uint16_t v ) noexcept;
    void write_short( std::int16_t  v ) noexcept;
    void write_dword( std::uint32_t v ) noexcept;
    void write_long ( std::int32_t  v ) noexcept;
    void write_float( float         v ) noexcept;
    [[nodiscard]] bool write_string( std::string_view s ) noexcept; // writes NUL terminator
    [[nodiscard]] bool write_bytes ( std::span<const std::byte> src ) noexcept;

    // ---- Bit reads -------------------------------------------------------

    [[nodiscard]] int           read_one_bit ()                       noexcept;
    [[nodiscard]] std::uint32_t read_ubit_long( int num_bits )        noexcept;
    [[nodiscard]] std::int32_t  read_sbit_long( int num_bits )        noexcept;
    [[nodiscard]] bool read_bits( std::span<std::byte> dst, std::size_t num_bits ) noexcept;

    // ---- Byte reads ------------------------------------------------------

    [[nodiscard]] std::uint8_t  read_byte () noexcept;
    [[nodiscard]] std::int8_t   read_char () noexcept;
    [[nodiscard]] std::uint16_t read_word () noexcept;
    [[nodiscard]] std::int16_t  read_short() noexcept;
    [[nodiscard]] std::uint32_t read_dword() noexcept;
    [[nodiscard]] std::int32_t  read_long () noexcept;
    [[nodiscard]] float         read_float() noexcept;

    // Reads into caller buffer up to `dst.size()-1` chars; always writes a
    // terminating NUL when `dst.size() > 0`.  Returns the number of chars
    // read (excluding NUL).  Sets overflow if the source string is longer.
    std::size_t read_string( std::span<char> dst ) noexcept;
    [[nodiscard]] bool read_bytes( std::span<std::byte> dst ) noexcept;

    // ---- Quantised reals (GoldSrc/Xash delta-network conventions) -------
    //
    // Coord: legacy uses 1/8-unit fixed-point packed into an int16 (default).
    //        The `large` overload writes a rounded int16 directly (matches
    //        the legacy ENGINE_WRITE_LARGE_COORD branch).
    // BitAngle: quantises an angle in [0,360) to `num_bits` bits.

    void  write_coord     ( float v )                  noexcept;
    void  write_coord_large( float v )                 noexcept;
    void  write_bit_angle ( float angle, int num_bits ) noexcept;
    void  write_vec3_coord( float x, float y, float z ) noexcept;
    void  write_vec3_angles( float x, float y, float z ) noexcept;

    [[nodiscard]] float read_coord     ()              noexcept;
    [[nodiscard]] float read_coord_large()             noexcept;
    [[nodiscard]] float read_bit_angle ( int num_bits ) noexcept;
    void                read_vec3_coord ( float &x, float &y, float &z ) noexcept;
    void                read_vec3_angles( float &x, float &y, float &z ) noexcept;

private:
    bool check_overflow( std::size_t additional_bits ) noexcept;

    std::byte   *data_      = nullptr;
    std::size_t  num_bits_  = 0;   // capacity in bits
    std::size_t  cur_bit_   = 0;
    const char  *name_      = "unnamed";
    bool         overflow_  = false;
};

} // namespace xash::networking
