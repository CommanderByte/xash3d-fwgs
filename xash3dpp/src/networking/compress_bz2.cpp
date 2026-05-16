// xash3dpp — compression backend: bzip2 implementation (link-time selected)
// Selected by XASH_NET_COMPRESSION=ON (Q-7 link-time-compat pattern).
// Mirror TU: compress_null.cpp (selected by XASH_NET_COMPRESSION=OFF).
//
// Both TUs export the same xash::networking::bz2 symbols; CMake links exactly
// one of them into xash3dpp_networking.  See:
//   * docs/boundaries/networking-boundary.md, OQ-7
//   * docs/architecture/networking/index.md, "CMake targets" table
//
// TODO(Layer 1 #10): wire 3rdparty/bzip2 (libbz2) into the xash3dpp CMake
// build and call BZ2_bzBuffToBuffCompress / BZ2_bzBuffToBuffDecompress here.
// Until that lands, this TU advertises a real-backend identity via
// `available() == true` but the codec bodies return NetError::NotInitialised
// so callers fall back to uncompressed transmission.  This keeps the
// link-time seam in place and forbids accidental linkage of *both* backends.

#include <xash3dpp/private/networking/compress.hpp>

#include <xash3dpp/core/log.hpp>

namespace xash::networking::bz2 {

namespace core = ::xash::core;

bool available() noexcept
{
    // Will flip to `true` and bodies will gain real impls when bzip2 is
    // linked.  Reporting `false` today lets the netchan flow skip the codec
    // entirely instead of calling compress() and discarding the error.
    return false;
}

Result<std::vector<std::byte>> compress( std::span<const std::byte> /*src*/ )
{
    core::log( core::LogLevel::Verbose, "compress_bz2",
               "compress(): TODO link 3rdparty/bzip2; returning NotInitialised" );
    return std::unexpected( NetError::NotInitialised );
}

Result<std::size_t> decompress( std::span<const std::byte> /*src*/,
                                std::span<std::byte>       /*dst*/ ) noexcept
{
    core::log( core::LogLevel::Verbose, "compress_bz2",
               "decompress(): TODO link 3rdparty/bzip2; returning NotInitialised" );
    return std::unexpected( NetError::NotInitialised );
}

} // namespace xash::networking::bz2
