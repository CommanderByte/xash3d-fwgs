// xash3dpp — `.sav` container + save-directory + SV_GetSaveComment + `.HL2`/
// `.HL3` + SAV-OQ-1 tests (Chunk 8, slice S8.5).
//
// Legacy reference: engine/server/sv_save.c (see each helper header for exact
// line cites).  Golden byte arrays are hand-derived from the documented wire
// rules (S8.1-S8.4 parity gates: field values inline, all-zero fields
// skipped, token table carries only NAMES, block header = post-skip actual
// field count) with the token-table slot assignments computed by simulating
// the documented hash (token_table.hpp hash_string) over the exact interning
// order — see the comment above each golden for its token->slot map.

#include <xash3dpp/private/save/client_state.hpp>
#include <xash3dpp/private/save/container_codec.hpp>
#include <xash3dpp/private/save/entity_patch.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/save_comment.hpp>
#include <xash3dpp/private/save/save_directory.hpp>

#include <xash3dpp/abi/eiface.hpp>
#include <xash3dpp/abi/entity_state.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/memory/memory.hpp>
#include <xash3dpp/utilities/string.hpp>

#include "../test_helpers.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace save = xash::save;
namespace mem  = xash::memory;
namespace abi  = xash::abi;
namespace fs_  = xash::filesystem;

static int g_pass = 0, g_fail = 0;

static mem::PoolHandle g_pool;

static std::byte b( int v ) noexcept { return static_cast<std::byte>( v & 0xFF ); }

static std::vector<std::byte> to_bytes( std::string_view s ) noexcept
{
    std::vector<std::byte> out( s.size() );
    for ( std::size_t i = 0; i < s.size(); ++i )
        out[i] = static_cast<std::byte>( s[i] );
    return out;
}

static bool bytes_eq( std::span<const std::byte> a, std::span<const std::byte> b_ ) noexcept
{
    return a.size() == b_.size() && ( a.empty() || std::memcmp( a.data(), b_.data(), a.size() ) == 0 );
}

// Finds the first occurrence of `needle` in `hay`; returns hay.size() (an
// out-of-range sentinel) if absent.  Used by the D2 payload-shape witness to
// locate a field record's text payload without hand-deriving the full
// token-slot layout.
static std::size_t find_bytes( std::span<const std::byte> hay, std::span<const std::byte> needle ) noexcept
{
    if ( needle.empty() || hay.size() < needle.size() )
        return hay.size();
    for ( std::size_t i = 0; i + needle.size() <= hay.size(); ++i )
        if ( bytes_eq( hay.subspan( i, needle.size() ), needle ) )
            return i;
    return hay.size();
}

// ===========================================================================
// 1. Container: GAME_HEADER golden (byte-exact, 180 bytes)
// ===========================================================================
//
// map_name="c1a0", comment="hi", 0 embedded files (mapCount DataEmpty-
// skipped).  token table = 8 slots.  Token interning order: GameHeader (block
// name), mapName, comment (mapCount skipped -> never interned).
// hash_string(name) % 8 + linear probe (no collisions): GameHeader->1,
// comment->2, mapName->3.
static void test_container_gameheader_golden()
{
    save::SavContainerParams params;
    params.map_name = "c1a0";
    params.comment  = "hi";

    auto buf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
    REQUIRE( buf != nullptr );

    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, nullptr, {}, *buf, image ).has_value() );

    const std::array<std::byte, 180> golden = {
        b( 0x4A ), b( 0x53 ), b( 0x41 ), b( 0x56 ), b( 0x71 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x80 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x08 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x20 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x47 ), b( 0x61 ), b( 0x6D ),
        b( 0x65 ), b( 0x48 ), b( 0x65 ), b( 0x61 ), b( 0x64 ), b( 0x65 ), b( 0x72 ), b( 0x00 ),
        b( 0x63 ), b( 0x6F ), b( 0x6D ), b( 0x6D ), b( 0x65 ), b( 0x6E ), b( 0x74 ), b( 0x00 ),
        b( 0x6D ), b( 0x61 ), b( 0x70 ), b( 0x4E ), b( 0x61 ), b( 0x6D ), b( 0x65 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x01 ), b( 0x00 ),
        b( 0x02 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x20 ), b( 0x00 ), b( 0x03 ), b( 0x00 ),
        b( 0x63 ), b( 0x31 ), b( 0x61 ), b( 0x30 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x50 ), b( 0x00 ), b( 0x02 ), b( 0x00 ), b( 0x68 ), b( 0x69 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    };

    REQUIRE( image.size() == golden.size() );
    for ( std::size_t i = 0; i < golden.size(); ++i )
        CHECK_EQ( image[i], golden[i] );
}

// ===========================================================================
// 2. Container write->read round-trip (memory-only) + DirectoryExtract
//    extension-blindness witness (an embedded record with an unrecognized
//    name/extension round-trips unchanged, same as any `.HL?` record).
// ===========================================================================
static void test_container_roundtrip_and_extension_blind()
{
    const std::vector<std::byte> hl1_data = to_bytes( "HL1-CONTENT-BYTES" );
    const std::vector<std::byte> foreign_data = to_bytes( "totally-unrecognized-payload" );

    std::array<save::EmbeddedFile, 2> files = { {
        { "map1.HL1", hl1_data },
        { "notes.txt", foreign_data }, // extension-blindness witness
    } };

    save::SavContainerParams params;
    params.map_name = "map1";
    params.comment  = "test save";

    auto wbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( wbuf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, nullptr, files, *wbuf, image ).has_value() );

    auto rbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( rbuf != nullptr );
    save::SavContainerResult result;
    REQUIRE( save::read_sav_container( image, nullptr, *rbuf, result ).has_value() );

    CHECK_STREQ( result.header.map_name, "map1" );
    CHECK_STREQ( result.header.comment, "test save" );
    CHECK_EQ( result.header.map_count, 2 );

    REQUIRE( result.records.size() == 2u );
    CHECK( result.records[0].name == "map1.HL1" );
    CHECK( bytes_eq( result.records[0].data, hl1_data ) );
    // The foreign-named record is extracted verbatim, exactly like a `.HL?`
    // record — extraction never inspects the embedded name/extension.
    CHECK( result.records[1].name == "notes.txt" );
    CHECK( bytes_eq( result.records[1].data, foreign_data ) );
}

