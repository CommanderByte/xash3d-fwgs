#pragma once
// xash3dpp — delta per-field payload codec (Layer 4 core)
// Legacy reference: engine/common/net_encode.c —
//   Delta_ClampIntegerField, Delta_CompareField, Delta_WriteField_,
//   Delta_ReadField_, Delta_CopyField
//
// These free functions serialise ONE field's payload; they never write the
// per-field mark bit or the GoldSrc group masks — framing belongs to the
// IDeltaWireFormat siblings (wire_format.hpp).  Arithmetic conversion chains
// (uint <-> float, truncations) replicate the legacy code exactly; golden
// vectors in tests/networking/delta/ pin the resulting bit patterns.
//
// All functions are caller-synchronised, no allocation, no logging.

#include <xash3dpp/networking/delta.hpp>

namespace xash::networking { class MessageBuf; }

namespace xash::networking::delta {

// Clamp an integer payload into `numbits` (signed range when signbit != 0).
// No-op for numbits >= 32.  Legacy: Delta_ClampIntegerField.
[[nodiscard]] int clamp_integer_field( int value, int signbit, int numbits ) noexcept;

// True when the field is unchanged between `from` and `to` (or inactive).
// Legacy: Delta_CompareField — integers compare post-multiplier+clamp,
// floats/angles compare raw bit patterns, timewindows compare quantised,
// strings compare exact.
[[nodiscard]] bool compare_field( const DeltaField &field,
                                  const void *from, const void *to ) noexcept;

// Serialise the field payload from `to` (legacy Delta_WriteField_; the
// legacy `from` parameter was unused).  `timebase` feeds the TIMEWINDOW
// encodings.
void write_field_payload( MessageBuf &msg, const DeltaField &field,
                          const void *to, double timebase ) noexcept;

// Deserialise one field payload into `to`.  Legacy: Delta_ReadField_.
void read_field_payload( MessageBuf &msg, const DeltaField &field,
                         void *to, double timebase ) noexcept;

// Copy the field value from `from` into `to` (receiver path for unchanged
// fields).  Legacy: Delta_CopyField (its timebase parameter was unused).
void copy_field( const DeltaField &field, const void *from, void *to ) noexcept;

} // namespace xash::networking::delta
