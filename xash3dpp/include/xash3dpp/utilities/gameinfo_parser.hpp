#pragma once
// xash3dpp — gameinfo.txt / liblist.gam parser  (public)
// Legacy reference: filesystem/filesystem.c  (FS_ParseGameInfo, FS_WriteGameInfo)
//
// Pure text transforms: no filesystem or OS dependency.
// For scanning the game directory tree, use Filesystem::scan_game_directories().
// For writing gameinfo.txt, call serialise_gameinfo() then Filesystem::write_file().

#include <xash3dpp/gameinfo.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace xash {

// Parse a gameinfo.txt content string (GoldSrc keyvalue syntax).
// Returns nullopt if the content is empty or cannot be parsed.
[[nodiscard]] std::optional<GameInfo> parse_gameinfo_txt(std::string_view content,
                                           std::string_view gamefolder);

// Parse a liblist.gam content string (GoldSrc alternate format).
// Returns nullopt if the content is empty or cannot be parsed.
[[nodiscard]] std::optional<GameInfo> parse_liblist_gam(std::string_view content,
                                          std::string_view gamefolder);

// Serialise a GameInfo to gameinfo.txt keyvalue text.
[[nodiscard]] std::string serialise_gameinfo(const GameInfo& info);

// Post-parse fixups: clamp budgets, derive platform dll_path, fill
// missing title from gamefolder, etc.  Call after parsing.
void apply_gameinfo_fixups(GameInfo& info);

} // namespace xash