// ===========================================================================
// 2b. Container: pfnSaveGlobalState/pfnRestoreGlobalState callback seam —
//     the 7-hazard verification pack found this gap: read_sav_container's
//     IRestoreGlobalState::restore_global_state call (container_codec.cpp:
//     191-194, the pre-spawn pfnRestoreGlobalState position, sv_save.c:1822)
//     had no test exercising (a) it firing exactly once, (b) the blob bytes
//     it receives round-tripping byte-exact from a writer-side
//     ISaveGlobalState, or (c) its ORDER relative to embedded-record
//     extraction.
// ===========================================================================

namespace
{
constexpr std::string_view k_global_marker_token = "GlobalMarker";

// Arbitrary non-trivial marker bytes (not all-zero/all-same, so an
// accidentally-uninitialized buffer can't satisfy the byte-compare below).
constexpr std::array<std::byte, 6> k_global_marker_blob = {
    std::byte{ 0xDE }, std::byte{ 0xAD }, std::byte{ 0xBE },
    std::byte{ 0xEF }, std::byte{ 0x01 }, std::byte{ 0x02 },
};

// Writer-side fake ISaveGlobalState: emits exactly ONE field record (token
// "GlobalMarker", payload = k_global_marker_blob) right after the GameHeader
// block — standing in for a real pfnSaveGlobalState producer.
struct FakeSaveGlobalState final : save::ISaveGlobalState
{
    int call_count = 0;

    save::Result<void>
    save_global_state( save::IFieldSink &sink, save::TokenTable &tokens ) noexcept override
    {
        ++call_count;
        auto tok = tokens.insert( k_global_marker_token );
        if ( !tok.has_value() )
            return std::unexpected( tok.error() );
        return sink.write_field_record( *tok, k_global_marker_blob );
    }
};

// Reader-side fake IRestoreGlobalState. Verifies:
//   (a) called exactly once                          -> call_count
//   (b) blob bytes round-trip byte-exact               -> captured_payload
//   (c) ORDER: fires strictly before record extraction -> records_size_at_call
//
// read_sav_container is single-shot: every observable side effect happens
// inside the one call, so nothing OUTSIDE that call can interleave with it
// to witness ordering directly, and the callback itself only receives
// `data`/`offset`/`tokens` — never a view onto `out`. To still get a real
// ordering witness (rather than trusting the source's lexical order), this
// fake is wired by the test, BEFORE calling read_sav_container, with a
// pointer to the SAME SavContainerResult::records vector the call will
// eventually populate. Per container_codec.cpp:191-207, the
// restore_global_state call happens strictly before `out.records.clear()`
// and the extraction loop that fills it, so a snapshot of
// `records_ptr->size()` taken DURING this callback reads 0 (records starts
// out default-empty and extraction hasn't run yet) precisely because
// extraction has not happened yet; had the engine ever run extraction FIRST,
// this snapshot would instead read `map_count` (nonzero — the test embeds
// one file) — which the test separately confirms IS the size once
// read_sav_container returns. That before/after size contrast (0 during the
// callback vs. map_count after return) is the strongest ordering evidence
// read_sav_container's signature admits.
struct FakeRestoreGlobalState final : save::IRestoreGlobalState
{
    int                     call_count           = 0;
    std::vector<std::byte>  captured_payload;
    std::size_t             records_size_at_call = static_cast<std::size_t>( -1 );
    const std::vector<save::ExtractedRecord> *records_ptr = nullptr; // @lifetime: caller — wired before read_sav_container runs.

    save::Result<void>
    restore_global_state( std::span<const std::byte> data, std::size_t &offset,
                          const save::TokenTable &tokens ) noexcept override
    {
        ++call_count;
        if ( records_ptr != nullptr )
            records_size_at_call = records_ptr->size();

        auto rec = save::next_field_record( data, offset );
        if ( !rec.has_value() )
            return std::unexpected( rec.error() );
        if ( tokens.token_at( rec->token_idx ) != k_global_marker_token )
            return std::unexpected( save::SaveError::BadFieldRecord );

        captured_payload.assign( rec->payload.begin(), rec->payload.end() );
        return {};
    }
};
} // namespace

static void test_container_restore_global_state_callback()
{
    const std::vector<std::byte> hl1_data = to_bytes( "GLOBALCB-LEVEL-BYTES" );
    std::array<save::EmbeddedFile, 1> files = { {
        { "map1.HL1", hl1_data },
    } };

    save::SavContainerParams params;
    params.map_name = "map1";
    params.comment  = "global state callback test";

    FakeSaveGlobalState writer_fake;

    auto wbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( wbuf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, &writer_fake, files, *wbuf, image ).has_value() );
    CHECK_EQ( writer_fake.call_count, 1 );

    FakeRestoreGlobalState reader_fake;
    save::SavContainerResult result;
    reader_fake.records_ptr = &result.records; // wire BEFORE the call — see class doc above.

    auto rbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( rbuf != nullptr );
    REQUIRE( save::read_sav_container( image, &reader_fake, *rbuf, result ).has_value() );

    // (a) called exactly once.
    CHECK_EQ( reader_fake.call_count, 1 );

    // (b) blob round-trip: the writer-side fake's payload survives byte-exact
    // through write_sav_container -> read_sav_container's GameHeader-relative
    // cursor hand-off.
    REQUIRE( reader_fake.captured_payload.size() == k_global_marker_blob.size() );
    CHECK( bytes_eq( reader_fake.captured_payload, std::span<const std::byte>( k_global_marker_blob ) ) );

    // (c) ORDER: at call time, no records had been extracted yet (0); after
    // read_sav_container returns, exactly `map_count` (1) have — proof the
    // callback fired strictly before record extraction ran.
    CHECK_EQ( reader_fake.records_size_at_call, 0u );
    REQUIRE( result.records.size() == 1u );
    CHECK( result.records[0].name == "map1.HL1" );
    CHECK( bytes_eq( result.records[0].data, hl1_data ) );
}

