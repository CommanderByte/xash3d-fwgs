// xash3dpp — server<->save wiring implementation (Chunk 8, slice S8.7).
//
// Retires the two ILevelChangeExecutor save stubs by driving the pure
// xash3dpp_save codecs (LevelStateWriter/Loader, container_codec, client_state,
// adjacent_transfer, save_directory, save_comment) through server-core seam
// bridges backed by the live edict arena, the game DLL's DLL_FUNCTIONS save
// callbacks, the world point-contents kernel, and the filesystem save dir.
//
// Legacy reference: engine/server/sv_save.c + sv_init.c:1120-1128
// (SV_ExecLoadGame); the exact call orders are cited per function below.
//
// Existing subsystems used:
//   xash3dpp_save       — the .sav/.HL1-3 codecs (driven through their seams)
//   xash3dpp_filesystem — save-directory I/O
//   xash3dpp_cmd_cvar   — the SP-cvar force + the save/load command registry
//   xash3dpp_core       — logging, thread-role asserts

#include <xash3dpp/private/server/save_bridge.hpp>

#include <xash3dpp/private/server/edict_arena.hpp>
#include <xash3dpp/private/server/engine_bridge.hpp> // alloc_private_data
#include <xash3dpp/private/server/entity_view.hpp>
#include <xash3dpp/private/server/lifecycle.hpp>
#include <xash3dpp/private/server/string_pool.hpp>
#include <xash3dpp/private/server/world_trace.hpp> // point_contents

#include <xash3dpp/private/save/adjacent_transfer.hpp>
#include <xash3dpp/private/save/client_state.hpp>
#include <xash3dpp/private/save/container_codec.hpp>
#include <xash3dpp/private/save/entity_patch.hpp>
#include <xash3dpp/private/save/entity_table.hpp>
#include <xash3dpp/private/save/format.hpp>
#include <xash3dpp/private/save/level_state_loader.hpp>
#include <xash3dpp/private/save/level_state_writer.hpp>
#include <xash3dpp/private/save/save_buffer.hpp>
#include <xash3dpp/private/save/save_comment.hpp>
#include <xash3dpp/private/save/save_directory.hpp>

#include <xash3dpp/abi/server_consts.hpp>
#include <xash3dpp/cmd_cvar/command.hpp>
#include <xash3dpp/cmd_cvar/context.hpp>
#include <xash3dpp/cmd_cvar/cvar.hpp>
#include <xash3dpp/core/log.hpp>
#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/filesystem/filesystem.hpp>
#include <xash3dpp/map_loader/contents.hpp>
#include <xash3dpp/map_loader/map_loader.hpp>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace xash::server {

namespace {

namespace sv_save = ::xash::save;

constexpr char k_tag[] = "server.save";

// SV_IsValidEdict: non-null and not freed (matches level_state_writer's guard).
[[nodiscard]] bool valid_edict( const ::xash::abi::edict_t *ed ) noexcept
{
    return ed != nullptr && ed->free == 0;
}

// Build "save/<name><ext>" (k_default_save_directory-rooted; sv_save.c:1735).
[[nodiscard]] std::string save_path( std::string_view name, std::string_view ext )
{
    std::string p( sv_save::k_default_save_directory );
    p.append( name );
    p.append( ext );
    return p;
}

// COM_FileWithoutPath: the bare filename after the last '/' or '\\'.
[[nodiscard]] std::string_view bare_name( std::string_view path ) noexcept
{
    const std::size_t slash = path.find_last_of( "/\\" );
    return slash == std::string_view::npos ? path : path.substr( slash + 1 );
}

// -------------------------------------------------------------------------
// Game-DLL save-callback bridges (save-boundary.md §Dependencies: the field
// codec is game-DLL-owned; the edict arena + world are server-core).
// -------------------------------------------------------------------------

// The pfnSave stand-in (IEntitySaver): projects a SAVERESTOREDATA over the
// writer's working buffer, positions it at the current write cursor, and calls
// the game DLL's pfnSave — which writes its field records straight into the ABI
// window (HL-SDK CSave::BufferData) and interns field NAMES into the raw
// pTokens array.  Afterwards the buffer's extent + the token table are
// reconciled from the ABI window (SaveBuffer::commit_abi_write /
// TokenTable::sync_from_abi).  sv_save.c:1548-1559.
class EntitySaverBridge final : public sv_save::IEntitySaver
{
public:
    EntitySaverBridge( ServerRuntime &rt, sv_save::SaveBuffer &buf,
                       sv_save::EntityTable &table, float time_basis ) noexcept
        : rt_( &rt ), buf_( &buf ), table_( &table ), time_basis_( time_basis )
    {
    }

    [[nodiscard]] sv_save::Result<void>
    save_entity( std::size_t table_index, const ::xash::abi::edict_t *edict,
                 sv_save::IFieldSink & /*sink*/, sv_save::TokenTable & /*tokens*/ ) noexcept override
    {
        const auto &funcs = rt_->game.funcs();
        if ( funcs.pfnSave == nullptr || !valid_edict( edict ) )
            return {}; // nothing to serialize (row.size stays 0)

        ::xash::abi::SAVERESTOREDATA srd;
        buf_->to_abi( srd ); // pCurrentData = base + cursor (the write position), pTokens filled
        srd.currentIndex = static_cast<int>( table_index );
        srd.time         = time_basis_; // FIELD_TIME rebase basis (header.time)
        if ( table_->count() > 0 )
        {
            srd.pTable     = &table_->row( 0 ); // DispatchSave indexes pTable[currentIndex]
            srd.tableCount = static_cast<int>( table_->count() );
        }

        rt_->globals.pSaveData = &srd;
        funcs.pfnSave( const_cast<::xash::abi::edict_t *>( edict ), &srd ); // SAFETY: the DLL ABI takes a non-const edict_t*; pfnSave reads entvars, does not free the edict
        rt_->globals.pSaveData = nullptr;

        // Adopt the DLL-written extent + any DLL-interned token names.
        if ( auto r = buf_->commit_abi_write( static_cast<std::size_t>( srd.size ) ); !r )
            return r;
        buf_->tokens().sync_from_abi();
        return {};
    }

private:
    ServerRuntime        *rt_;
    sv_save::SaveBuffer  *buf_;
    sv_save::EntityTable *table_;
    float                 time_basis_;
};

// szCurrentMapName sink (eiface.h:345 "To check global entities").  A
// pfnRestore-bound SAVERESTOREDATA must carry the map name LoadSaveData was
// called with (sv_save.c:941): for LoadGameState that is the level being
// loaded; for LoadAdjacentEnts's per-connection restores it is the ADJACENT
// level's own name (sv_save.c:1963 — LoadSaveData(currentLevelData.levelList
// [i].mapName), traced against the :1941-1965 loop).  EntityRestorerBridge and
// TransitionRestorerBridge both implement this so AdjacentLevelBridge can
// re-point the active bridge exactly when AdjacentTransfer::run() loads a new
// connection's image (immediately BEFORE that connection's transfers run —
// adjacent_transfer.cpp's per-connection loop — so the ordering is safe).
class ICurrentMapSink
{
public:
    ICurrentMapSink() noexcept                            = default;
    virtual ~ICurrentMapSink()                             = default;
    ICurrentMapSink( const ICurrentMapSink & )             = delete;
    ICurrentMapSink &operator=( const ICurrentMapSink & )  = delete;

