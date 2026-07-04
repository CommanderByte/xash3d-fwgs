#pragma once
// xash3dpp — Info string helpers (server-private, Chunk 6 S9)
// Legacy reference: engine/common/soundlib.. no — engine/common/info.c
//   (Info_ValueForKey :~30, Info_SetValueForKey :~250, Info_IsValid :~170,
//    Info_RemoveKey / Info_RemovePrefixedKeys).
//
// The GoldSrc userinfo/serverinfo wire format is a flat `\key\value\...`
// string.  The server rewrite needs value lookup + set/remove + validation
// for userinfo processing (SV_UserinfoChanged), serverinfo/localinfo, the
// A2S/netinfo responders, and master-server info replies.  The utilities
// layer has no Info type yet (boundary Dependencies lists `Info_` under
// utilities as a future consolidation); until then it lives here, scoped to
// the server, as free functions over caller-owned buffers (no globals, Q-2).
//
// Threading: main-thread only (server-boundary OQ-9).

#include <cstddef>

namespace xash::server {

// Info_ValueForKey: copy the value bound to `key` into `out` (always NUL-
// terminated when `out_size > 0`) and return `out`.  Absent key ⇒ out[0]='\0'.
// The legacy rotating-static-buffer contract is replaced by a caller buffer
// so there is no shared mutable state.
const char *info_value_for_key( const char *s, const char *key, char *out,
                                std::size_t out_size ) noexcept;

// Info_SetValueForKey: remove any existing binding for `key`, then append
// `\key\value` when `value` is non-empty, provided the result fits in
// `max_size` (incl. NUL).  Rejects keys/values containing '\\' or '"'
// (legacy quirk: over-long or illegal inputs are dropped with a warning and
// `s` is left unchanged).  `star_allowed` mirrors the legacy protected-key
// flag: when false, keys beginning with '*' are refused (engine-reserved).
void info_set_value_for_key( char *s, const char *key, const char *value,
                             std::size_t max_size,
                             bool star_allowed = false ) noexcept;

// Info_RemoveKey: strip the (first) binding for `key` in place.
void info_remove_key( char *s, const char *key ) noexcept;

// Info_RemovePrefixedKeys: strip every key beginning with `prefix` (legacy
// uses this to drop '_'-prefixed userinfo keys before broadcast).
void info_remove_prefixed_keys( char *s, char prefix ) noexcept;

// Info_IsValid: reject strings that contain a '"' character or that do not
// form a well-shaped `\key\value` sequence (odd backslash count / empty key).
[[nodiscard]] bool info_is_valid( const char *s ) noexcept;

} // namespace xash::server
