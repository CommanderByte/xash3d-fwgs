# xash3dpp — networking subsystem
# Transport (UDP / loopback), netchan, codec, delta encoder, satellites.
# Legacy reference: engine/common/net_ws.{c,h}, netchan.{c,h}, net_encode.c,
#                   net_buffer.{c,h}, net_chan.{c,h}.
#
# Satellite-component decisions (Q-11):
#   * HTTP downloader      → separate sibling target  xash3dpp_http (not here)
#   * Master-server list   → same target              master_list.cpp
#   * Async DNS resolver   → same target              (Layer 1+)
#   * Default protocol drv → same target              protocol_driver_goldsrc.cpp
#
# Compat policy (Q-12): per-client IProtocolDriver under
# include/xash3dpp/private/networking/; no engine-wide compat singleton.

# ---------------------------------------------------------------------------
# Link-time compression backend selection (Q-7 recurring pattern).
# Defaults to OFF for dedicated servers, ON otherwise.  Wire-frozen frame
# magic XASH_HEADER_COMPRESSEDPACKET (0xFFFFFFFD) is still decoded either way
# — the OFF path simply rejects compressed payloads.
# ---------------------------------------------------------------------------
if(XASH_DEDICATED)
    set(_xash_net_compression_default OFF)
else()
    set(_xash_net_compression_default ON)
endif()
option(XASH_NET_COMPRESSION "Enable bzip2/LZSS compression in xash3dpp_networking"
       ${_xash_net_compression_default})

if(XASH_NET_COMPRESSION)
    # LZSS is always compiled in; XASH_NET_COMPRESSION only gates which codec
    # adapter is wired into the protocol driver at runtime.
    # TODO(Layer 1 #10): add compress_bz2.cpp when bzip2 dependency is wired.
    set(XASH_NET_COMPRESSION_SRC compress_lzss.cpp)
else()
    set(XASH_NET_COMPRESSION_SRC compress_lzss.cpp compress_null.cpp)
endif()

add_library(xash3dpp_networking STATIC
    context.cpp
    address.cpp
    message_buf.cpp
    compat_xash.cpp
    compat_goldsrc.cpp
    lag_queue.cpp
    compress_lzss.cpp
    compress_null.cpp
    protocol_driver_goldsrc.cpp
    master_list.cpp
)

target_include_directories(xash3dpp_networking
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_networking PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_networking
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_memory
    PRIVATE xash3dpp_core
    PRIVATE xash3dpp_platform
)

if(XASH_NET_COMPRESSION)
    target_compile_definitions(xash3dpp_networking PRIVATE XASH_NET_COMPRESSION=1)
endif()