// ===========================================================================
// 3. SAV-OQ-1 — .HLX foreign-block round-trip + the self-describing header
//    shape + ClearSaveDir's `*.HL?` glob covering `.HLX`.
// ===========================================================================
static void test_hlx_door()
{
    // A hand-built .HLX side-block payload: the self-describing header
    // (magic/version/size) + 4 bytes of arbitrary content, matching the
    // "reserved-namespace skip logic" shape (format.hpp HlxSideBlockHeader).
    std::vector<std::byte> hlx_payload;
    auto push_i32 = [&]( std::int32_t v ) {
        const auto u = static_cast<std::uint32_t>( v );
        hlx_payload.push_back( static_cast<std::byte>( u & 0xFF ) );
        hlx_payload.push_back( static_cast<std::byte>( ( u >> 8 ) & 0xFF ) );
        hlx_payload.push_back( static_cast<std::byte>( ( u >> 16 ) & 0xFF ) );
        hlx_payload.push_back( static_cast<std::byte>( ( u >> 24 ) & 0xFF ) );
    };
    push_i32( save::k_hlx_side_block_magic );
    push_i32( save::k_hlx_side_block_version );
    push_i32( 4 ); // 4 payload bytes follow
    hlx_payload.push_back( b( 'D' ) );
    hlx_payload.push_back( b( 'A' ) );
    hlx_payload.push_back( b( 'T' ) );
    hlx_payload.push_back( b( 'A' ) );

    const std::vector<std::byte> hl1_data = to_bytes( "legacy-level-bytes" );

    std::array<save::EmbeddedFile, 2> files = { {
        { "map1.HL1", hl1_data },
        { "map1.HLX", hlx_payload }, // NO producer ships this in real saves — door-keep only.
    } };

    save::SavContainerParams params;
    params.map_name = "map1";
    params.comment  = "hlx test";

    auto wbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( wbuf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, nullptr, files, *wbuf, image ).has_value() );

    // A build WITHOUT any SAV-OQ-1-specific awareness still loads the WHOLE
    // save — the container reader has no extension special-casing.
    auto rbuf = save::create_save_buffer( g_pool, 4096, 16, 0.0f );
    REQUIRE( rbuf != nullptr );
    save::SavContainerResult result;
    REQUIRE( save::read_sav_container( image, nullptr, *rbuf, result ).has_value() );
    REQUIRE( result.records.size() == 2u );
    CHECK( result.records[0].name == "map1.HL1" );
    CHECK( bytes_eq( result.records[0].data, hl1_data ) );
    CHECK( result.records[1].name == "map1.HLX" );
    CHECK( bytes_eq( result.records[1].data, hlx_payload ) );

    // The self-describing header decodes cleanly for a reader that DOES know
    // about `.HLX` blocks.
    auto hdr = save::parse_hlx_header( result.records[1].data );
    REQUIRE( hdr.has_value() );
    CHECK_EQ( hdr->magic, save::k_hlx_side_block_magic );
    CHECK_EQ( hdr->version, save::k_hlx_side_block_version );
    CHECK_EQ( hdr->size, 4 );

    // ClearSaveDir's `*.HL?` glob (single-char wildcard) covers `.HLX` but
    // not a 4-character (or longer) suffix.
    CHECK( ::xash::utilities::match_pattern( "map1.HLX", "*.HL?", true ) );
    CHECK( ::xash::utilities::match_pattern( "map1.HL1", "*.HL?", true ) );
    CHECK( ::xash::utilities::match_pattern( "map1.HL2", "*.HL?", true ) );
    CHECK( !::xash::utilities::match_pattern( "map1.HLXX", "*.HL?", true ) );
    CHECK( !::xash::utilities::match_pattern( "map1.txt", "*.HL?", true ) );
}

