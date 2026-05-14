#pragma once
// Moved — GameInfo now lives at xash3dpp/include/xash3dpp/gameinfo.hpp
// in the xash:: namespace so all engine subsystems can include it without
// depending on the filesystem module.
//
// This header is kept for source-level backward compatibility only.
#include <xash3dpp/gameinfo.hpp>

// Inject into xash::filesystem so existing #include users keep compiling.
namespace xash::filesystem {
    using GameInfo = ::xash::GameInfo;
} // namespace xash::filesystem

// Prevent the rest of the original file from being processed.
#if 0

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

} // namespace xash::filesystem (original — now redirect)
#endif // 0
