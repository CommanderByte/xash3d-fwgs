# xash3dpp — test registration helper
#
# xash3dpp_add_test(<name> [SOURCE <path>] LIBS <lib>...)
#
# Collapses the add_executable / target_link_libraries / add_test triple
# that every test target repeats. SOURCE defaults to <name>.cpp; pass it
# explicitly for satellite-subfolder sources whose target name differs from
# the path (e.g. test_bsp_header <- bsp/test_bsp_header.cpp).

function(xash3dpp_add_test name)
    cmake_parse_arguments(ARG "" "SOURCE" "LIBS" ${ARGN})
    if(NOT ARG_SOURCE)
        set(ARG_SOURCE ${name}.cpp)
    endif()
    add_executable(${name} ${ARG_SOURCE})
    target_link_libraries(${name} PRIVATE ${ARG_LIBS})
    add_test(NAME ${name} COMMAND ${name})
endfunction()