    virtual void set_current_map( std::string_view name ) noexcept = 0;
};

// The pfnRestore stand-in + edict recreation (IEntityRestorer).
// sv_save.c:1434-1457 (create) + 1664-1685 (restore).
class EntityRestorerBridge final : public sv_save::IEntityRestorer, public ICurrentMapSink
{
public:
    // `table`/`buf` are the SAME EntityTable/SaveBuffer the caller feeds to the
    // LevelStateLoader/AdjacentTransfer for this restore pass — dispatch_restore
    // reconstructs the legacy pBaseData/pCurrentData/size/bufferSize window
    // straight from them (sv_save.c:1667-1668; see dispatch_restore below).
    EntityRestorerBridge( ServerRuntime &rt, const sv_save::EntityTable &table,
                          const sv_save::SaveBuffer &buf ) noexcept
        : rt_( &rt ), table_( &table ), buf_( &buf )
    {
    }

    // szCurrentMapName (eiface.h:345): copied with Q_strncpy truncation
    // semantics (sv_save.c:941) into the 32-byte field.
    // compliance-allow(thread-assert): writes only the stack-local restore
    // bridge's own szCurrentMapName buffer, no engine state
    void set_current_map( std::string_view name ) noexcept override
    {
        const std::size_t n =
            name.size() < sizeof( current_map_ ) - 1 ? name.size() : sizeof( current_map_ ) - 1;
        std::memcpy( current_map_, name.data(), n );
        current_map_[n] = '\0';
    }

    [[nodiscard]] ::xash::abi::edict_t *
    create_entity( sv_save::RestoreCreateKind kind, int id,
                   std::string_view classname ) noexcept override
    {
        switch ( kind )
        {
        case sv_save::RestoreCreateKind::World:
        {
            // Reuse edict 0 (SV_EdictNum(0) + SV_InitEdict), sv_save.c:1434-1438.
            ::xash::abi::edict_t *ed = rt_->arena.edict_num( 0 );
            if ( ed != nullptr )
                rt_->arena.init_edict( ed );
            return ed;
        }
        case sv_save::RestoreCreateKind::Client:
        {
            // Reuse the client edict at slot id (sv_save.c:1440-1449); the
            // residual `active && SV_IsValidEdict(ed)` gate (:1448).
            ::xash::abi::edict_t *ed = rt_->arena.edict_num( static_cast<std::size_t>( id ) );
            return valid_edict( ed ) ? ed : nullptr;
        }
        case sv_save::RestoreCreateKind::Named:
        default:
        {
            // SV_CreateNamedEntity(NULL, classname) — index NOT preserved
            // (sv_save.c:1451-1453).
            char name[64];
            const std::size_t n =
                classname.size() < sizeof( name ) - 1 ? classname.size() : sizeof( name ) - 1;
            std::memcpy( name, classname.data(), n );
            name[n] = '\0';
            return alloc_private_data( nullptr, rt_->strings.alloc_string( name ), nullptr );
        }
        }
    }

    [[nodiscard]] int
    restore_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                    std::span<const std::byte> entity_data, const sv_save::TokenTable &tokens,
                    float time_basis ) noexcept override
    {
        return dispatch_restore( pent, table_index, entity_data, tokens, time_basis,
                                 /*globalEntity=*/0, /*use_landmark=*/false, {} );
    }

