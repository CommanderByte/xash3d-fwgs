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
    xash3dpp_add_test(${test_name} LIBS xash3dpp_utilities)
endforeach()
