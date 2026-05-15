// xash3dpp — launcher entry point
// Legacy reference: game_launch/game.cpp  (WinMain / main calling Host_Main)
//
// Thin bootstrap: resolve rootdir from the executable path, scan argv for
// the standard engine options, fill HostArgs, then call Host::Main.
// No engine logic lives here.

#include <xash3dpp/host/host.hpp>

#include <cstdlib>      // atoi, EXIT_SUCCESS
#include <cstring>      // strcmp
#include <filesystem>
#include <string>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

// ---------------------------------------------------------------------------
// exe_directory — resolve the directory containing the running binary.
// Used as rootdir so game folders are found relative to the installation.
// ---------------------------------------------------------------------------

static std::string exe_directory(int argc, char** argv)
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH))
        return std::filesystem::path(buf).parent_path().string();
#endif
    if (argc > 0 && argv[0][0] != '\0')
        return std::filesystem::path(argv[0]).parent_path().string();

    return std::filesystem::current_path().string();
}

// ---------------------------------------------------------------------------
// Lightweight argv helpers — no heap allocations, no getopt dependency.
// ---------------------------------------------------------------------------

static const char* get_arg(int argc, char** argv,
                            const char* key,
                            const char* fallback = nullptr) noexcept
{
    for (int i = 1; i < argc - 1; ++i)
        if (std::strcmp(argv[i], key) == 0)
            return argv[i + 1];
    return fallback;
}

static bool has_flag(int argc, char** argv, const char* flag) noexcept
{
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], flag) == 0)
            return true;
    return false;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    int    argc = __argc;
    char** argv = __argv;
#else
int main(int argc, char** argv)
{
#endif
    xash::HostArgs args;

    args.argc    = argc;
    args.argv    = argv;
    args.rootdir = exe_directory(argc, argv);

    // -game <folder>  — active game directory (default: "valve")
    const char* game = get_arg(argc, argv, "-game", "valve");
    args.gamedir = game;

    // -basedir <folder>  — if not supplied, basedir == gamedir
    const char* base = get_arg(argc, argv, "-basedir", nullptr);
    args.basedir = base ? base : game;

    // -rodir <path>  — optional read-only content mirror
    const char* rodir = get_arg(argc, argv, "-rodir", "");
    args.rodir = rodir;

    args.dedicated = has_flag(argc, argv, "-dedicated");

    // -dev [level]  — developer verbosity (default 0)
    if (const char* dev = get_arg(argc, argv, "-dev", nullptr))
        args.developer = std::atoi(dev);

    xash::Host host;
    return host.Main(args);
}
