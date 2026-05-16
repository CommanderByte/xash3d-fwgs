// xash3dpp — gameinfo.txt / liblist.gam parser implementation
// Legacy reference: filesystem/filesystem.c  (FS_ParseGameInfo, FS_WriteGameInfo)
//
// scan_game_directories() lives in filesystem.cpp (Filesystem::scan_game_directories).
// write_gameinfo_file() is a host concern: call serialise_gameinfo() + write_file().

#include <xash3dpp/utilities/gameinfo_parser.hpp>

#include <xash3dpp/limits.hpp>

#include <algorithm>

namespace xash {

std::optional<GameInfo> parse_gameinfo_txt(std::string_view content,
                                           std::string_view gamefolder)
{
    // TODO: parse GoldSrc keyvalue pairs (key "value" per line)
    return std::nullopt;
}

std::optional<GameInfo> parse_liblist_gam(std::string_view content,
                                          std::string_view gamefolder)
{
    // TODO: parse liblist.gam format; map fields to GameInfo members
    return std::nullopt;
}

std::string serialise_gameinfo(const GameInfo& info) {
    // TODO: emit gameinfo.txt keyvalue text
    return {};
}

void apply_gameinfo_fixups(GameInfo& info) {
    // Clamp budgets to sane ranges.
    info.max_edicts    = std::clamp(info.max_edicts,    ::xash::limits::gameinfo_edicts_min,    ::xash::limits::gameinfo_edicts_max);
    info.max_tents     = std::clamp(info.max_tents,     ::xash::limits::gameinfo_tents_min,     ::xash::limits::gameinfo_tents_max);
    info.max_beams     = std::clamp(info.max_beams,     ::xash::limits::gameinfo_beams_min,     ::xash::limits::gameinfo_beams_max);
    info.max_particles = std::clamp(info.max_particles, ::xash::limits::gameinfo_particles_min, ::xash::limits::gameinfo_particles_max);

    // TODO: derive dll_path from game_dll / game_dll_linux / game_dll_osx
    //       based on current platform; fill missing title from gamefolder.
}

} // namespace xash