// ===========================================================================
// 4. .HL2 — empty golden (null capabilities, no static entities)
// ===========================================================================
//
// Every SAVE_CLIENT field is DataEmpty (all zero) -> the ClientHeader block
// carries actualCount == 0 -> the body past the preamble+tokens is just the
// 8-byte block-header record.  token table = 4 slots; ClientHeader->slot 3
// (hash_string("ClientHeader") % 4, no collision).
static void test_client_state_empty_golden()
{
    save::ClientStateParams params; // every field/pointer at its default (0 / nullptr)

    auto buf = save::create_save_buffer( g_pool, 1024, 4, 0.0f );
    REQUIRE( buf != nullptr );

    std::vector<std::byte> image;
    REQUIRE( save::write_client_state( params, *buf, image ).has_value() );

    const std::array<std::byte, 44> golden = {
        b( 0x4A ), b( 0x53 ), b( 0x41 ), b( 0x56 ), b( 0x67 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x08 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
        b( 0x10 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x43 ),
        b( 0x6C ), b( 0x69 ), b( 0x65 ), b( 0x6E ), b( 0x74 ), b( 0x48 ), b( 0x65 ), b( 0x61 ),
        b( 0x64 ), b( 0x65 ), b( 0x72 ), b( 0x00 ), b( 0x04 ), b( 0x00 ), b( 0x03 ), b( 0x00 ),
        b( 0x00 ), b( 0x00 ), b( 0x00 ), b( 0x00 ),
    };

    REQUIRE( image.size() == golden.size() );
    for ( std::size_t i = 0; i < golden.size(); ++i )
        CHECK_EQ( image[i], golden[i] );

    // Round-trip: an all-empty LoadedClientState.
    auto rbuf = save::create_save_buffer( g_pool, 1024, 4, 0.0f );
    REQUIRE( rbuf != nullptr );
    save::LoadedClientState loaded;
    REQUIRE( save::read_client_state( image, *rbuf, loaded ).has_value() );
    CHECK_EQ( loaded.header.decal_count, 0 );
    CHECK_EQ( loaded.header.entity_count, 0 );
    CHECK_EQ( loaded.header.sound_count, 0 );
    CHECK( loaded.decals.empty() );
    CHECK( loaded.static_entities.empty() );
    CHECK( loaded.sounds.empty() );
}

// ===========================================================================
// 5. .HL2 — functional round-trip with populated decal/static/sound lists
//    via the OQ-4 capability interfaces.
// ===========================================================================

namespace
{
struct FakeDecalProvider final : save::IDecalListProvider
{
    save::SaveDecalEntry entry{};
    std::span<const save::SaveDecalEntry> decals() noexcept override
    {
        return std::span<const save::SaveDecalEntry>( &entry, 1 );
    }
};

struct FakeSoundProvider final : save::IDynamicSoundsProvider
{
    save::SaveSoundEntry entry{};
    std::span<const save::SaveSoundEntry> dynamic_sounds() noexcept override
    {
        return std::span<const save::SaveSoundEntry>( &entry, 1 );
    }
};

struct FakeMusicProvider final : save::IMusicStateProvider
{
    save::MusicState state{};
    save::MusicState music_state() noexcept override { return state; }
};
} // namespace

static void test_client_state_populated_roundtrip()
{
    FakeDecalProvider decal_provider;
    std::memcpy( decal_provider.entry.name, "{splat1", 7 );
    decal_provider.entry.position[0] = 10.0f;
    decal_provider.entry.position[1] = 20.0f;
    decal_provider.entry.position[2] = 30.0f;
    decal_provider.entry.entity_index = 5;
    decal_provider.entry.depth        = 2;
    decal_provider.entry.flags        = 1;
    decal_provider.entry.scale        = 1.5f;

    FakeSoundProvider sound_provider;
    std::memcpy( sound_provider.entry.name, "ambience/hum.wav", 16 );
    sound_provider.entry.entnum      = 3;
    sound_provider.entry.volume      = 0.75f;
    sound_provider.entry.attenuation = 1.0f;
    sound_provider.entry.looping     = 1;
    sound_provider.entry.channel     = 2;

    FakeMusicProvider music_provider;
    music_provider.state.intro_track    = "track01";
    music_provider.state.main_track     = "track02";
    music_provider.state.track_position = 12345;

    // D2: `messagenum` (FIELD_MODELNAME) is a companion-TEXT field — the wire
    // value is `model_name`, not a raw copy of the in-struct handle (which is
    // left untouched, verified below).
    save::StaticEntityEntry static_entry;
    static_entry.state.origin[0] = 1.0f;
    static_entry.state.origin[1] = 2.0f;
    static_entry.state.origin[2] = 3.0f;
    static_entry.state.sequence  = 4;
    static_entry.state.frame     = 5.5f;
    static_entry.state.colormap  = 6;
    static_entry.model_name      = "models/w_static.mdl";
    static_entry.state.movetype  = 7;
    static_entry.state.animtime  = 8.0f;
    std::array<save::StaticEntityEntry, 1> statics = { static_entry };

    save::ClientStateParams params;
    params.view_entity    = 9;
    params.wateralpha     = 0.5f;
    params.wateramp       = 0.25f;
    params.static_entities = statics;
    params.decal_provider = &decal_provider;
    params.sound_provider = &sound_provider;
    params.music_provider = &music_provider;

    auto wbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( wbuf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_client_state( params, *wbuf, image ).has_value() );

    auto rbuf = save::create_save_buffer( g_pool, 4096, 32, 0.0f );
    REQUIRE( rbuf != nullptr );
    save::LoadedClientState loaded;
    REQUIRE( save::read_client_state( image, *rbuf, loaded ).has_value() );

    CHECK_EQ( loaded.header.viewentity, 9 );
    CHECK( loaded.header.wateralpha == 0.5f );
    CHECK( loaded.header.wateramp == 0.25f );

    REQUIRE( loaded.decals.size() == 1u );
    CHECK( loaded.decals[0].position[0] == 10.0f );
    CHECK( loaded.decals[0].position[2] == 30.0f );
    CHECK_EQ( loaded.decals[0].entity_index, 5 );
    CHECK_EQ( static_cast<int>( loaded.decals[0].depth ), 2 );
    CHECK( loaded.decals[0].scale == 1.5f );

    REQUIRE( loaded.sounds.size() == 1u );
    CHECK_EQ( loaded.sounds[0].entnum, 3 );
    CHECK( loaded.sounds[0].volume == 0.75f );
    CHECK_EQ( loaded.sounds[0].looping, 1 );

    REQUIRE( loaded.static_entities.size() == 1u );
    CHECK( loaded.static_entities[0].state.origin[0] == 1.0f );
    CHECK_EQ( loaded.static_entities[0].state.sequence, 4 );
    CHECK( loaded.static_entities[0].state.frame == 5.5f );
    CHECK_EQ( loaded.static_entities[0].model_name, std::string( "models/w_static.mdl" ) );
    CHECK_EQ( loaded.static_entities[0].state.messagenum, 0 ); // in-struct handle untouched (D2)
    CHECK_EQ( loaded.static_entities[0].state.movetype, 7 );
    CHECK( loaded.static_entities[0].state.animtime == 8.0f ); // time_basis 0.0f -> unchanged

    CHECK_STREQ( loaded.header.intro_track, "track01" );
    CHECK_STREQ( loaded.header.main_track, "track02" );
    CHECK_EQ( loaded.header.track_position, 12345 );
}

// ===========================================================================
// 5b. .HL2 STATICENTITY — D2: `messagenum` (FIELD_MODELNAME) carries the
//     model-name TEXT on the wire (strlen+1), not a raw 4-byte copy of the
//     in-struct handle — mirrors S8.2's ETABLE classname pattern
//     (descriptor_codec.hpp FieldTextBinding).
// ===========================================================================

namespace
{
// Hand-assembles a minimal `.HL2` image (ClientHeader.entityCount=1 + one
// STATICENTITY block whose ONLY field is "messagenum") using nothing but the
// LOW-LEVEL primitives (SaveBuffer/TokenTable/write_block_header/IFieldSink)
// — NEVER write_descriptor_block's text-field branch under test.  This is an
// INDEPENDENT witness that read_client_state correctly decodes a text-shaped
// record from an external/legacy-shape producer, not merely a round-trip of
// this codec's own writer.  `messagenum_payload` is the raw field-record
// payload bytes to use (a well-formed TEXT+NUL, or a deliberately malformed
// one for the BadFieldRecord witness).
std::vector<std::byte>
hand_build_hl2_one_static( mem::PoolHandle pool, std::span<const std::byte> messagenum_payload )
{
    auto buf = save::create_save_buffer( pool, 1024, 8, 0.0f );
    if ( !buf )
        return {};
    buf->reset();

    save::SaveBufferSink sink( *buf );
    save::TokenTable    &tokens = buf->tokens();

    // ClientHeader block: entityCount = 1 (every other SAVE_CLIENT field is
    // DataEmpty-zero, so this is the block's only field).
    if ( !save::write_block_header( sink, tokens, "ClientHeader", 1 ).has_value() )
        return {};
    {
        auto tok = tokens.insert( "entityCount" );
        if ( !tok.has_value() )
            return {};
        const std::array<std::byte, 4> one = { b( 1 ), b( 0 ), b( 0 ), b( 0 ) };
        if ( !sink.write_field_record( *tok, one ).has_value() )
            return {};
    }

    // STATICENTITY block: messagenum = the caller-supplied payload bytes.
    if ( !save::write_block_header( sink, tokens, "STATICENTITY", 1 ).has_value() )
        return {};
    {
        auto tok = tokens.insert( "messagenum" );
        if ( !tok.has_value() )
            return {};
        if ( !sink.write_field_record( *tok, messagenum_payload ).has_value() )
            return {};
    }

    const std::size_t      data_size  = buf->size();
    const std::size_t      token_size = tokens.flattened_size();
    std::vector<std::byte> token_blob( token_size );
    if ( !tokens.flatten( token_blob ).has_value() )
        return {};

    std::vector<std::byte> image;
    auto                   push_i32 = [&]( std::int32_t v ) {
        const auto u = static_cast<std::uint32_t>( v );
        image.push_back( b( static_cast<int>( u & 0xFF ) ) );
        image.push_back( b( static_cast<int>( ( u >> 8 ) & 0xFF ) ) );
        image.push_back( b( static_cast<int>( ( u >> 16 ) & 0xFF ) ) );
        image.push_back( b( static_cast<int>( ( u >> 24 ) & 0xFF ) ) );
    };
    push_i32( save::k_savegame_magic );
    push_i32( save::k_client_savegame_version );
    push_i32( static_cast<std::int32_t>( data_size ) );
    push_i32( static_cast<std::int32_t>( tokens.token_count() ) );
    push_i32( static_cast<std::int32_t>( token_size ) );
    image.insert( image.end(), token_blob.begin(), token_blob.end() );
    const std::span<const std::byte> data_reg = buf->data();
    image.insert( image.end(), data_reg.begin(), data_reg.end() );
    return image;
}
} // namespace

static void test_static_entity_model_name_text()
{
    // --- Payload-shape witness: an otherwise-all-zero entity_state_t with
    //     only model_name set produces exactly ONE STATICENTITY field record
    //     ("messagenum"), whose payload is the TEXT + NUL (size = strlen+1 =
    //     7 for a 6-char name) — NOT a 4-byte raw copy. ---
    {
        save::StaticEntityEntry entry;
        entry.model_name = "m1.mdl"; // strlen 6 -> wire size 7, disambiguates from a raw 4-byte copy

        save::ClientStateParams params;
        params.static_entities = std::span<const save::StaticEntityEntry>( &entry, 1 );

        auto buf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( buf != nullptr );
        std::vector<std::byte> image;
        REQUIRE( save::write_client_state( params, *buf, image ).has_value() );

        std::vector<std::byte> text_and_nul = to_bytes( "m1.mdl" );
        text_and_nul.push_back( b( 0 ) );

        const std::size_t idx = find_bytes( image, text_and_nul );
        REQUIRE( idx < image.size() ); // the text+NUL payload is present verbatim
        REQUIRE( idx >= 4u );
        // Record layout (field_sink.hpp): short size, short token_idx,
        // payload[size].  The 2-byte little-endian SIZE field sits 4 bytes
        // before the payload (token_idx occupies the 2 bytes right before
        // it): size == 7 (strlen+1), never 4.
        CHECK_EQ( image[idx - 4], b( 7 ) );
        CHECK_EQ( image[idx - 3], b( 0 ) );

        // Round-trip sanity: decodes back to the same text; the in-struct
        // handle (messagenum int) is untouched (stays 0).
        auto rbuf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( rbuf != nullptr );
        save::LoadedClientState loaded;
        REQUIRE( save::read_client_state( image, *rbuf, loaded ).has_value() );
        REQUIRE( loaded.static_entities.size() == 1u );
        CHECK_EQ( loaded.static_entities[0].model_name, std::string( "m1.mdl" ) );
        CHECK_EQ( loaded.static_entities[0].state.messagenum, 0 );
    }

    // --- Independent read-side witness: a hand-built legacy-shape record
    //     (see hand_build_hl2_one_static above) reads correctly. ---
    {
        std::vector<std::byte> payload = to_bytes( "legacy.mdl" );
        payload.push_back( b( 0 ) ); // trailing NUL

        const std::vector<std::byte> image = hand_build_hl2_one_static( g_pool, payload );
        REQUIRE( !image.empty() );

        auto rbuf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( rbuf != nullptr );
        save::LoadedClientState loaded;
        REQUIRE( save::read_client_state( image, *rbuf, loaded ).has_value() );
        REQUIRE( loaded.static_entities.size() == 1u );
        CHECK_EQ( loaded.static_entities[0].model_name, std::string( "legacy.mdl" ) );
        CHECK_EQ( loaded.static_entities[0].state.messagenum, 0 );
    }

    // --- Empty model name -> field skipped (DataEmpty; the block header's
    //     field count omits it, and the field name is never interned). ---
    {
        save::StaticEntityEntry empty_entry; // model_name stays "" (default)
        save::ClientStateParams eparams;
        eparams.static_entities = std::span<const save::StaticEntityEntry>( &empty_entry, 1 );

        auto ebuf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( ebuf != nullptr );
        std::vector<std::byte> eimage;
        REQUIRE( save::write_client_state( eparams, *ebuf, eimage ).has_value() );

        auto erbuf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( erbuf != nullptr );
        save::LoadedClientState eloaded;
        REQUIRE( save::read_client_state( eimage, *erbuf, eloaded ).has_value() );
        REQUIRE( eloaded.static_entities.size() == 1u );
        CHECK( eloaded.static_entities[0].model_name.empty() );
    }

    // --- Missing-NUL -> BadFieldRecord (the read side rejects a text-family
    //     payload that does not end in a NUL terminator). ---
    {
        const std::vector<std::byte> payload = to_bytes( "nonul" ); // deliberately NO trailing NUL

        const std::vector<std::byte> image = hand_build_hl2_one_static( g_pool, payload );
        REQUIRE( !image.empty() );

        auto rbuf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
        REQUIRE( rbuf != nullptr );
        save::LoadedClientState loaded;
        auto r = save::read_client_state( image, *rbuf, loaded );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::BadFieldRecord );
    }
}

