// xash3dpp — audio output device implementations (Chunk 9, slice S9.1).
// NullDevice (the ratified null-audio production target) + SinkDevice (the
// virtually-clocked test consumer / S9.8 determinism witness).
// Legacy reference: engine/platform/stub/s_stub.c (null backend) + the SNDDMA_*
// seam (platform.h:390-402).  Boundary: docs/boundaries/sound-boundary.md
// §SNDDMA platform seam.

#include <xash3dpp/sound/device.hpp>

#include <algorithm>
#include <cstdint>

namespace xash::sound {

namespace {

// Negotiate caps from a request.  Both the null and sink devices accept the
// request verbatim (they impose no hardware constraint); a real backend would
// clamp to what the OS granted.
[[nodiscard]] DeviceCaps negotiate( const DeviceSpec &spec ) noexcept
{
    DeviceCaps caps;
    caps.speed         = spec.speed;
    caps.channels      = spec.channels;
    caps.width         = spec.width;
    // 0 request -> a nominal one-frame default so caps never advertise a
    //   zero-length buffer; a real backend reports the granted period size.
    caps.buffer_frames = spec.buffer_frames != 0 ? spec.buffer_frames : 1;
    return caps;
}

} // namespace

// ---------------------------------------------------------------------------
// NullDevice — opens successfully, consumes nothing (no-output).
// ---------------------------------------------------------------------------

Result<DeviceCaps> NullDevice::open( const DeviceSpec &spec ) noexcept
{
    open_ = true;
    return negotiate( spec );
}

void NullDevice::close() noexcept
{
    open_    = false;
    active_  = false;
    source_  = nullptr;
}

void NullDevice::set_active( bool active ) noexcept
{
    // Topology-symmetry Activate that legacy s_stub omits (SND-OQ-2): tracked
    // but inert — NullDevice never pulls from the source regardless.
    active_ = active;
}

void NullDevice::set_fill_source( IAudioFillSource *source ) noexcept
{
    source_ = source; // accepted for API symmetry; never invoked
}

// ---------------------------------------------------------------------------
// SinkDevice — virtually clocked; pump() pulls exactly n_frames on demand.
// ---------------------------------------------------------------------------

Result<DeviceCaps> SinkDevice::open( const DeviceSpec &spec ) noexcept
{
    open_          = true;
    frames_pumped_ = 0;
    return negotiate( spec );
}

void SinkDevice::close() noexcept
{
    open_   = false;
    active_ = false;
    source_ = nullptr;
    buffer_.clear();
}

void SinkDevice::set_active( bool active ) noexcept
{
    active_ = active;
}

void SinkDevice::set_fill_source( IAudioFillSource *source ) noexcept
{
    source_ = source;
}

std::span<const std::int16_t> SinkDevice::pump( std::size_t n_frames )
{
    // Interleaved stereo: two int16 samples per frame (SND-OQ-5).
    const std::size_t n_samples = n_frames * 2;
    buffer_.assign( n_samples, std::int16_t{ 0 } ); // zero-fill = silence baseline

    // Pull only when open, active, and a source is registered; otherwise the
    // buffer stays silent (documented no-source / inactive contract).
    if( open_ && active_ && source_ != nullptr && n_samples != 0 )
        source_->fill( std::span<std::int16_t>{ buffer_.data(), n_samples } );

    frames_pumped_ += n_frames;
    return std::span<const std::int16_t>{ buffer_.data(), buffer_.size() };
}

} // namespace xash::sound
