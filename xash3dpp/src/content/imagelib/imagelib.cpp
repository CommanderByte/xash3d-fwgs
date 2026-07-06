// xash3dpp — imagelib (image codec library) implementation
// Legacy reference: engine/common/imagelib/
//
// Existing subsystems used:
//   xash3dpp_memory — pool-backed allocations (create_pool / destroy_pool)
//   xash3dpp_core   — thread-role assertions (main-thread lifecycle)
//
// Skeleton: lifecycle only. The IImageCodec registry, the per-format decoders,
// and the WAD3 pack/unpack path are TODO (boundary O-2 / H-2/H-3/H-4).

#include <xash3dpp/imagelib/imagelib.hpp>
#include <xash3dpp/private/imagelib/codec.hpp>

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

#include <array>
#include <string_view>

namespace xash::imagelib {

namespace {

// File extension (no leading dot), or "" if none.
[[nodiscard]] std::string_view extension_of( std::string_view name ) noexcept
{
    const auto dot = name.find_last_of( '.' );
    if( dot == std::string_view::npos )
        return {};
    return name.substr( dot + 1 );
}

} // namespace

// ---------------------------------------------------------------------------
// Pimpl body
// ---------------------------------------------------------------------------

struct ImageDecoder::Impl {
    xash::memory::PoolHandle pool_;
    ImageStats               stats_ {};
    // TODO(O-2): the per-call decode scratch (the former global `imglib_t
    //   image` moves here — boundary H-1) and a std::array/registry of the
    //   IImageCodec implementations.
};

ImageDecoder::ImageDecoder() : impl_{std::make_unique<Impl>()} {}
ImageDecoder::~ImageDecoder() = default;
ImageDecoder::ImageDecoder(ImageDecoder&&) noexcept            = default;
ImageDecoder& ImageDecoder::operator=(ImageDecoder&&) noexcept = default;

bool ImageDecoder::init()
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    impl_->pool_ = xash::memory::create_pool("imagelib");
    return static_cast<bool>(impl_->pool_);
}

void ImageDecoder::shutdown()
{
    xash::core::assert_thread_role(xash::core::ThreadRole::Main);
    // Release all pool-owned resources before destroying the pool.
    if (impl_->pool_) {
        xash::memory::destroy_pool(impl_->pool_);
        impl_->pool_ = {};
    }
}

const ImageStats& ImageDecoder::stats() const noexcept { return impl_->stats_; }

Result<Image> ImageDecoder::decode( std::string_view name, std::span<const std::byte> file )
{
    xash::core::assert_thread_role( xash::core::ThreadRole::Main );

    if( file.empty() )
    {
        ++impl_->stats_.decode_failures;
        return std::unexpected( ImageError::Empty );
    }

    // Lowercase the extension for case-insensitive dispatch (legacy compares
    // stricmp against the load_game[] table). Extensions are short.
    std::array<char, 16> lo {};
    const std::string_view ext = extension_of( name );
    const std::size_t n = ext.size() < lo.size() ? ext.size() : 0;
    for( std::size_t i = 0; i < n; ++i )
    {
        const char c = ext[i];
        lo[i] = ( c >= 'A' && c <= 'Z' ) ? static_cast<char>( c - 'A' + 'a' ) : c;
    }
    const std::string_view ext_lc{ lo.data(), n };

    // Codec registry — one entry per codec as they land (O-2).
    static const IImageCodec *const registry[] = {
        &wad_codec(),
        &tga_codec(),
        &bmp_codec(),
        &dds_codec(),
    };

    for( const IImageCodec *codec : registry )
    {
        if( codec->handles( ext_lc ) )
        {
            Result<Image> r = codec->decode( name, file );
            if( r )
                ++impl_->stats_.images_decoded;
            else
                ++impl_->stats_.decode_failures;
            return r;
        }
    }

    ++impl_->stats_.decode_failures;
    return std::unexpected( ImageError::UnknownFormat );
}

} // namespace xash::imagelib