    // Shared pfnRestore projection (also used by the transition bridge).
    [[nodiscard]] int
    dispatch_restore( ::xash::abi::edict_t *pent, std::size_t table_index,
                      std::span<const std::byte> entity_data, const sv_save::TokenTable &tokens,
                      float time_basis, int global_entity, bool use_landmark,
                      sv_save::SaveVec3 landmark_offset ) noexcept
    {
        const auto &funcs = rt_->game.funcs();
        if ( funcs.pfnRestore == nullptr || pent == nullptr )
            return 0; // no codec / no edict: a benign no-op restore

        // Bounds assertion (defense-in-depth; the loader's view_at already
        // rejected an out-of-range location/size before entity_data was ever
        // constructed, level_state_loader.cpp/adjacent_transfer.cpp — reject-
        // gracefully rather than trust `table_index` blindly here too).
        if ( table_index >= table_->count() )
        {
            ::xash::core::log( ::xash::core::LogLevel::Warning, k_tag,
                               "dispatch_restore: table_index out of range" );
            return 0;
        }

        // Legacy window convention (sv_save.c:1667-1668/1851-1852): pBaseData is
        // the WHOLE data-region base, constant across the restore loop;
        // pCurrentData = pBaseData + row.location; `size` is the write-cursor
        // quirk `= row.location` (NOT the entity's byte length); bufferSize is
        // the region-wide capacity, also constant.  `entity_data` is the
        // loader's already-bounds-checked [pBaseData+location, +row.size)
        // window, so subtracting `location` off its start recovers pBaseData;
        // `buf_` is the SAME [ETABLE][data] buffer that window was sliced
        // from, so its end IS the data region's end.
        const ::xash::abi::ENTITYTABLE &row = table_->row( table_index );
        const std::size_t location = static_cast<std::size_t>( row.location );

        // SAFETY: the ABI window hands the DLL a raw char* into the loader's
        // owned data region; std::byte* -> char* is a permitted byte-type pun.
        // The `-location` step is pointer arithmetic within the same
        // allocation (entity_data.data() == region base + location, by
        // construction of the view the caller sliced it from).
        auto *current = const_cast<char *>( reinterpret_cast<const char *>( entity_data.data() ) );
        char *base    = current - location;

        const std::span<const std::byte> region = buf_->data(); // [ETABLE][data]
        const char *region_begin = reinterpret_cast<const char *>( region.data() );
        const char *region_end   = region_begin + region.size();

        if ( base < region_begin || current + entity_data.size() > region_end )
        {
            ::xash::core::log( ::xash::core::LogLevel::Warning, k_tag,
                               "dispatch_restore: window out of region bounds" );
            return 0;
        }
        const std::size_t buffer_size = static_cast<std::size_t>( region_end - base );

        ::xash::abi::SAVERESTOREDATA srd{};
        srd.pBaseData    = base;
        srd.pCurrentData = current;                     // pBaseData + location (sv_save.c:1667)
        srd.size         = static_cast<int>( location ); // write-cursor quirk (sv_save.c:1668)
        srd.bufferSize   = static_cast<int>( buffer_size );
        srd.currentIndex = static_cast<int>( table_index );
        srd.time         = time_basis;
        // SAFETY: abi_pointers() regenerates a scratch char** view over the
        // token slots; it does not mutate the authoritative slot storage, so a
        // const TokenTable is safely projected read-only for the DLL.
        srd.pTokens      = const_cast<sv_save::TokenTable &>( tokens ).abi_pointers();
        srd.tokenCount   = static_cast<int>( tokens.token_count() );
        srd.fUseLandmark = use_landmark ? 1 : 0;
        srd.vecLandmarkOffset[0] = landmark_offset.x;
        srd.vecLandmarkOffset[1] = landmark_offset.y;
        srd.vecLandmarkOffset[2] = landmark_offset.z;
        // szCurrentMapName (eiface.h:345), set by set_current_map: the level
        // being loaded (LoadGameState path) or the adjacent connection's own
        // map (transition path, via AdjacentLevelBridge::load_level_image).
        std::memcpy( srd.szCurrentMapName, current_map_, sizeof( srd.szCurrentMapName ) );

        rt_->globals.pSaveData = &srd;
        const int rc           = funcs.pfnRestore( pent, &srd, global_entity );
        rt_->globals.pSaveData = nullptr;
        return rc;
    }

private:
    ServerRuntime              *rt_;
    const sv_save::EntityTable *table_;
    const sv_save::SaveBuffer  *buf_;
    char                        current_map_[32] = {};
};

// EntityInSolid's world-contents probe (ISolidTestProvider), sv_save.c:485-488.
class SolidTestBridge final : public sv_save::ISolidTestProvider
{
public:
    explicit SolidTestBridge( ServerRuntime &rt ) noexcept : rt_( &rt ) {}

    [[nodiscard]] bool point_in_solid( const sv_save::SaveVec3 &point, int group_mask ) noexcept override
    {
        const int saved       = rt_->move_env.group_mask;
        rt_->move_env.group_mask = group_mask; // svs.groupmask (sv_save.c:486)
        const ::xash::utilities::Vec3 p{ point.x, point.y, point.z };
        const int contents    = point_contents( rt_->move_env, p );
        rt_->move_env.group_mask = saved;
        return contents == ::xash::map_loader::k_contents_solid;
    }

private:
    ServerRuntime *rt_;
};

// LoadSaveData/EntityPatchRead/EntityPatchWrite over the save directory
// (IAdjacentLevelSource), sv_save.c:1963-1995.
class AdjacentLevelBridge final : public sv_save::IAdjacentLevelSource
{
public:
    // `sink` is re-pointed to `map_name` on every load_level_image call — the
    // exact call-order analog of LoadSaveData setting szCurrentMapName
    // (sv_save.c:941/1963; see ICurrentMapSink's doc comment above).
    AdjacentLevelBridge( ::xash::filesystem::Filesystem &fs, ICurrentMapSink &sink ) noexcept
        : fs_( &fs ), sink_( &sink )
    {
    }

    [[nodiscard]] std::optional<std::vector<std::byte>>
    load_level_image( std::string_view map_name ) noexcept override
    {
        const std::string path = save_path( map_name, ".HL1" );
        if ( !fs_->file_exists( path, true ) )
            return std::nullopt; // a connection whose level was never saved
        sink_->set_current_map( map_name );
        return fs_->load_file( path, true );
    }

    [[nodiscard]] std::optional<std::vector<std::byte>>
    load_entity_patch( std::string_view map_name ) noexcept override
    {
        const std::string path = save_path( map_name, ".HL3" );
        if ( !fs_->file_exists( path, true ) )
            return std::nullopt; // nothing was ever removed from this level
        return fs_->load_file( path, true );
    }

    [[nodiscard]] sv_save::Result<void>
    store_entity_patch( std::string_view map_name, const sv_save::EntityTable &table ) noexcept override
    {
        return sv_save::write_hl3_file( *fs_, map_name, table );
    }

private:
    ::xash::filesystem::Filesystem *fs_;
    ICurrentMapSink                *sink_;
};

// CreateEntityTransitionList's per-entity work (ITransitionRestorer),
// sv_save.c:1836-1922 — the changelevel adjacent-transfer branches.
class TransitionRestorerBridge final : public sv_save::ITransitionRestorer, public ICurrentMapSink
{
public:
    TransitionRestorerBridge( ServerRuntime &rt, const sv_save::EntityTable &table,
                              const sv_save::SaveBuffer &buf ) noexcept
        : rt_( &rt ), inner_( rt, table, buf )
    {
    }

    // szCurrentMapName sink — forwards to the shared dispatch_restore state
    // (see EntityRestorerBridge::set_current_map / AdjacentLevelBridge).
    // compliance-allow(thread-assert): thin delegator to the inner restore
    // bridge's caller-owned name buffer
    void set_current_map( std::string_view name ) noexcept override
    {
        inner_.set_current_map( name );
    }

    [[nodiscard]] ::xash::abi::edict_t *
    create_entity( sv_save::RestoreCreateKind kind, int id,
                   std::string_view classname ) noexcept override
    {
        return inner_.create_entity( kind, id, classname );
    }

