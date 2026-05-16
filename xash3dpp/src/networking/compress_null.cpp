// xash3dpp — compression backend: null implementation
// Selected by XASH_NET_COMPRESSION=OFF (dedicated-server default).
// Boundary spec recurring pattern: link-time compat selection (Q-7).
//
// Both compress_bz2.cpp and compress_lzss.cpp will provide the same symbols
// with real implementations when XASH_NET_COMPRESSION=ON.  They land in
// Layer 4 alongside fragmenting work.

namespace xash::networking {

// TODO(Chunk 5): declare compress_bz2 / decompress_bz2 / compress_lzss /
// decompress_lzss entry points here as no-ops once the codec layer is ready.

} // namespace xash::networking
