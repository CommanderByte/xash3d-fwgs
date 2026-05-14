#pragma once
// xash3dpp — build number and VCS metadata
// Legacy reference: public/build.h + public/build.c + public/build_vcs.c
//
// build_vcs.cpp is generated at configure/link time from VCS state.
// All other symbols are computed from it at startup.

#include <string_view>

namespace xash::utilities::build {

// Days since 2015-04-01 derived from the last commit date.
// Returns -1 if the date string could not be parsed.
int number() noexcept;

// Parse an ISO date string "YYYY-MM-DD" into a build number offset.
int number_from_date( std::string_view iso_date ) noexcept;

// Frozen compat number — always 4529 (Xash3D base build).
// Some mods test against this value; do not change.
constexpr int COMPAT_NUMBER = 4529;

// ---------------------------------------------------------------------------
// VCS strings — supplied by build_vcs.cpp (generated)
// ---------------------------------------------------------------------------

extern const std::string_view commit;        // short commit hash
extern const std::string_view branch;        // branch name
extern const std::string_view commit_date;   // "YYYY-MM-DD"

} // namespace xash::utilities::build