    [[nodiscard]] int
    transfer_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                     std::span<const std::byte> entity_data, const sv_save::TokenTable &tokens,
                     const sv_save::TransitionContext &ctx ) noexcept override
    {
        return inner_.dispatch_restore( pent, table_index, entity_data, tokens, ctx.time_basis,
                                        /*globalEntity=*/0, ctx.use_landmark, ctx.landmark_offset );
    }

    [[nodiscard]] sv_save::GlobalMergeResult
    merge_global_entity( ::xash::abi::edict_t *pent, std::size_t table_index,
                         std::span<const std::byte> entity_data, const sv_save::TokenTable &tokens,
                         const sv_save::TransitionContext &ctx ) noexcept override
    {
        // The global-merge pre-read (classname/globalname) + SV_FindGlobalEntity
        // is a game-DLL/global-state concern; XASH3DPP-STUB(chunk12): a full
        // global-entity registry lands with the global-state seam.  Here the
        // merge is attempted as a shouldPrecache==1 restore; no repoint target
        // is resolved (leave the row's pent as-is on failure).
        sv_save::GlobalMergeResult out;
        const int rc = inner_.dispatch_restore( pent, table_index, entity_data, tokens,
                                                ctx.time_basis, /*globalEntity=*/1,
                                                ctx.use_landmark, ctx.landmark_offset );
        out.merged = rc > 0;
        return out;
    }

private:
    ServerRuntime         *rt_;
    EntityRestorerBridge   inner_;
};

// -------------------------------------------------------------------------
// SaveGameState (.HL1 assembly) + LoadGameState (.HL1 restore) helpers.
// -------------------------------------------------------------------------

// Collect the current level's non-empty lightstyles (sv_save.c:1529-1538).
void gather_lightstyles( ServerRuntime &rt,
                         std::vector<sv_save::LightStyleInput> &out )
{
    for ( int i = 0; static_cast<std::size_t>( i ) < ::xash::limits::server_lightstyles; ++i )
    {
        const LightStyle *ls = rt.lightstyles.style( i );
        if ( ls == nullptr || ls->pattern[0] == '\0' )
            continue;
        out.push_back( sv_save::LightStyleInput{ i, ls->pattern, /*time*/ 0.0f } );
    }
}

[[nodiscard]] float cvar_f( ServerRuntime &rt, std::string_view name ) noexcept
{
    return rt.cvars != nullptr ? rt.cvars->cvar_variable_value( name ) : 0.0f;
}
[[nodiscard]] const char *cvar_s( ServerRuntime &rt, std::string_view name ) noexcept
{
    return rt.cvars != nullptr ? rt.cvars->cvar_variable_string( name ) : "";
}

// Cvar_SetValue (cvar.c:811-820): "%d" when the float is within 1e-6 of an
// integer, else "%f" — used for the skill/sky-color/sky-vec header fields
// applied on restore (sv_save.c:1649/1653-1658).
void cvar_set_value( ServerRuntime &rt, std::string_view name, float value ) noexcept
{
    if ( rt.cvars == nullptr )
        return;
    char val[32];
    if ( std::fabs( value - static_cast<float>( static_cast<int>( value ) ) ) < 0.000001f )
        std::snprintf( val, sizeof( val ), "%d", static_cast<int>( value ) );
    else
        std::snprintf( val, sizeof( val ), "%f", static_cast<double>( value ) );
    rt.cvars->cvar_set( name, val );
}

// SaveGameState(false) (sv_save.c:1469-1619): serialize the current level to
// `save/<map>.HL1` (+ an empty client `.HL2` on a dedicated server).
// `connections` is empty for a full save; the changelevel path passes the
// landmark list.  Returns false on any write failure.
[[nodiscard]] bool
write_level_state( ServerRuntime &rt, std::string_view map_name,
                   std::span<const ::xash::abi::LEVELLIST> connections ) noexcept
{
    if ( rt.fs == nullptr || !rt.game_pool )
        return false;

    const float time = rt.globals.time;

    auto wbuf = sv_save::create_save_buffer( rt.game_pool, ::xash::limits::save_heap_size,
                                             ::xash::limits::save_hash_strings, time );
    if ( !wbuf )
        return false;

    // Gather all edicts [0, num_entities) + their classname text (sv_save.c:1492).
    std::vector<::xash::abi::edict_t *> edicts;
    std::vector<std::string_view>       classnames;
    const std::size_t                   count = rt.arena.num_entities();
    edicts.reserve( count );
    classnames.reserve( count );
    for ( std::size_t i = 0; i < count; ++i )
    {
        ::xash::abi::edict_t *ed = rt.arena.edict_num( i );
        edicts.push_back( ed );
        classnames.push_back( valid_edict( ed )
                                  ? std::string_view{ rt.strings.get_string( ed->v.classname ) }
                                  : std::string_view{} );
    }

    std::vector<sv_save::LightStyleInput> lightstyles;
    gather_lightstyles( rt, lightstyles );

    sv_save::EntityTable table;
    EntitySaverBridge    saver( rt, *wbuf, table, time );

    sv_save::LevelStateParams params;
    params.skill_level = static_cast<int>( cvar_f( rt, "skill" ) );
    params.time        = time;
    params.map_name    = map_name;
    params.sky_name    = cvar_s( rt, "sv_skyname" );
    params.sky_color_r = static_cast<int>( cvar_f( rt, "sv_skycolor_r" ) );
    params.sky_color_g = static_cast<int>( cvar_f( rt, "sv_skycolor_g" ) );
    params.sky_color_b = static_cast<int>( cvar_f( rt, "sv_skycolor_b" ) );
    params.sky_vec_x   = cvar_f( rt, "sv_skyvec_x" );
    params.sky_vec_y   = cvar_f( rt, "sv_skyvec_y" );
    params.sky_vec_z   = cvar_f( rt, "sv_skyvec_z" );
    params.connections = connections;
    params.lightstyles = lightstyles;
    params.edicts      = edicts;
    params.classnames  = classnames;
    params.saver       = &saver;

    std::vector<std::byte> hl1_image;
    sv_save::LevelStateWriter writer( *wbuf, table );
    if ( auto r = writer.write( params, hl1_image ); !r )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag, "SaveGameState: .HL1 write failed" );
        return false;
    }

    if ( !rt.fs->write_file( save_path( map_name, ".HL1" ), hl1_image ) )
        return false;

    // Client state `.HL2` — dedicated server: null capabilities => the empty
    // legacy-loadable block (client_state.hpp "Empty .HL2 derivation").
    sv_save::ClientStateParams client;
    client.wateralpha = cvar_f( rt, "sv_wateralpha" );
    client.wateramp   = cvar_f( rt, "sv_wateramp" );
    if ( auto r = sv_save::write_hl2_file( *rt.fs, map_name, client, *wbuf ); !r )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag, "SaveClientState: .HL2 write failed" );
        return false;
    }

    return true;
}