// ===========================================================================
// 6. .HL3 entity patch — round-trip + the plain-assignment clobber quirk +
//    out-of-range index rejection.
// ===========================================================================
static void test_entity_patch_roundtrip()
{
    save::EntityTable table;
    table.init( 4 );
    table.row( 1 ).flags |= static_cast<int>( abi::k_fenttable_removed );
    table.row( 3 ).flags |= static_cast<int>( abi::k_fenttable_removed );

    const std::vector<std::byte> image = save::write_entity_patch( table );

    auto indices = save::read_entity_patch( image );
    REQUIRE( indices.has_value() );
    REQUIRE( indices->size() == 2u );
    CHECK_EQ( ( *indices )[0], 1 );
    CHECK_EQ( ( *indices )[1], 3 );

    save::EntityTable fresh;
    fresh.init( 4 );
    // Pre-set an UNRELATED flag bit on row 1 to witness the plain-assignment
    // quirk (sv_save.c:1069 — `pTable[entityId].flags = FENTTABLE_REMOVED`,
    // NOT SetBits): applying the patch must CLOBBER it, not OR it in.
    fresh.row( 1 ).flags = static_cast<int>( abi::k_fenttable_player );

    REQUIRE( save::apply_entity_patch( fresh, *indices ).has_value() );
    CHECK_EQ( static_cast<unsigned>( fresh.row( 1 ).flags ), abi::k_fenttable_removed );
    CHECK_EQ( static_cast<unsigned>( fresh.row( 3 ).flags ), abi::k_fenttable_removed );
    CHECK_EQ( fresh.row( 0 ).flags, 0 );
    CHECK_EQ( fresh.row( 2 ).flags, 0 );

    // Out-of-range index -> reject-gracefully (deviation from legacy's
    // unchecked pTable[entityId] write).
    const std::array<std::int32_t, 1> bad = { 100 };
    auto                              r   = save::apply_entity_patch( fresh, bad );
    CHECK( !r.has_value() );
    CHECK( r.error() == save::SaveError::CorruptHeader );

    // Truncated .HL3 image -> TruncatedBlock.
    {
        std::vector<std::byte> bad_image = { b( 0x05 ), b( 0x00 ), b( 0x00 ), b( 0x00 ) }; // count=5, no indices follow
        auto                   bad_r     = save::read_entity_patch( bad_image );
        CHECK( !bad_r.has_value() );
        CHECK( bad_r.error() == save::SaveError::TruncatedBlock );
    }
}

