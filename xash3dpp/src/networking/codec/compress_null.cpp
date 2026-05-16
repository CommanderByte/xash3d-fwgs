// xash3dpp — compression backend: null implementation (link-time selected)
// Selected by XASH_NET_COMPRESSION=OFF (dedicated-server default).
// Mirror TU: compress_bz2.cpp (selected by XASH_NET_COMPRESSION=ON).
//
// Both TUs export the same xash::networking::bz2 symbols; CMake links exactly
// one of them into xash3dpp_networking.  This file gives netchan a no-op
// codec that always returns NetError::NotInitialised and advertises
// `available() == false`, so the caller takes the uncompressed path.
//
// Note: LZSS is wire-frozen and always compiled in (see compress_lzss.cpp),
// regardless of XASH_NET_COMPRESSION — this option only gates the bzip2
// codec (see docs/architecture/networking/index.md, "CMake targets").

#include <xash3dpp/private/networking/codec/compress.hpp>

#include <xash3dpp/core/log.hpp>

namespace xash::networking::bz2 {

namespace core = ::xash::core;

bool available() noexcept
{
    return false;
}

Result<std::vector<std::byte>> compress( std::span<const std::byte> /*src*/ )
{
    core::log( core::LogLevel::Verbose, "compress_null",
               "compress(): compression disabled at build time "
               "(XASH_NET_COMPRESSION=OFF)" );
    return std::unexpected( NetError::NotInitialised );
}

Result<std::size_t> decompress( std::span<const std::byte> /*src*/,
                                std::span<std::byte>       /*dst*/ ) noexcept
{
    core::log( core::LogLevel::Verbose, "compress_null",
               "decompress(): compression disabled at build time "
               "(XASH_NET_COMPRESSION=OFF)" );
    return std::unexpected( NetError::NotInitialised );
}

} // namespace xash::networking::bz2
