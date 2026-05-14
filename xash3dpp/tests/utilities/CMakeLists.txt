set(test_sources
    test_atlas
    test_build
    test_dynlib
    test_gameinfo_parser
    test_hash
    test_math
    test_matrix
    test_path
    test_string
    test_swap
    test_utf
)

foreach(test_name IN LISTS test_sources)
    add_executable(${test_name} ${test_name}.cpp)
    target_link_libraries(${test_name} PRIVATE xash3dpp_utilities)
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
