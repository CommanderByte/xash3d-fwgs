#pragma once
// xash3dpp — imagelib GoldSrc/Quake 8-bit palette machinery (private)
// Legacy reference: engine/common/imagelib/img_utils.c
//   palette_q1[768]/palette_hl[768] (the built-in colour tables),
//   Image_ComparePalette (Q1/HL classify), Image_SetPalette (the LUMP_*
//   rendermodes), Image_GetPaletteQ1/HL, Image_Copy8bitRGBA (index->RGBA
//   expansion + the fullbright/luma index >= 224 rule), Image_FindBestBlack.
//
// These are the shared helpers behind the indexed codecs (codec_mip.cpp today;
// codec_lmp/spr/mdl later). Free functions in namespace xash::imagelib, stateless
// (the legacy global `image.d_currentpal` scratch is gone — boundary H-1), so
// they are reentrant like the codecs that call them.
//
// @thread-safety: pure functions over caller-owned buffers + immutable static
// tables — safe to share across threads.

#include <xash3dpp/imagelib/image.hpp>  // Palette, Rgba

#include <cstddef>
#include <cstdint>
#include <span>

namespace xash::imagelib {

// ---------------------------------------------------------------------------
// Rendermode — mirrors legacy LUMP_* (imagelib.h) for palette expansion. Only
// the four modes an indexed codec builds from a raw table are modelled; the
// predefined-table modes (LUMP_QUAKE1/HALFLIFE) are the quake_palette() /
// halflife_palette() accessors below.
// ---------------------------------------------------------------------------

enum class Rendermode : std::uint8_t
{
    Normal,    // LUMP_NORMAL   — opaque colour, alpha 0xFF
    Masked,    // LUMP_MASKED   — index 255 fully transparent (1-bit alpha)
    Gradient,  // LUMP_GRADIENT — front-colour (index 255) gradient decal
    TexGamma,  // LUMP_TEXGAMMA — LUMP_NORMAL with texgamma applied (HL mips)
};

// Palette classification — mirrors legacy Image_ComparePalette / PAL_*.
enum class PaletteClass : std::int8_t
{
    Invalid  = -1,  // null / too-short palette
    Custom   = 0,   // matches neither built-in table
    Quake1,         // first 765 bytes match palette_q1
    HalfLife,       // first 765 bytes match palette_hl
};

// Fullbright/luma index threshold. Legacy detects a luma layer with `idx > 224`
// and Image_Copy8bitRGBA strips fullbright with `idx >= 224 ? black : idx` — the
// two use-sites differ by one, so callers pick the comparison explicitly.
inline constexpr std::uint8_t k_fullbright_index = 224;

// The 768-byte (256 * RGB) built-in palettes, transcribed byte-for-byte from
// img_utils.c. Spans over immutable file-static tables.
[[nodiscard]] std::span<const std::uint8_t> quake_palette_rgb() noexcept;
[[nodiscard]] std::span<const std::uint8_t> halflife_palette_rgb() noexcept;

// Classify a raw palette against the built-in tables (legacy compares the first
// 765 bytes, tolerating a changed final colour). `pal` shorter than 765 bytes or
// empty is PaletteClass::Invalid.
[[nodiscard]] PaletteClass classify_palette( std::span<const std::uint8_t> pal ) noexcept;

// Build a 256-entry RGBA Palette from a raw RGB table under `mode` (mirrors
// Image_SetPalette). Reads are bounds-clamped: a short `pal` yields zero bytes
// for the missing entries rather than faulting. Gradient uses pal[765..767] as
// the front colour with alpha == index; Masked forces entry 255 to {0,0,0,0}.
[[nodiscard]] Palette build_palette( std::span<const std::uint8_t> pal, Rendermode mode ) noexcept;

// The built-in Quake palette (LUMP_NORMAL, then index 255 forced transparent) —
// mirrors Image_GetPaletteQ1 (d_8toQ1table[255] = 0).
[[nodiscard]] Palette quake_palette() noexcept;
// The built-in Half-Life palette (LUMP_NORMAL) — mirrors Image_GetPaletteHL.
[[nodiscard]] Palette halflife_palette() noexcept;

// True if any pixel is a fullbright index ( > 224 ), i.e. the mip carries a
// luma/self-illum layer. When `exclude_255`, index 255 does not count (matches
// the Quake1-mip branch's `fin[i] > 224 && fin[i] != 255`; the HL-mip Quake1
// branch passes false).
[[nodiscard]] bool detect_luma( std::span<const std::byte> indices, bool exclude_255 ) noexcept;

// Index of the darkest palette colour (packed RGB < 32), or -1 when none is dark
// enough — legacy Image_FindBestBlack, the replacement colour for stripped
// fullbright pixels when a HL-palette mip carries luma.
[[nodiscard]] int find_best_black( const Palette &pal ) noexcept;

// True if the palette has any non-grey entry (legacy HAS_COLOR test in
// Image_Copy8bitRGBA: some entry with r != g or g != b).
[[nodiscard]] bool palette_has_color( const Palette &pal ) noexcept;

// Expand 8-bit indices to RGBA through `pal` (mirrors Image_Copy8bitRGBA):
// out[i*4 + 0..3] = pal[indices[i]] as R,G,B,A. Writes min(indices.size(),
// out.size()/4) texels — the codec sizes `out` at indices.size() * 4.
void expand_indexed_rgba( std::span<const std::byte> indices, const Palette &pal,
                          std::span<std::byte> out ) noexcept;

} // namespace xash::imagelib
