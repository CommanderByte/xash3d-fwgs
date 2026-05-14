// xash3dpp — gameinfo.txt / liblist.gam parser implementation
// Legacy reference: filesystem/filesystem.c  (FS_ParseGameInfo, FS_WriteGameInfo)
//
// scan_game_directories() lives in filesystem.cpp (Filesystem::ScanGameDirectories).
// write_gameinfo_file() is a host concern: call serialise_gameinfo() + WriteFile().

#include <xash3dpp/utilities/gameinfo_parser.hpp>

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
    info.max_edicts    = std::clamp(info.max_edicts,    64,  8192);
    info.max_tents     = std::clamp(info.max_tents,     32,  4096);
    info.max_beams     = std::clamp(info.max_beams,     16,  2048);
    info.max_particles = std::clamp(info.max_particles, 256, 65536);

    // TODO: derive dll_path from game_dll / game_dll_linux / game_dll_osx
    //       based on current platform; fill missing title from gamefolder.
}

} // namespace xash
