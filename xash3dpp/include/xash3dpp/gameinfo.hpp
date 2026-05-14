#pragma once
// xash3dpp — game information parsed from gameinfo.txt / liblist.gam
//
// This struct is engine-wide configuration: the filesystem module discovers
// and parses it, but all other engine subsystems (host, server, client, …)
// read from it too.  It lives here at the package root so subsystems can
// include it without depending on the filesystem module.
//
// Legacy reference: filesystem/filesystem.h  (gameinfo_t)

#include <cstdint>
#include <string>

namespace xash {

struct GameInfo {
    // --- Identity ----------------------------------------------------------
    std::string title;
    std::string gamefolder;
    std::string basedir;
    std::string falldir;       // optional fallback dir; empty if none

    // --- Native library paths ----------------------------------------------
    // Platform-specific suffix resolution happens at FindLibrary() time.
    std::string game_dll;        // server game library (Windows)
    std::string game_dll_linux;
    std::string game_dll_osx;
    std::string dll_path;        // client library

    // --- Entity / effect budgets ------------------------------------------
    int max_edicts    = 900;
    int max_tents     = 500;
    int max_beams     = 128;
    int max_particles = 4096;

    // --- Behavioural flags ------------------------------------------------
    bool secure                = false;
    bool nomodels              = false;  // force player.mdl
    bool noskills              = false;
    bool render_picbutton_text = false;
    bool internal_vgui_support = false;
    bool hd_background         = false;
    bool animated_title        = false;
    bool encrypted_dll         = false;
    bool custom_loader         = false;
    bool rodir                 = false;  // metadata: parsed from the read-only dir

    // --- Demo / map names -------------------------------------------------
    std::string demomap;
    std::string startmap;
    std::string trainmap;

    // --- Game-type bitfield -----------------------------------------------
    // Values mirror the legacy GAME_* defines (singleplayer_only, etc.)
    std::uint32_t game_flags = 0;
};

} // namespace xash