// ===========================================================================
// Temp-directory fixture for save_directory + file-backed I/O tests (mirrors
// tests/filesystem/test_filesystem.cpp's precedent).
// ===========================================================================

static std::filesystem::path g_testdir;

static void setup_testdir()
{
    g_testdir = std::filesystem::temp_directory_path() / "xash3dpp_save_test";
    std::filesystem::remove_all( g_testdir );
    std::filesystem::create_directories( g_testdir / "game" / "save" );
}

static void teardown_testdir()
{
    std::filesystem::remove_all( g_testdir );
}

static std::string rootdir() { return g_testdir.string(); }
static std::string gamedir() { return ( g_testdir / "game" ).string(); }

static void write_testfile( std::string_view rel, std::string_view content )
{
    std::ofstream f( g_testdir / "game" / std::filesystem::u8path( std::string( rel ) ), std::ios::binary );
    f << content;
}

static bool testfile_exists( std::string_view rel )
{
    return std::filesystem::exists( g_testdir / "game" / std::filesystem::u8path( std::string( rel ) ) );
}

static std::string read_testfile( std::string_view rel )
{
    std::ifstream f( g_testdir / "game" / std::filesystem::u8path( std::string( rel ) ), std::ios::binary );
    std::string   s( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
    return s;
}

// ===========================================================================
// 7. age_save_list — the exact rotation-rename sequence.
// ===========================================================================
static void test_age_save_list_sequence()
{
    setup_testdir();

    write_testfile( "save/quick.sav", "Q0" );
    write_testfile( "save/quick01.sav", "Q1" );
    write_testfile( "save/quick02.sav", "Q2" );

    fs_::Filesystem fs;
    REQUIRE( fs.init( rootdir(), "valve", "game" ) );
    fs.add_game_directory( gamedir(), fs_::SearchPathFlags::GameDir );

    save::age_save_list( fs, "quick", 2 ); // GI->quicksave_aged_count == 2

    // Oldest slot (quick02) content is gone; the chain shifted up by one;
    // the unnumbered stem is now free.
    CHECK( !testfile_exists( "save/quick.sav" ) );
    CHECK( testfile_exists( "save/quick01.sav" ) );
    CHECK( testfile_exists( "save/quick02.sav" ) );
    CHECK_EQ( read_testfile( "save/quick01.sav" ), std::string( "Q0" ) );
    CHECK_EQ( read_testfile( "save/quick02.sav" ), std::string( "Q1" ) );

    fs.shutdown();
    teardown_testdir();
}

// ===========================================================================
// 8. directory_count / clear_save_dir (the `*.HL?` glob, incl. `.HLX`).
// ===========================================================================
static void test_directory_count_and_clear_save_dir()
{
    setup_testdir();

    write_testfile( "save/level.HL1", "a" );
    write_testfile( "save/level.HL2", "b" );
    write_testfile( "save/level.HLX", "c" ); // matches the *.HL? glob too
    write_testfile( "save/quick.sav", "d" ); // must survive clear_save_dir

    fs_::Filesystem fs;
    REQUIRE( fs.init( rootdir(), "valve", "game" ) );
    fs.add_game_directory( gamedir(), fs_::SearchPathFlags::GameDir );

    CHECK_EQ( save::directory_count( fs, "save/*.HL?" ), 3u );

    save::clear_save_dir( fs );

    CHECK( !testfile_exists( "save/level.HL1" ) );
    CHECK( !testfile_exists( "save/level.HL2" ) );
    CHECK( !testfile_exists( "save/level.HLX" ) );
    CHECK( testfile_exists( "save/quick.sav" ) ); // untouched — not `*.HL?`

    fs.shutdown();
    teardown_testdir();
}

// ===========================================================================
// 9. select_latest_save — pure ordering (strictly-greatest wins, tie keeps
//    first, entries without a time are skipped).
// ===========================================================================
static void test_select_latest_save_ordering()
{
    const auto now = std::filesystem::file_time_type::clock::now();

    std::array<save::SaveFileTimeEntry, 4> entries = { {
        { "save/a.sav", now },
        { "save/b.sav", now + std::chrono::seconds( 10 ) }, // newest
        { "save/c.sav", std::nullopt },                     // skipped (no time)
        { "save/d.sav", now + std::chrono::seconds( 10 ) }, // tie with b -> b (first seen) wins
    } };

    auto best = save::select_latest_save( entries );
    REQUIRE( best.has_value() );
    CHECK( *best == "save/b.sav" );

    // All-nullopt -> no result.
    std::array<save::SaveFileTimeEntry, 2> none = { { { "x", std::nullopt }, { "y", std::nullopt } } };
    CHECK( !save::select_latest_save( none ).has_value() );
}

static void test_latest_save_file_backed()
{
    setup_testdir();

    write_testfile( "save/old.sav", "old" );
    write_testfile( "save/new.sav", "new" );

    // Force a detectable mtime ordering (filesystem mtime resolution can be
    // coarse) via std::filesystem::last_write_time.
    const auto base = std::filesystem::last_write_time( g_testdir / "game" / "save" / "old.sav" );
    std::filesystem::last_write_time( g_testdir / "game" / "save" / "new.sav",
                                      base + std::chrono::seconds( 60 ) );

    fs_::Filesystem fs;
    REQUIRE( fs.init( rootdir(), "valve", "game" ) );
    fs.add_game_directory( gamedir(), fs_::SearchPathFlags::GameDir );

    auto latest = save::latest_save( fs );
    REQUIRE( latest.has_value() );
    CHECK( latest->find( "new.sav" ) != std::string::npos );

    fs.shutdown();
    teardown_testdir();
}

// ===========================================================================
// 10. save_comment — version-gate cases (no filesystem needed; each gate
//     returns before touching the rest of the image).
// ===========================================================================
static void test_save_comment_version_gates()
{
    auto make_header = [&]( std::int32_t magic, std::int32_t version ) {
        std::vector<std::byte> img( 8 );
        const auto             m = static_cast<std::uint32_t>( magic );
        const auto             v = static_cast<std::uint32_t>( version );
        img[0] = b( static_cast<int>( m & 0xFF ) );
        img[1] = b( static_cast<int>( ( m >> 8 ) & 0xFF ) );
        img[2] = b( static_cast<int>( ( m >> 16 ) & 0xFF ) );
        img[3] = b( static_cast<int>( ( m >> 24 ) & 0xFF ) );
        img[4] = b( static_cast<int>( v & 0xFF ) );
        img[5] = b( static_cast<int>( ( v >> 8 ) & 0xFF ) );
        img[6] = b( static_cast<int>( ( v >> 16 ) & 0xFF ) );
        img[7] = b( static_cast<int>( ( v >> 24 ) & 0xFF ) );
        return img;
    };

    // Bad magic entirely -> BadMagic (SaveError, not a status).
    {
        auto img = make_header( 0x11223344, save::k_savegame_version );
        auto r   = save::save_comment( img, "save/x.sav", std::nullopt );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::BadMagic );
    }
    // 0x0065 special-case old version.
    {
        auto img = make_header( save::k_savegame_magic, 0x0065 );
        auto r   = save::save_comment( img, "save/x.sav", std::nullopt );
        REQUIRE( r.has_value() );
        CHECK( r->status == save::SaveCommentStatus::OldVersionUnsupported );
    }
    // tag < SAVEGAME_VERSION.
    {
        auto img = make_header( save::k_savegame_magic, save::k_savegame_version - 1 );
        auto r   = save::save_comment( img, "save/x.sav", std::nullopt );
        REQUIRE( r.has_value() );
        CHECK( r->status == save::SaveCommentStatus::OldVersion );
    }
    // tag > SAVEGAME_VERSION.
    {
        auto img = make_header( save::k_savegame_magic, save::k_savegame_version + 1 );
        auto r   = save::save_comment( img, "save/x.sav", std::nullopt );
        REQUIRE( r.has_value() );
        CHECK( r->status == save::SaveCommentStatus::InvalidVersion );
    }
}

