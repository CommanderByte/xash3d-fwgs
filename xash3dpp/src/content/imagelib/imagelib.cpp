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

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/memory/memory.hpp>

namespace xash::imagelib {

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

} // namespace xash::imagelib