// LoadGameState (sv_save.c:1628-1695): restore `save/<map>.HL1` into the freshly
// spawned world.  Returns false when the file is missing / corrupt (the caller
// falls back to SV_SpawnEntities).  On success, `header_time_out` receives the
// restored FIELD_TIME basis the caller applies to sv.time (:1692).
[[nodiscard]] bool
load_level_state( ServerRuntime &rt, std::string_view map_name, float &header_time_out ) noexcept
{
    if ( rt.fs == nullptr || !rt.game_pool )
        return false;

    const std::string path = save_path( map_name, ".HL1" );
    if ( !rt.fs->file_exists( path, true ) )
        return false;

    std::vector<std::byte> image = rt.fs->load_file( path, true );
    if ( image.empty() )
        return false;

    auto lbuf = sv_save::create_save_buffer( rt.game_pool );
    if ( !lbuf )
        return false;

    sv_save::EntityTable      table;
    sv_save::LevelStateLoader loader( *lbuf, table );
    sv_save::LevelState       state;
    if ( auto r = loader.load( image, state ); !r )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Warning, k_tag,
                            "LoadGameState: .HL1 parse failed (err %d, %zu bytes)",
                            static_cast<int>( r.error() ), image.size() );
        return false;
    }

    // skill + sky parms from the loaded header (sv_save.c:1649-1658), applied
    // BEFORE entity restore — matching ParseSaveTables' position in legacy
    // (:985, right after the header/adjacency/lightstyle reads, before
    // CreateEntitiesInRestoreList at :1661).
    cvar_set_value( rt, "skill", static_cast<float>( state.header.skill_level ) );
    if ( rt.cvars != nullptr )
        rt.cvars->cvar_set( "sv_skyname", state.header.sky_name );
    cvar_set_value( rt, "sv_skycolor_r", static_cast<float>( state.header.sky_color_r ) );
    cvar_set_value( rt, "sv_skycolor_g", static_cast<float>( state.header.sky_color_g ) );
    cvar_set_value( rt, "sv_skycolor_b", static_cast<float>( state.header.sky_color_b ) );
    cvar_set_value( rt, "sv_skyvec_x", state.header.sky_vec_x );
    cvar_set_value( rt, "sv_skyvec_y", state.header.sky_vec_y );
    cvar_set_value( rt, "sv_skyvec_z", state.header.sky_vec_z );

    // Lightstyles (sv_save.c:996-1003, ParseSaveTables' updateGlobals==true
    // branch): reset every style, then apply each saved record.  The
    // LoadAdjacentEnts ParseSaveTables call (updateGlobals==false, :1967) does
    // NOT do this — adjacent levels never touch the live lightstyle table.
    rt.lightstyles.reset();
    for ( const sv_save::SaveLightStyle &ls : state.lightstyles )
        rt.lightstyles.set( ls.index, ls.style, ls.time );

    EntityRestorerBridge restorer( rt, table, *lbuf );
    // szCurrentMapName (eiface.h:345): the LoadGameState path's LoadSaveData
    // call (sv_save.c:1636) sets it from `level` == the level being loaded —
    // `map_name` here, for BOTH the SV_LoadGame and the SV_ChangeLevel
    // own-level restore callers of load_level_state.
    restorer.set_current_map( map_name );
    sv_save::RestoreConfig config;
    config.max_clients = rt.persistent.maxclients > 0 ? rt.persistent.maxclients : 1;
    if ( auto r = loader.restore_entities( state, restorer, config ); !r )
    {
        ::xash::core::log( ::xash::core::LogLevel::Warning, k_tag, "LoadGameState: entity restore failed" );
        return false;
    }

    header_time_out = state.header.time;
    return true;
}

// -------------------------------------------------------------------------
// Command handlers (B5 ctx overload).
// -------------------------------------------------------------------------

[[nodiscard]] SaveCommandContext *cmd_ctx( void *user ) noexcept
{
    return static_cast<SaveCommandContext *>( user );
}

// SV_SaveGame (sv_save.c:2199-2239): IsValidSave -> build comment -> write slot.
void do_save_game( ServerRuntime &rt, std::string_view name ) noexcept
{
    if ( !save_is_valid( rt ) )
        return;
    const std::string comment = save_build_comment( rt );
    if ( !save_write_slot( rt, name, comment ) )
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag, "save failed" );
}

void cmd_save( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c == nullptr || c->rt == nullptr || c->rt->cvars == nullptr )
        return;
    // SV_Save_f (sv_cmds.c:427-449): argc 1 => "new" (free-slot scan folded
    // into the name), argc 2 => argv(1), else S_USAGE "save <savename>\n" (:444).
    switch ( c->rt->cvars->cmd_argc() )
    {
    case 1:
        do_save_game( *c->rt, "new" );
        break;
    case 2:
        do_save_game( *c->rt, std::string_view{ c->rt->cvars->cmd_argv( 1 ) } );
        break;
    default:
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Usage: save <savename>" );
        break;
    }
}

void cmd_savequick( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c != nullptr && c->rt != nullptr )
        do_save_game( *c->rt, "quick" );
}

void cmd_autosave( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c == nullptr || c->rt == nullptr || c->rt->cvars == nullptr )
        return;
    // SV_AutoSave_f (sv_cmds.c:488-497): argc != 1 => S_USAGE "autosave\n".
    if ( c->rt->cvars->cmd_argc() != 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Usage: autosave" );
        return;
    }
    if ( cvar_f( *c->rt, "sv_autosave" ) != 0.0f ) // SV_AutoSave_f gate (sv_cmds.c)
        do_save_game( *c->rt, "autosave" );
}

// SV_Load_f/SV_QuickLoad_f: queue the restore onto the MapLoader FSM
// (COM_LoadGame -> STATE_LOAD_GAME -> exec_load_game).
void queue_load( SaveCommandContext &c, std::string_view name ) noexcept
{
    if ( c.maps != nullptr )
        c.maps->load_game( name );
}

void cmd_load( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c == nullptr || c->rt == nullptr || c->rt->cvars == nullptr )
        return;
    // SV_Load_f (sv_cmds.c:400-410): argc != 2 => S_USAGE "load <savename>\n".
    if ( c->rt->cvars->cmd_argc() != 2 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Usage: load <savename>" );
        return;
    }
    queue_load( *c, c->rt->cvars->cmd_argv( 1 ) );
}

void cmd_loadquick( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c != nullptr )
        queue_load( *c, "quick" );
}

