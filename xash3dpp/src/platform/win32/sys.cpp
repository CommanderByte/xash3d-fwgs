// xash3dpp — platform (Win32) — process-level services
// Legacy reference: engine/platform/win32/sys_win.c,
//                  engine/common/system.h
//
// Existing subsystems used:
//   xash3dpp_utilities — path::extract_dir, path::fix_slashes

#ifndef _WIN32
#  error "This file is Win32-only"
#endif

#include <xash3dpp/platform/platform.hpp>
#include <xash3dpp/limits.hpp>
#include <xash3dpp/utilities/path.hpp>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include <cstdint>   // std::uint32_t, std::uintptr_t
#include <cstring>   // std::memcpy
#include <optional>  // std::optional (name_for_symbol / locate_export_table)

#include <xash3dpp/private/core/assert_main.hpp>

namespace xash::platform {

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

double get_time() noexcept
{
    // Capture QPC frequency and start counter on first call (magic-static,
    // thread-safe since C++11).
    struct ClockInit
    {
        LONGLONG freq;
        LONGLONG start;
        ClockInit() noexcept
        {
            LARGE_INTEGER f, s;
            QueryPerformanceFrequency( &f );
            QueryPerformanceCounter( &s );
            freq  = f.QuadPart;
            start = s.QuadPart;
        }
    };
    static const ClockInit s_clock{};
    // Capture main-thread ID on first call (inside magic-static — thread-safe).
    static const bool s_main_captured = []() noexcept {
        ::xash::core::detail::capture_main_thread();
        return true;
    }();
    (void)s_main_captured;

    LARGE_INTEGER now;
    QueryPerformanceCounter( &now );
    return static_cast<double>( now.QuadPart - s_clock.start )
         / static_cast<double>( s_clock.freq );
}

void sleep( std::uint32_t ms ) noexcept
{
    Sleep( static_cast<DWORD>( ms ) );
}

// ---------------------------------------------------------------------------
// Dynamic library loading
// ---------------------------------------------------------------------------

LibHandle open_library( std::string_view path ) noexcept
{
    // Convert UTF-8 path to UTF-16 for LoadLibraryW.
    wchar_t wbuf[::xash::limits::platform_path_buf_wchars];
    int len = MultiByteToWideChar( CP_UTF8, 0,
                                   path.data(), static_cast<int>( path.size() ),
                                   wbuf, static_cast<int>( std::size( wbuf ) ) - 1 );
    if( len <= 0 )
        return {};
    wbuf[len] = L'\0';

    HMODULE h = LoadLibraryW( wbuf );
    return { static_cast<void *>( h ) };
}

void *get_symbol( LibHandle lib, const char *name ) noexcept
{
    if( !lib )
        return nullptr;
    return reinterpret_cast<void *>( // SAFETY: fn-ptr → void* — Win32 loader contract; GetProcAddress results round-trip through void* by API design
        GetProcAddress( static_cast<HMODULE>( lib.native ), name ) );
}

void close_library( LibHandle &lib ) noexcept
{
    if( !lib )
        return;
    FreeLibrary( static_cast<HMODULE>( lib.native ) );
    lib = {};
}

// ---------------------------------------------------------------------------
// Dynamic library reverse lookup (SAV-OQ-3) — see platform.hpp for the
// full contract and the lib_win.c cites for the forwarder/ordinal-only
// handling below.
// ---------------------------------------------------------------------------

namespace {

// Located export directory of a mapped PE image, with the tables already
// resolved to absolute pointers (RVA + module base — a loaded image has its
// sections mapped at their VirtualAddress offsets, so this is a direct
// pointer add; no PointerToRawData translation is needed, unlike legacy's
// file-based LibraryLoadSymbols).
struct PeExportTable
{
    std::uintptr_t base;
    const DWORD    *names;         // AddressOfNames: RVA[] -> name-string RVA
    const WORD     *name_ordinals; // AddressOfNameOrdinals: RVA[] -> functions[] index
    const DWORD    *functions;     // AddressOfFunctions: RVA[] -> code/forwarder RVA
    DWORD           num_names;
    DWORD           num_functions;
    DWORD           export_dir_rva;
    DWORD           export_dir_size;
};

// Walk the mapped PE headers of |lib| and locate its export directory.
// Returns nullopt if |lib| is null or the image has no (or a malformed)
// export directory — defensive only; every module this engine loads is
// itself a well-formed PE image validated by the OS loader.
[[nodiscard]] std::optional<PeExportTable> locate_export_table( LibHandle lib ) noexcept
{
    if( !lib )
        return std::nullopt;

    // SAFETY: HMODULE IS the module's mapped base address per the Win32
    // loader contract (LoadLibrary's return value doubles as the image
    // base). Every reinterpret_cast below is a PE-header walk over that
    // OS-mapped, OS-validated image — the same header chain lib_win.c reads
    // from the file, here read directly from memory instead.
    auto base = reinterpret_cast<std::uintptr_t>( lib.native );

    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>( base ); // SAFETY: see above
    if( dos->e_magic != IMAGE_DOS_SIGNATURE )
        return std::nullopt;

    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>( // SAFETY: see above
        base + static_cast<std::uintptr_t>( dos->e_lfanew ) );
    if( nt->Signature != IMAGE_NT_SIGNATURE )
        return std::nullopt;

    const IMAGE_DATA_DIRECTORY &export_dir =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if( export_dir.VirtualAddress == 0 || export_dir.Size == 0 )
        return std::nullopt; // no export directory (e.g. a plain EXE)

    const auto *dir = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY *>( // SAFETY: see above
        base + export_dir.VirtualAddress );

    PeExportTable table{};
    table.base            = base;
    table.names           = reinterpret_cast<const DWORD *>( base + dir->AddressOfNames );        // SAFETY: see above
    table.name_ordinals   = reinterpret_cast<const WORD  *>( base + dir->AddressOfNameOrdinals );  // SAFETY: see above
    table.functions       = reinterpret_cast<const DWORD *>( base + dir->AddressOfFunctions );     // SAFETY: see above
    table.num_names       = dir->NumberOfNames;
    table.num_functions   = dir->NumberOfFunctions;
    table.export_dir_rva  = export_dir.VirtualAddress;
    table.export_dir_size = export_dir.Size;
    return table;
}

// True if |func_rva| is a forwarder RVA — i.e. it points inside the export
// directory's own address range (a null-terminated "OtherDll.OtherFunc"
// string) instead of at executable code. Standard PE forwarder-detection
// convention; legacy's COM_NameForFunction (lib_win.c:581-601) does not
// perform this check — see the @deviation note in platform.hpp.
[[nodiscard]] bool is_forwarder_rva( const PeExportTable &table, DWORD func_rva ) noexcept
{
    return func_rva >= table.export_dir_rva &&
           func_rva <  table.export_dir_rva + table.export_dir_size;
}

} // namespace

std::optional<std::string_view> name_for_symbol( LibHandle lib, const void *addr ) noexcept
{
    if( !addr )
        return std::nullopt;

    auto table = locate_export_table( lib );
    if( !table )
        return std::nullopt;

    auto target = reinterpret_cast<std::uintptr_t>( addr ); // SAFETY: address-as-integer comparison only, never dereferenced here

    // Only the AddressOfNames-sized arrays are walked — an export present by
    // ordinal only (no name) is never in these arrays and is silently
    // unmatched, exactly like legacy's LibraryLoadSymbols.
    for( DWORD i = 0; i < table->num_names; ++i )
    {
        WORD ordinal = table->name_ordinals[i];
        if( ordinal >= table->num_functions )
            continue; // malformed table — defensive only

        DWORD func_rva = table->functions[ordinal];
        if( is_forwarder_rva( *table, func_rva ) )
            continue;

        if( table->base + func_rva != target )
            continue;

        const char *name = reinterpret_cast<const char *>( table->base + table->names[i] ); // SAFETY: name RVA into the same mapped image
        return std::string_view{ name };
    }
    return std::nullopt;
}

bool enumerate_exports( LibHandle lib, ExportVisitor visit, void *userdata ) noexcept
{
    if( !visit )
        return false;

    auto table = locate_export_table( lib );
    if( !table )
        return false;

    for( DWORD i = 0; i < table->num_names; ++i )
    {
        WORD ordinal = table->name_ordinals[i];
        if( ordinal >= table->num_functions )
            continue; // malformed table — defensive only

        DWORD func_rva = table->functions[ordinal];
        if( is_forwarder_rva( *table, func_rva ) )
            continue;

        const char *name = reinterpret_cast<const char *>( table->base + table->names[i] ); // SAFETY: name RVA into the same mapped image
        const void *fn    = reinterpret_cast<const void *>( table->base + func_rva );        // SAFETY: code RVA into the same mapped image; already forwarder-filtered above
        visit( std::string_view{ name }, fn, userdata );
    }
    return true;
}

// ---------------------------------------------------------------------------
// System paths
// ---------------------------------------------------------------------------

std::string get_executable_dir()
{
    wchar_t wbuf[MAX_PATH];
    DWORD len = GetModuleFileNameW( nullptr, wbuf, MAX_PATH );
    if( len == 0 )
        return {};

    char buf[MAX_PATH * 4];
    int n = WideCharToMultiByte( CP_UTF8, 0,
                                 wbuf, static_cast<int>( len ),
                                 buf, static_cast<int>( sizeof( buf ) ) - 1,
                                 nullptr, nullptr );
    if( n <= 0 )
        return {};
    buf[n] = '\0';

    // extract_dir returns the directory component WITHOUT trailing separator.
    auto dir = ::xash::utilities::fix_slashes( ::xash::utilities::extract_dir(
        std::string_view{ buf, static_cast<std::size_t>( n ) } ) );
    // Callers expect a trailing '/' so they can append a filename directly.
    if( !dir.empty() && dir.back() != '/' )
        dir += '/';
    return dir;
}

std::string get_working_directory()
{
    wchar_t wbuf[MAX_PATH];
    DWORD len = GetCurrentDirectoryW( MAX_PATH, wbuf );
    if( len == 0 )
        return {};

    char buf[MAX_PATH * 4];
    int n = WideCharToMultiByte( CP_UTF8, 0,
                                 wbuf, static_cast<int>( len ),
                                 buf, static_cast<int>( sizeof( buf ) ) - 1,
                                 nullptr, nullptr );
    if( n <= 0 )
        return {};
    buf[n] = '\0';

    std::string result{ buf, static_cast<std::size_t>( n ) };
    if( result.back() != '/' && result.back() != '\\' )
        result += '/';
    return ::xash::utilities::fix_slashes( result );
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

bool is_debugger_present() noexcept
{
    return static_cast<bool>( IsDebuggerPresent() );
}

// ---------------------------------------------------------------------------
// User interaction
// ---------------------------------------------------------------------------

void message_box( std::string_view title, std::string_view message ) noexcept
{
    // Null-terminate stack copies — MessageBoxA requires C strings.
    char t[256], m[1024];
    std::size_t tlen = title.size()   < sizeof( t ) - 1 ? title.size()   : sizeof( t ) - 1;
    std::size_t mlen = message.size() < sizeof( m ) - 1 ? message.size() : sizeof( m ) - 1;
    std::memcpy( t, title.data(),   tlen ); t[tlen] = '\0';
    std::memcpy( m, message.data(), mlen ); m[mlen] = '\0';
    MessageBoxA( nullptr, m, t, MB_OK | MB_SETFOREGROUND | MB_ICONSTOP );
}

void shell_execute( std::string_view path, std::string_view params ) noexcept
{
    char p[1024], a[1024];
    std::size_t plen = path.size()   < sizeof( p ) - 1 ? path.size()   : sizeof( p ) - 1;
    std::size_t alen = params.size() < sizeof( a ) - 1 ? params.size() : sizeof( a ) - 1;
    std::memcpy( p, path.data(),   plen ); p[plen] = '\0';
    std::memcpy( a, params.data(), alen ); a[alen] = '\0';
    ShellExecuteA( nullptr, "open", p, alen ? a : nullptr, nullptr, SW_SHOW );
}

} // namespace xash::platform
