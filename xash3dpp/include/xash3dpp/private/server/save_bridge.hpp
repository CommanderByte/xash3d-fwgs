#pragma once
// xash3dpp — server<->save wiring: the ILevelChangeExecutor save paths, the
// game-DLL save-callback bridges, and the save/load console commands
// (Chunk 8, slice S8.7).
//
// This is the SERVER-CORE half of save/restore (save-boundary.md §Dependencies:
// "server owns the changelevel/loadgame orchestration ... the edict arena").
// The pure `.sav`/`.HL1-3` codecs (xash3dpp_save) are driven here through their
// injected seams (IEntitySaver / IEntityRestorer / ISolidTestProvider /
// IAdjacentLevelSource / ISaveGlobalState / IRestoreGlobalState /
// ITransitionRestorer), each backed by the live edict arena, the game DLL's
// DLL_FUNCTIONS save callbacks, the world point-contents kernel, and the
// filesystem save directory.
//
// Legacy reference: engine/server/sv_save.c :2199-2239 (SV_SaveGame),
// :1704-1774 (SaveGameSlot), :521-583 (IsValidSave), :2124-2192 (SV_LoadGame),
// sv_init.c:1120-1128 (SV_ExecLoadGame), :2049-2117 (SV_ChangeLevel),
// sv_cmds.c:1030-1120 (command registration).
//
// Threading: main-thread only (save is entirely T_Main; save-boundary.md
// §Threading).  Every entry point asserts ThreadRole::Main.

#include <cstddef>
#include <string>
#include <string_view>

namespace xash { class MapLoader; }
namespace xash::cmd_cvar { class CmdCvarContext; }

namespace xash::server {

struct ServerRuntime;

// -----------------------------------------------------------------------------
// ILevelChangeExecutor save paths (driven by the MapLoader FSM).
// -----------------------------------------------------------------------------

// SV_LoadGame staging + SV_ExecLoadGame (sv_save.c:2124-2192 + sv_init.c:1120):
// read `save/<save_name>.sav`, extract its embedded `.HL?` records to the save
// directory, force the single-player cvars (maxplayers 1 / deathmatch 0 /
// coop 0), spawn the container's map, LoadGameState (fallback SpawnEntities),
// apply sv.time, then ActivateServer(false).  Returns false on any staging or
// spawn failure (the caller keeps the previous server running).
[[nodiscard]] bool save_exec_load_game( ServerRuntime &rt, std::string_view save_name ) noexcept;

// SV_ChangeLevel with loadfromsavedgame == true (sv_save.c:2049-2117): the H1
// sequence — SaveGameState(true) with a Sys_Warn-not-Host_Error abort keeping
// the server running, InactivateClients/FinalMessage/DeactivateServer,
// SpawnServer, SaveFinish (RAII), LoadGameState (fallback SpawnEntities),
// LoadAdjacentEnts, ClearSaveDir on sv_newunit, ActivateServer(false).
[[nodiscard]] bool save_exec_change_level( ServerRuntime &rt, std::string_view map,
                                           std::string_view landmark, bool background ) noexcept;

// -----------------------------------------------------------------------------
// SV_SaveGame mechanics (used by the save/savequick/autosave commands).
// -----------------------------------------------------------------------------

// IsValidSave (sv_save.c:521-583): the save-precondition gate incl. the physint
// SV_AllowSaveGame veto seam.  Returns true when a save is permitted now.
[[nodiscard]] bool save_is_valid( ServerRuntime &rt ) noexcept;

// SaveBuildComment (sv_save.c:306-357): the world/level description text for the
// new save's GAME_HEADER.comment (dll_comment > gTitleComments > world message
// > map name), formatted with the elapsed sv.time.
[[nodiscard]] std::string save_build_comment( ServerRuntime &rt ) noexcept;

// SaveGameSlot (sv_save.c:1704-1774): snapshot the current level to
// `save/<map>.HL1`/`.HL2`, then bundle every `save/*.HL?` scratch file into
// `save/<save_name>.sav` with the GAME_HEADER + global-state blob.  AgeSaveList
// runs for the quick/autosave stems.  Returns false on any write failure.
[[nodiscard]] bool save_write_slot( ServerRuntime &rt, std::string_view save_name,
                                    std::string_view comment ) noexcept;

// -----------------------------------------------------------------------------
// Console commands (B5 ctx overload).
// -----------------------------------------------------------------------------

// The user pointer handed to every save-command dispatch (the B5 CommandCtxFn
// `user`).  @lifetime: caller — must outlive the registration (Server::Impl
// owns it and unregisters at shutdown).
struct SaveCommandContext
{
    ServerRuntime     *rt   = nullptr; // @lifetime: engine
    ::xash::MapLoader *maps = nullptr; // @lifetime: engine — the FSM the load path queues onto
};

// SV_InitHostCommands + SV_InitOperatorCommands save half (sv_cmds.c:1040-1081):
// register save/load/savequick/loadquick/autosave/killsave/reload via the B5
// context overload with the exact legacy privilege flags.  Idempotent replace
// is the cmd registry's own concern.
void register_save_commands( ::xash::cmd_cvar::CmdCvarContext &cvars,
                             SaveCommandContext &ctx ) noexcept;

// Unregister them (SV_KillOperatorCommands save half, sv_cmds.c:1115-1120 + the
// restricted set), called from Server::shutdown.
void unregister_save_commands( ::xash::cmd_cvar::CmdCvarContext &cvars ) noexcept;

} // namespace xash::server