// SV_DeleteSave_f (killsave): remove the .sav (+ its .bmp saveshot).
void cmd_killsave( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c == nullptr || c->rt == nullptr || c->rt->cvars == nullptr || c->rt->fs == nullptr )
        return;
    // SV_DeleteSave_f (sv_cmds.c:466-476): argc != 2 => S_USAGE "killsave <name>\n".
    if ( c->rt->cvars->cmd_argc() != 2 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Usage: killsave <name>" );
        return;
    }
    const std::string_view name = c->rt->cvars->cmd_argv( 1 );
    (void)c->rt->fs->remove( save_path( name, ".sav" ) );
    (void)c->rt->fs->remove( save_path( name, ".bmp" ) );
}

// SV_Reload_f (sv_cmds.c): continue from the latest save, else restart the map.
void cmd_reload( void *user ) noexcept
{
    auto *c = cmd_ctx( user );
    if ( c == nullptr || c->rt == nullptr || c->rt->fs == nullptr )
        return;
    if ( auto latest = sv_save::latest_save( *c->rt->fs ); latest.has_value() )
    {
        queue_load( *c, *latest );
        return;
    }
    // XASH3DPP-STUB(chunk12): the restart-current-map fallback (COM_LoadLevel
    // sv_hostmap) needs the host map cvar; the latest-save path is the common one.
}

} // namespace

// ---------------------------------------------------------------------------
// IsValidSave (sv_save.c:521-583).
// ---------------------------------------------------------------------------

bool save_is_valid( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( !rt.persistent.initialized || rt.level.state != ServerState::Active )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Not playing a local game." );
        return false;
    }
    if ( rt.level.background )
        return false; // ignore autosave during background (+ UI_CreditsActive, client-side)

    // Physint SV_AllowSaveGame veto seam (sv_save.c:533-540).
    // XASH3DPP-STUB(chunk11): svgame.physFuncs is the physics-interface
    // negotiation (SV_InitPhysicsAPI) that lands with Chunk 11; until then no
    // physint provider exists, so the veto never fires (legacy default: allowed).

    if ( rt.persistent.maxclients != 1 )
    {
        ::xash::core::log( ::xash::core::LogLevel::Info, k_tag, "Can't save multiplayer games." );
        return false;
    }

    // CL_Active / intermission / the spawned-live-player gate (sv_save.c:558-582)
    // are client-machinery state (S9) — XASH3DPP-STUB(chunk12): the live-player
    // dead/health screening lands with the client-connection integration; the
    // single-player + active-server gates above are the load-bearing ones for
    // the server-core save path.
    return true;
}

// ---------------------------------------------------------------------------
// SaveBuildComment (sv_save.c:306-357).
// ---------------------------------------------------------------------------

std::string save_build_comment( ServerRuntime &rt ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    // Optional game-DLL SV_SaveGameComment export (SV_InitSaveRestore).
    char        dll_buf[80] = {};
    std::string dll_comment;
    if ( rt.save_game_comment != nullptr )
    {
        rt.save_game_comment( dll_buf, static_cast<int>( sizeof( dll_buf ) ) );
        dll_comment.assign( dll_buf );
    }

    // svgame.edicts->v.message — the world entity's title text (string pool).
    std::string_view       world_message;
    ::xash::abi::edict_t   *world = rt.arena.edict_num( 0 );
    if ( valid_edict( world ) && world->v.message != 0 )
        world_message = rt.strings.get_string( world->v.message );

    return sv_save::build_save_comment( rt.level.name, world_message, dll_comment,
                                        rt.globals.time );
}

// ---------------------------------------------------------------------------
// SaveGameSlot (sv_save.c:1704-1774).
// ---------------------------------------------------------------------------

bool save_write_slot( ServerRuntime &rt, std::string_view save_name,
                      std::string_view comment ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.fs == nullptr || !rt.game_pool )
        return false;

    const std::string_view map_name{ rt.level.name };

    // (1) Snapshot the current level to save/<map>.HL1 + .HL2 (SaveGameState).
    if ( !write_level_state( rt, map_name, {} ) )
        return false;

    // (2) Age the quick/autosave slot rotation (sv_save.c:1739-1743).  The aged
    // budget is gameinfo-owned (GI->quicksave_aged_count / autosave_aged_count,
    // filesystem.h:125-126); xash::GameInfo does not expose either field yet
    // (XASH3DPP-STUB(chunk12): the gameinfo.txt aged-count parse, filesystem.c
    // :1015-1023), so use the legacy SAVE_AGED_COUNT default of 2 for both
    // (filesystem.c:53,788) — NOT 1.
    constexpr int k_default_aged_count = 2; // SAVE_AGED_COUNT (filesystem.c:53)
    if ( sv_save::is_quicksave_stem( save_name ) )
        sv_save::age_save_list( *rt.fs, "quick", k_default_aged_count );
    else if ( sv_save::is_autosave_stem( save_name ) )
        sv_save::age_save_list( *rt.fs, "autosave", k_default_aged_count );

    // (3) Bundle every save/*.HL? scratch file into the .sav container
    // (DirectoryCopy, sv_save.c:1770).  glob -> load -> embed.
    const ::xash::filesystem::SearchResult hits =
        rt.fs->search( save_path( "*", ".HL?" ), true, true );

    std::vector<std::vector<std::byte>> blobs; // own the embedded bytes
    std::vector<sv_save::EmbeddedFile>  embedded;
    blobs.reserve( hits.files.size() );
    embedded.reserve( hits.files.size() );
    for ( const std::string &path : hits.files )
    {
        blobs.push_back( rt.fs->load_file( path, true ) );
        embedded.push_back( sv_save::EmbeddedFile{ bare_name( path ), blobs.back() } );
    }

    auto sav_buf = sv_save::create_save_buffer( rt.game_pool );
    if ( !sav_buf )
        return false;

    sv_save::SavContainerParams params;
    params.map_name = map_name;
    params.comment  = comment;

    // XASH3DPP-STUB(chunk11): a game DLL exporting pfnSaveGlobalState would emit
    // a global-state blob here (write_sav_container's ISaveGlobalState seam); no
    // physint/global-state provider exists until Chunk 11, so it is omitted
    // (nullptr == the "null = empty" seam, container_codec.hpp).
    if ( auto r = sv_save::write_sav_file( *rt.fs, save_name, params, /*global_state=*/nullptr,
                                           embedded, *sav_buf );
         !r )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag, "SaveGameSlot: .sav write failed" );
        return false;
    }

    // XASH3DPP-STUB(chunk12): the "saveshot <name>" Cbuf preview-image hook
    // (sv_save.c:1748) is CLIENT-side (CL_GenericShot_f) — deferred with the
    // client renderer.
    ::xash::core::logf( ::xash::core::LogLevel::Info, k_tag, "Saved game to %.*s.sav",
                        static_cast<int>( save_name.size() ), save_name.data() );
    return true;
}

