// xash3dpp — launcher entry point
// Legacy reference: game_launch/game.cpp  (WinMain / main calling Host_Main)
//
// Thin bootstrap: resolve rootdir from the executable path, scan argv for
// the standard engine options, fill HostArgs, then call Host::Main.
// No engine logic lives here.
//
// stats exempt: the launcher is a one-shot argv → HostArgs bootstrap with no
// mutable runtime state and no per-frame path — there is nothing to instrument
// (reviewer §5; QN Observability exemption for state-free entry points).

#include <xash3dpp/core/thread_role.hpp>
#include <xash3dpp/host/host.hpp>
#include <xash3dpp/platform/platform.hpp>

#include <cstdlib>      // atoi, EXIT_SUCCESS
#include <cstring>      // strcmp
#include <string>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

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
    // The process's first thread IS the engine main thread.  Register the
    // role before ANY engine call: assert_thread_role(Main) is fatal on an
    // Unknown thread, so an unregistered production main would abort at the
    // first asserted entry point (QN prerequisite; test mains do the same).
    xash::core::register_thread_role(xash::core::ThreadRole::Main);

    xash::HostArgs args;

    args.argc    = argc;
    args.argv    = argv;
    args.rootdir = xash::platform::get_executable_dir();

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
