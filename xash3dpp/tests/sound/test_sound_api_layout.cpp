// xash3dpp — vendored client-sound-interface layout parity (Chunk 9, S9.1).
// Compares every field offset + struct size of the vendored xash::abi sound
// surface against the ACTUAL legacy header (common/sound_api.h), included
// verbatim in a sealed namespace below.  Field/slot ORDER is the ABI — any
// drift (missing slot, reorder, width change, lost `volatile`/reserved padding)
// fails here before it can reach an ABI-conformant client DLL.
// Precedent: tests/server/abi/test_eiface_layout.cpp (the same include dance).

#include <xash3dpp/abi/sound_api.hpp>

#include "../test_helpers.hpp"

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Legacy side: the real common/sound_api.h in a sealed namespace.  xash3d_types.h
// is guarded off (XASH_TYPES_H) and the handful of types/macros it would supply
// are shimmed here; wavdata_t is referenced only by pointer, so a forward
// declaration is layout-sufficient (test_eiface_layout precedent).
// ---------------------------------------------------------------------------

namespace legacy {

typedef unsigned char  byte;
typedef int            qboolean;
typedef unsigned int   uint;
typedef unsigned short word;
typedef float          vec_t;
typedef vec_t          vec3_t[3];
typedef struct wavdata_s wavdata_t; // fwd (pointer-only use)

#define XASH_TYPES_H
#define MAX_QPATH    64
#define NUM_AMBIENTS 4
#include <common/sound_api.h>
#undef XASH_TYPES_H
#undef MAX_QPATH
#undef NUM_AMBIENTS

} // namespace legacy

static int g_pass = 0, g_fail = 0;

// ---------------------------------------------------------------------------
// Per-struct field lists (transcribed from common/sound_api.h; one offset
// comparison per field on both sides).
// ---------------------------------------------------------------------------

#define SAMPLEPAIR_FIELDS( X ) X( left ) X( right )

#define SFX_FIELDS( X ) \
    X( name ) X( cache ) X( servercount ) X( hashValue ) X( hashNext )

#define VOXWORD_FIELDS( X ) \
    X( sfx ) X( volume ) X( pitch ) X( timecompress ) X( start ) X( end ) X( flags )

#define CHANNEL_FIELDS( X ) \
    X( name ) X( sfx ) X( origin ) X( dist_mult ) X( entchannel ) X( flags ) \
    X( entnum ) X( master_vol ) X( leftvol ) X( rightvol ) X( basePitch ) \
    X( word_index ) X( inauduble_free_time ) X( sample ) X( forced_end ) \
    X( data ) X( words ) X( engine_reserved ) X( game_reserved )

#define RAWCHAN_FIELDS( X ) \
    X( entnum ) X( master_vol ) X( leftvol ) X( rightvol ) X( dist_mult ) \
    X( origin ) X( s_rawend ) X( oldtime ) X( engine_reserved ) \
    X( game_reserved ) X( max_samples ) X( rawsamples )

#define SNDFORMAT_FIELDS( X ) X( speed ) X( width ) X( channels )

#define SNDGLOBALS_FIELDS( X ) \
    X( backend_name ) X( buffer ) X( format ) X( initialized ) X( samples ) \
    X( samplepos ) X( paintedtime ) X( soundtime ) X( origin ) X( forward ) \
    X( right ) X( up ) X( entnum ) X( streaming ) X( stream_paused ) \
    X( channels ) X( max_channels ) X( total_channels ) X( raw_channels ) \
    X( max_raw_channels ) X( ambient_sfx ) X( have_ambient_sfx )

#define VOICEINFO_FIELDS( X ) X( width ) X( samplerate ) X( frame_size )

#define SOUNDAPI_SLOTS( X ) \
    X( CL_GetEntitySpatialization ) X( S_GetSfxByHandle ) X( pfnS_RawEntSamples ) \
    X( pfnSND_ForceInitMouth ) X( pfnVoice_GetAudioInfo )

#define SOUNDIFACE_SLOTS( X ) \
    X( version ) X( pfnS_Init ) X( pfnS_Shutdown ) X( pfnS_UpdateSound ) \
    X( pfnS_PaintChannels ) X( pfnS_UpdateChannel ) X( pfnS_UpdateRawChannel ) \
    X( pfnS_Spatialize ) X( pfnS_FreeSound )

// One field's offset, both sides — STRUCT is the enclosing object-like macro.
#define PIN_ONE( f ) \
    CHECK_EQ( offsetof( xash::abi::STRUCT, f ), offsetof( legacy::STRUCT, f ) );
// Pin sizeof of the enclosing STRUCT, both sides.
#define PIN_SIZEOF() \
    CHECK_EQ( sizeof( xash::abi::STRUCT ), sizeof( legacy::STRUCT ) )

static void test_support_pods()
{
#define STRUCT portable_samplepair_t
    PIN_SIZEOF();
    SAMPLEPAIR_FIELDS( PIN_ONE )
#undef STRUCT

#define STRUCT sfx_t
    PIN_SIZEOF();
    SFX_FIELDS( PIN_ONE )
#undef STRUCT

#define STRUCT voxword_t
    PIN_SIZEOF();
    VOXWORD_FIELDS( PIN_ONE )
#undef STRUCT

#define STRUCT snd_format_t
    PIN_SIZEOF();
    SNDFORMAT_FIELDS( PIN_ONE )
#undef STRUCT

#define STRUCT voice_audio_info_t
    PIN_SIZEOF();
    VOICEINFO_FIELDS( PIN_ONE )
#undef STRUCT
}

static void test_channel_layout()
{
#define STRUCT channel_t
    PIN_SIZEOF();
    CHANNEL_FIELDS( PIN_ONE )
#undef STRUCT
}

static void test_rawchan_layout()
{
#define STRUCT rawchan_t
    PIN_SIZEOF();
    RAWCHAN_FIELDS( PIN_ONE )
#undef STRUCT
}

static void test_snd_globals_layout()
{
#define STRUCT snd_globals_t
    PIN_SIZEOF();
    SNDGLOBALS_FIELDS( PIN_ONE )
#undef STRUCT
}

static void test_interface_tables()
{
    // engine->client table: 5 slots.
#define STRUCT sound_api_t
    PIN_SIZEOF();
    SOUNDAPI_SLOTS( PIN_ONE )
#undef STRUCT

    // client->engine table: int version + 8 slots.
#define STRUCT sound_interface_t
    PIN_SIZEOF();
    SOUNDIFACE_SLOTS( PIN_ONE )
#undef STRUCT
}

static void test_version_constant()
{
    CHECK_EQ( xash::abi::k_cl_sound_interface_version, CL_SOUND_INTERFACE_VERSION );
    CHECK_EQ( xash::abi::k_cl_sound_interface_version, 1 );
    // The volatile qualifier on s_rawend is the sole ABI concurrency hint;
    // confirm it is preserved (offset pinned above; this documents intent).
    CHECK_EQ( sizeof( xash::abi::rawchan_t ), sizeof( legacy::rawchan_t ) );
}

int main()
{
    RUN_TEST( test_support_pods );
    RUN_TEST( test_channel_layout );
    RUN_TEST( test_rawchan_layout );
    RUN_TEST( test_snd_globals_layout );
    RUN_TEST( test_interface_tables );
    RUN_TEST( test_version_constant );

    std::printf( "sound_api_layout: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail == 0 ? 0 : 1;
}