// ---------------------------------------------------------------------------
// SV_ExecLoadGame + SV_LoadGame staging (sv_init.c:1120-1128 + sv_save.c:2124).
// ---------------------------------------------------------------------------

bool save_exec_load_game( ServerRuntime &rt, std::string_view save_name ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.fs == nullptr || rt.maps == nullptr )
        return false;

    // (1) Read + extract the .sav container (SaveReadHeader + DirectoryExtract,
    //     sv_save.c:2155-2156).  A pool for the container's working buffer.
    if ( !rt.game_pool )
    {
        // The game DLL is not yet loaded (SV_InitGame) — bring it up so the
        // restore has an arena + string pool + save callbacks (sv_save.c:2149).
        if ( !load_progs( rt, rt.cfg.game_dll ) )
            return false;
    }

    auto sav_buf = sv_save::create_save_buffer( rt.game_pool );
    if ( !sav_buf )
        return false;

    auto loaded = sv_save::load_sav_file( *rt.fs, save_name, /*global_state=*/nullptr, *sav_buf );
    if ( !loaded || !loaded->has_value() )
    {
        ::xash::core::logf( ::xash::core::LogLevel::Error, k_tag, "Couldn't load %.*s.sav",
                            static_cast<int>( save_name.size() ), save_name.data() );
        return false;
    }
    const sv_save::SavContainerResult &container = **loaded;

    // Extract every embedded record to the save directory (DirectoryExtract is
    // extension-blind, sv_save.c:679-706) so LoadGameState/LoadAdjacentEnts can
    // read save/<map>.HL1/.HL2/.HL3 by name.
    for ( const sv_save::ExtractedRecord &rec : container.records )
    {
        if ( !rt.fs->write_file( save_path( rec.name, "" ), rec.data ) )
            ::xash::core::log( ::xash::core::LogLevel::Warning, k_tag, "extract: record write failed" );
    }

    const std::string_view map_name{ container.header.map_name };

    // SV_MapIsValid(gameHeader.mapName, NULL) (sv_save.c:2163-2175): reject an
    // unsupported-version or missing map before spawning it.  XASH3DPP-STUB
    // (chunk12): pfn_is_map_valid is still the chunk6 always-fail stub
    // (engine_table.cpp), so wiring a real check here would make every load
    // fail; the map-existence gate stays deferred until that lands.
    //
    // Host_IsDedicated() / UI_CreditsActive() (sv_save.c:2131-2135) are
    // client/UI-side refusals — XASH3DPP-STUB(chunk12): the client-connection
    // integration.  The single-player + active-server gates (mirroring
    // save_is_valid's) are the load-bearing ones for the server-core load
    // path today.

    // (2) Force the single-player cvars (sv_save.c:2186-2188).
    if ( rt.cvars != nullptr )
    {
        rt.cvars->cvar_full_set( "maxplayers", "1", ::xash::cmd_cvar::FCVAR_LATCH );
        rt.cvars->cvar_set( "deathmatch", "0" );
        rt.cvars->cvar_set( "coop", "0" );
    }

    // (3) SV_ExecLoadGame: SpawnServer -> LoadGameState (fallback SpawnEntities)
    //     -> ActivateServer(false) (sv_init.c:1120-1128).
    rt.strings.set_dynamic( false ); // STATIC string-pool half for the new level
    if ( !spawn_server( rt, std::string( map_name ).c_str(), nullptr, /*background=*/false ) )
        return false;

    float header_time = rt.globals.time;
    if ( !load_level_state( rt, map_name, header_time ) )
    {
        const ::xash::map_loader::WorldData *world = rt.maps->world();
        if ( world != nullptr )
            spawn_entities( rt, *world );
    }
    else
    {
        // sv.time = header.time, restored AFTER SpawnServer reset it (sv_save.c:1692).
        rt.level.time    = header_time;
        rt.globals.time  = header_time;
        rt.bridge.sv_time = rt.level.time; // ABI-shim mirror of sv.time
    }

    // Restore pauses until the client connects (sv_save.c:1647/1941).
    rt.level.loadgame = true;
    rt.level.paused   = true;

    activate_server( rt, /*run_physics=*/false ); // the settle-frame restore path
    return rt.level.state == ServerState::Active;
}

// ---------------------------------------------------------------------------
// SV_ChangeLevel with loadfromsavedgame == true (sv_save.c:2049-2117).
// ---------------------------------------------------------------------------