// ===========================================================================
// 11. save_comment — full parse over a hand-built (via write_sav_container)
//     `.sav` image, incl. the quick/autosave comment-prefix classification.
// ===========================================================================
static void test_save_comment_full_parse()
{
    save::SavContainerParams params;
    params.map_name = "c1a0";
    params.comment  = "a nice save";

    auto buf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
    REQUIRE( buf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, nullptr, {}, *buf, image ).has_value() );

    // "quick" substring in the save name -> the [quick] comment prefix.
    auto r = save::save_comment( image, "save/quick01.sav", std::nullopt );
    REQUIRE( r.has_value() );
    CHECK( r->status == save::SaveCommentStatus::Ok );
    CHECK( r->map_name == "c1a0" );
    CHECK( r->description == "[quick]a nice save" );
    CHECK( r->description_ext.empty() ); // description well under 64 chars

    // No quick/autosave substring -> no prefix.
    auto r2 = save::save_comment( image, "save/manual01.sav", std::nullopt );
    REQUIRE( r2.has_value() );
    CHECK( r2->description == "a nice save" );

    // "autosave" substring -> the [autosave] prefix.
    auto r3 = save::save_comment( image, "save/autosave.sav", std::nullopt );
    REQUIRE( r3.has_value() );
    CHECK( r3->description == "[autosave]a nice save" );
}

// ===========================================================================
// 12. save_comment — corrupt-input cases -> specific SaveErrors (the
//     adjudicated NULL-deref fix: an empty/garbage token table resolves the
//     GameHeader name check gracefully instead of crashing).
// ===========================================================================
static void test_save_comment_corrupt_cases()
{
    save::SavContainerParams params;
    params.map_name = "c1a0";
    params.comment  = "x";

    auto buf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
    REQUIRE( buf != nullptr );
    std::vector<std::byte> good;
    REQUIRE( save::write_sav_container( params, nullptr, {}, *buf, good ).has_value() );

    // Sanity: the good image parses.
    REQUIRE( save::save_comment( good, "x", std::nullopt ).has_value() );

    // 1. Truncated below the 8-byte id+version prefix.
    {
        auto r = save::save_comment( std::span<const std::byte>( good ).subspan( 0, 4 ), "x", std::nullopt );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }
    // 2. Over-budget tokenCount ("<corrupted hashtable>" -> CorruptHeader).
    {
        std::vector<std::byte> bad = good;
        bad[12] = b( 0x00 );
        bad[13] = b( 0x10 ); // 0x1000 = 4096 > SAVE_HASHSTRINGS (4095)
        bad[14] = b( 0x00 );
        bad[15] = b( 0x00 );
        auto r = save::save_comment( bad, "x", std::nullopt );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::CorruptHeader );
    }
    // 3. Declared data size runs past the image -> TruncatedBlock.
    {
        std::vector<std::byte> bad = good;
        bad[8]  = b( 0xFF );
        bad[9]  = b( 0xFF );
        bad[10] = b( 0x00 );
        bad[11] = b( 0x00 );
        auto r = save::save_comment( bad, "x", std::nullopt );
        CHECK( !r.has_value() );
        CHECK( r.error() == save::SaveError::TruncatedBlock );
    }
    // 4. tokenSize == 0 with a nonzero tokenCount — the exact legacy NULL-
    //    pTokenList crash trigger.  A genuine field-header-shaped record
    //    (size=4,tokenIdx=2,payload=int32(0)) sits in the data region so the
    //    name lookup actually runs; token slot 2 resolves to "" (every slot
    //    is NULL when tokenSize==0), which fails the "GameHeader" name check
    //    gracefully (MissingGameHeader status) instead of crashing.
    {
        std::vector<std::byte> img( 28, std::byte{ 0 } );
        const auto              magic = static_cast<std::uint32_t>( save::k_savegame_magic );
        const auto              ver   = static_cast<std::uint32_t>( save::k_savegame_version );
        img[0] = b( static_cast<int>( magic & 0xFF ) );
        img[1] = b( static_cast<int>( ( magic >> 8 ) & 0xFF ) );
        img[2] = b( static_cast<int>( ( magic >> 16 ) & 0xFF ) );
        img[3] = b( static_cast<int>( ( magic >> 24 ) & 0xFF ) );
        img[4] = b( static_cast<int>( ver & 0xFF ) );
        img[5] = b( static_cast<int>( ( ver >> 8 ) & 0xFF ) );
        img[8]  = b( 8 ); // size = 8 (one field-header-shaped record)
        img[12] = b( 4 ); // tokenCount = 4
        // tokenSize (bytes 16-19) stays 0.
        // data region (bytes 20-27): short size=4, short tokenIdx=2, int32 payload=0.
        img[20] = b( 4 );
        img[22] = b( 2 );
        auto r = save::save_comment( img, "x", std::nullopt );
        REQUIRE( r.has_value() ); // no crash — the adjudicated fix
        CHECK( r->status == save::SaveCommentStatus::MissingGameHeader );
    }
}

