#pragma once
// xash3dpp — IWindowControls: window-property mutators
// Legacy reference: docs/boundaries/input-boundary.md "IWindowControls" table
// — exactly the 4 functions that dereference host.hWnd (SetMousePos,
// GetMouseGrab, SetMouseGrab, Minimize_f), plus the cursor composite
// (SetCursorType) and the window-decision-pending clipboard pair.
//
// XASH3DPP-STUB(chunk12): no window exists in this chunk. NullWindowControls
// is the only implementation shipped today; a real SDL-backed implementation
// is a Chunk-12 vendoring target (input-boundary.md "External ABI contracts").

#include <cstddef>

namespace xash::input {

// dc_arrow / dc_none — legacy VGUI_DefaultCursor (cursor_type.h).
enum class CursorType
{
    None,
    Arrow,
};

class IWindowControls
{
public:
    virtual ~IWindowControls() = default;

    // Platform_SetMousePos -> SDL_WarpMouseInWindow(host.hWnd, ...).
    virtual void set_mouse_pos(int x, int y) noexcept = 0;

    // Platform_GetMouseGrab / Platform_SetMouseGrab -> SDL_Get/SetWindowGrab(host.hWnd).
    [[nodiscard]] virtual bool get_mouse_grab() const noexcept = 0;
    virtual void set_mouse_grab(bool grab) noexcept = 0;

    // Platform_Minimize_f -> guarded `if (host.hWnd) SDL_MinimizeWindow(...)`.
    virtual void minimize() noexcept = 0;

    // Platform_SetCursorType — transitively needs hWnd + window_center_x/y
    // (warps via set_mouse_pos), sets mouse-visible + SDL cursor show/hide.
    virtual void set_cursor_type(CursorType type) noexcept = 0;

    // INP-OQ-3: clipboard placement re-deferred to the window decision;
    // carried here null-backed in the interim (boundary spec §IWindowControls).
    [[nodiscard]] virtual std::size_t get_clipboard_text(char *buf, std::size_t buf_size) const noexcept = 0;
    virtual void set_clipboard_text(const char *text) noexcept = 0;
};

// XASH3DPP-STUB(chunk12): every operation is a documented no-op / neutral
// value.  Used as the default IWindowControls when no real backend is wired.
class NullWindowControls final : public IWindowControls
{
public:
    void set_mouse_pos(int, int) noexcept override {}
    [[nodiscard]] bool get_mouse_grab() const noexcept override { return false; }
    void set_mouse_grab(bool) noexcept override {}
    void minimize() noexcept override {}
    void set_cursor_type(CursorType) noexcept override {}

    [[nodiscard]] std::size_t get_clipboard_text(char *buf, std::size_t buf_size) const noexcept override
    {
        if (buf != nullptr && buf_size > 0) { buf[0] = '\0'; }
        return 0;
    }
    void set_clipboard_text(const char *) noexcept override {}
};

} // namespace xash::input
