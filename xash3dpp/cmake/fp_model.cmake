# xash3dpp — floating-point model (Q-18 PM_FP_MODEL)
#
# Default posture: STRICT everywhere.  The root CMakeLists applies
# /fp:precise (MSVC) or -ffp-contract=off (GCC/Clang) globally so that
# identical source produces bit-identical float results across compilers
# and architectures (no FMA contraction, no algebraic re-association).
# This is what keeps engine-side traces/physics reproducible between an
# x64 server and an ARM client built from the same tree.
#
# Per-target opt-out seam (mirrors the link-time compat-isolation pattern):
# a target whose math is NOT netcode-sensitive — nothing it computes feeds
# the simulation, prediction, traces, or any value that crosses the wire —
# may call xash3dpp_relax_fp() to reclaim FMA contraction.  Candidates are
# presentation-side targets only (renderer, particles, audio DSP).
#
# NEVER relax these targets (simulation-critical set, Q-18):
#   xash3dpp_networking, xash3dpp_map_loader (world/trace/PVS),
#   future xash3dpp_world / xash3dpp_physics / xash3dpp_server,
#   and any target linked into their result paths (utilities math).
#
# Full rationale: docs/design/pm-determinism-decision.md and
# decisions-architecture.md §Q-18.

function(xash3dpp_relax_fp target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /fp:fast)
    else()
        target_compile_options(${target} PRIVATE -ffp-contract=fast)
    endif()
    message(STATUS "xash3dpp: relaxed FP model on non-simulation target '${target}' (Q-18)")
endfunction()