// ===========================================================================
// 13. Timestamp formatting — pinned with a fixed time_t (TZ forced to UTC for
//     the process so the reentrant localtime wrapper is deterministic).
// ===========================================================================
static void test_timestamp_formatting_fixed()
{
#if defined( _WIN32 )
    _putenv_s( "TZ", "UTC" );
    _tzset();
#else
    ::setenv( "TZ", "UTC", 1 );
    ::tzset();
#endif

    save::SavContainerParams params;
    params.map_name = "c1a0";
    params.comment  = "timestamp test";

    auto buf = save::create_save_buffer( g_pool, 1024, 8, 0.0f );
    REQUIRE( buf != nullptr );
    std::vector<std::byte> image;
    REQUIRE( save::write_sav_container( params, nullptr, {}, *buf, image ).has_value() );

    // 2026-01-05 09:30:00 UTC (epoch 1767605400) — single-digit day, pins the
    // zero-pad.
    const std::time_t fixed_time = 1767605400;

    auto r = save::save_comment( image, "x", fixed_time );
    REQUIRE( r.has_value() );
    CHECK( r->status == save::SaveCommentStatus::Ok );
    // "%b%d %Y" — NO space between month abbrev and day (legacy quirk);
    // "%d" is a ZERO-PADDED 2-digit day (strftime semantics) — "Jan05", not
    // "Jan5" (CORRECTED 2026-07-19, S8.5 parity audit).
    CHECK_EQ( r->date_string, std::string( "Jan05 2026" ) );
    // "%H:%M" — zero-padded.
    CHECK_EQ( r->time_string, std::string( "09:30" ) );

    // 2026-01-15 09:30:00 UTC (epoch 1768469400) — double-digit day, pins the
    // already-2-digit shape is unaffected by the zero-pad fix.
    const std::time_t fixed_time_2 = 1768469400;

    auto r2 = save::save_comment( image, "x", fixed_time_2 );
    REQUIRE( r2.has_value() );
    CHECK( r2->status == save::SaveCommentStatus::Ok );
    CHECK_EQ( r2->date_string, std::string( "Jan15 2026" ) );
    CHECK_EQ( r2->time_string, std::string( "09:30" ) );
}

// ===========================================================================
// 14. build_save_comment — the write-side comment builder + gTitleComments.
// ===========================================================================
static void test_build_save_comment()
{
    CHECK_EQ( save::k_title_comments.size(), 66u );

    // gTitleComments prefix match (case-insensitive, first match wins).
    const std::string c1 = save::build_save_comment( "c1a0", "", "", 90.0f );
    // c1a0 does not itself appear as an exact table key, but "C1A0" appears
    // literally in the table -> match on the "C1A0" row's own entry.
    CHECK( c1.find( "#C1A1TITLE" ) == std::string::npos ); // sanity: not the C1A1 title
    CHECK( c1.find( "#C0A1TITLE" ) != std::string::npos ); // the C1A0 row's fallback title

    // No table match -> falls back to world_message.
    const std::string c2 = save::build_save_comment( "no_such_map", "Hello World", "", 0.0f );
    CHECK( c2.find( "Hello World" ) != std::string::npos );

    // No table match, no world message -> falls back to map_name.
    const std::string c3 = save::build_save_comment( "no_such_map", "", "", 65.0f );
    CHECK( c3.find( "no_such_map" ) != std::string::npos );
    CHECK( c3.find( "01:05" ) != std::string::npos ); // 65s -> 1:05, zero-padded

    // dll_comment takes precedence over everything.
    const std::string c4 = save::build_save_comment( "c1a0", "World Msg", "DLL Comment", 0.0f );
    CHECK( c4.find( "DLL Comment" ) != std::string::npos );
}

// ===========================================================================
// main
// ===========================================================================
int main()
{
    xash::core::register_thread_role( xash::core::ThreadRole::Main );
    g_pool = mem::create_pool( "sav_container_test" );

    RUN_TEST( test_container_gameheader_golden );
    RUN_TEST( test_container_roundtrip_and_extension_blind );
    RUN_TEST( test_container_restore_global_state_callback );
    RUN_TEST( test_hlx_door );
    RUN_TEST( test_client_state_empty_golden );
    RUN_TEST( test_client_state_populated_roundtrip );
    RUN_TEST( test_static_entity_model_name_text );
    RUN_TEST( test_entity_patch_roundtrip );
    RUN_TEST( test_age_save_list_sequence );
    RUN_TEST( test_directory_count_and_clear_save_dir );
    RUN_TEST( test_select_latest_save_ordering );
    RUN_TEST( test_latest_save_file_backed );
    RUN_TEST( test_save_comment_version_gates );
    RUN_TEST( test_save_comment_full_parse );
    RUN_TEST( test_save_comment_corrupt_cases );
    RUN_TEST( test_timestamp_formatting_fixed );
    RUN_TEST( test_build_save_comment );

    if ( g_pool )
        mem::destroy_pool( g_pool );

    std::printf( "sav_container: %d passed, %d failed\n", g_pass, g_fail );
    return g_fail ? 1 : 0;
}