bool save_exec_change_level( ServerRuntime &rt, std::string_view map,
                             std::string_view landmark, bool background ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    if ( rt.level.state != ServerState::Active )
    {
        ::xash::core::log( ::xash::core::LogLevel::Error, k_tag, "server not running" );
        return false;
    }
    if ( rt.fs == nullptr || rt.maps == nullptr || !rt.game_pool )
        return false;

    char old_level[64];
    const std::size_t oln = std::string_view{ rt.level.name }.size();
    const std::size_t n   = oln < sizeof( old_level ) - 1 ? oln : sizeof( old_level ) - 1;
    std::memcpy( old_level, rt.level.name, n );
    old_level[n] = '\0';

    // Smooth transition in progress (sv_save.c:2075).
    rt.globals.changelevel = 1;

    // (1) SaveGameState(true): snapshot the current level (with its landmark
    //     connections) to the save dir.  Fill the connection list via the game
    //     DLL's pfnParmsChangeLevel (sv_save.c:1944 pattern).
    std::vector<::xash::abi::LEVELLIST> old_connections;
    {
        ::xash::abi::SAVERESTOREDATA srd{};
        rt.globals.pSaveData = &srd;
        if ( rt.game.funcs().pfnParmsChangeLevel != nullptr )
            rt.game.funcs().pfnParmsChangeLevel();
        const int cc = srd.connectionCount > 0 ? srd.connectionCount : 0;
        for ( int i = 0; i < cc && i < ::xash::abi::k_max_level_connections; ++i )
            old_connections.push_back( srd.levelList[i] );
        rt.globals.pSaveData = nullptr;
    }

    if ( !write_level_state( rt, std::string_view{ rt.level.name }, old_connections ) )
    {
        // Sys_Warn (NOT Host_Error) — keep the server running (sv_save.c:2080-2087).
        ::xash::core::log( ::xash::core::LogLevel::Warning, k_tag,
                           "Can't write save file for performing change level; check permissions" );
        rt.globals.changelevel = 0;
        return false;
    }

    // (2) Teardown (sv_save.c:2090-2092).  SV_InactivateClients / SV_FinalMessage
    // are client-machinery (S9) — deactivate_server is the load-bearing step.
    // XASH3DPP-STUB(chunk12): SV_InactivateClients + SV_FinalMessage.
    deactivate_server( rt );

    // (3) Respawn the new map (sv_save.c:2094).  The RAII SaveBuffers used above
    // have already been released (SaveFinish), so no early-return leak (the
    // legacy `// ???` leak, save-boundary.md adjudicated deviation).
    rt.strings.set_dynamic( false );
    if ( !spawn_server( rt, std::string( map ).c_str(), std::string( landmark ).c_str(), background ) )
        return false;

    // (4) LoadGameState(newmap) (fallback SpawnEntities) (sv_save.c:2100-2101).
    float header_time = rt.globals.time;
    if ( !load_level_state( rt, map, header_time ) )
    {
        const ::xash::map_loader::WorldData *world = rt.maps->world();
        if ( world != nullptr )
            spawn_entities( rt, *world );
    }

    // (5) LoadAdjacentEnts(oldlevel, landmark) (sv_save.c:2102): carry entities
    //     across the landmark from every adjacent saved level.
    {
        // The NEW level's connection list (pfnParmsChangeLevel after spawn).
        std::vector<::xash::abi::LEVELLIST> new_connections;
        ::xash::abi::SAVERESTOREDATA        srd{};
        rt.globals.pSaveData = &srd;
        if ( rt.game.funcs().pfnParmsChangeLevel != nullptr )
            rt.game.funcs().pfnParmsChangeLevel();
        const int cc = srd.connectionCount > 0 ? srd.connectionCount : 0;
        for ( int i = 0; i < cc && i < ::xash::abi::k_max_level_connections; ++i )
            new_connections.push_back( srd.levelList[i] );
        rt.globals.pSaveData = nullptr;

        auto abuf = sv_save::create_save_buffer( rt.game_pool );
        if ( abuf )
        {
            sv_save::EntityTable       atable;
            // `restorer` must exist before `source` — AdjacentLevelBridge holds
            // a sink reference to it (szCurrentMapName re-pointing, see
            // ICurrentMapSink's doc comment).
            TransitionRestorerBridge   restorer( rt, atable, *abuf );
            AdjacentLevelBridge        source( *rt.fs, restorer );
            SolidTestBridge            solid( rt );

            sv_save::AdjacentTransferParams tp;
            tp.new_map_name          = map;
            tp.old_level             = old_level;
            tp.landmark_name         = landmark;
            tp.new_level_connections = new_connections;
            tp.sv_time               = static_cast<float>( rt.level.time );
            tp.config.max_clients    = rt.persistent.maxclients > 0 ? rt.persistent.maxclients : 1;

            sv_save::AdjacentTransfer transfer( *abuf, atable );
            if ( auto r = transfer.run( tp, source, restorer, solid ); !r )
            {
                // Host_Error( "Level transition ERROR..." ) (sv_save.c:1999-2001/
                // 2013-2014): a missing back-connection or a failed .HL3 patch
                // rewrite is a hard failure in legacy, not a warn-and-continue.
                // Q-5: Host_Error semantics map to an executor failure — abort
                // the changelevel exec (matching the SaveGameState-write-
                // failure early return above).
                ::xash::core::log( ::xash::core::LogLevel::Error, k_tag,
                                   "LoadAdjacentEnts: transition transfer failed" );
                rt.globals.changelevel = 0;
                return false;
            }
        }
    }

    // (6) ClearSaveDir on sv_newunit (sv_save.c:2104-2105).
    if ( cvar_f( rt, "sv_newunit" ) != 0.0f )
        sv_save::clear_save_dir( *rt.fs );

    // svgame.globals->changelevel is cleared inside activate_server
    // (sv_init.c:645, INSIDE SV_ActivateServer after the baseline/resource-
    // list fill — NOT SV_SpawnServer, see spawn.cpp).  Clearing it here would
    // drop it FALSE one step too early vs legacy, before pfnServerActivate /
    // CreateBaseline run — it stays true through the whole restore/transfer
    // AND the ActivateServer baseline fill in legacy.
    activate_server( rt, /*run_physics=*/false );
    return rt.level.state == ServerState::Active;
}

// ---------------------------------------------------------------------------
// Command registration (sv_cmds.c:1030-1120).
// ---------------------------------------------------------------------------

void register_save_commands( ::xash::cmd_cvar::CmdCvarContext &cvars,
                             SaveCommandContext &ctx ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );

    namespace cc = ::xash::cmd_cvar;

    // SV_InitHostCommands (Cmd_AddRestrictedCommand => FCMD_PRIVILEGED).
    cvars.cmd_add( "load", cmd_load, &ctx, cc::FCMD_PRIVILEGED, "load a saved game file" );
    cvars.cmd_add( "loadquick", cmd_loadquick, &ctx, cc::FCMD_PRIVILEGED,
                   "load a quick-saved game file" );
    cvars.cmd_add( "reload", cmd_reload, &ctx, cc::FCMD_PRIVILEGED,
                   "continue from latest save or restart level" );
    cvars.cmd_add( "killsave", cmd_killsave, &ctx, cc::FCMD_PRIVILEGED,
                   "delete a saved game file and saveshot" );

    // SV_InitOperatorCommands (Cmd_AddCommand => not privileged).
    cvars.cmd_add( "save", cmd_save, &ctx, 0, "save the game to a file" );
    cvars.cmd_add( "savequick", cmd_savequick, &ctx, 0, "save the game to the quicksave" );
    cvars.cmd_add( "autosave", cmd_autosave, &ctx, 0, "save the game to 'autosave' file" );
}

void unregister_save_commands( ::xash::cmd_cvar::CmdCvarContext &cvars ) noexcept
{
    ::xash::core::assert_thread_role( ::xash::core::ThreadRole::Main );
    for ( const char *name :
          { "load", "loadquick", "reload", "killsave", "save", "savequick", "autosave" } )
        cvars.cmd_remove( name );
}

} // namespace xash::server
