# xash3dpp — cmd_cvar subsystem
# Command buffer, console variable registry, tokenization, config I/O.
# Legacy reference: engine/common/cmd.c, base_cmd.c, cvar.c, con_utils.c

# GoldSrc compatibility quirks (HL25 cvar redirect, HACKS_RELATED_HLMODS, etc.)
# When OFF, the null policy is linked instead and core files have zero #ifdefs.
option(XASH_GOLDSRC_COMPAT "Enable GoldSrc/HL1 compatibility quirks in cmd_cvar" ON)

if(XASH_GOLDSRC_COMPAT)
    set(XASH_CMD_CVAR_COMPAT_SRC compat_goldsrc.cpp)
else()
    set(XASH_CMD_CVAR_COMPAT_SRC compat_null.cpp)
endif()

add_library(xash3dpp_cmd_cvar STATIC
    base_cmd.cpp
    cvar.cpp
    cmd.cpp
    context.cpp
    context_init.cpp
    context_misc.cpp
    cvar_ops.cpp
    cmd_ops.cpp
    cmd_dispatch.cpp
    ${XASH_CMD_CVAR_COMPAT_SRC}
)

target_include_directories(xash3dpp_cmd_cvar
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_cmd_cvar PUBLIC cxx_std_20)

target_link_libraries(xash3dpp_cmd_cvar
    PUBLIC  xash3dpp_utilities
    PUBLIC  xash3dpp_memory
    PRIVATE xash3dpp_platform
)

if(XASH_GOLDSRC_COMPAT)
    target_compile_definitions(xash3dpp_cmd_cvar PRIVATE XASH_GOLDSRC_COMPAT=1)
endif()
