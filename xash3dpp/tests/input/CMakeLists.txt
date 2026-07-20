cmake_minimum_required(VERSION 3.16)

# cmd_cvar_test_stubs (TrustedOracle/UntrustedOracle/NullPolicy) is defined
# in tests/cmd_cvar/CMakeLists.txt, processed earlier in the top-level build.
xash3dpp_add_test(test_input_lifecycle LIBS xash3dpp_input)
xash3dpp_add_test(test_bindings_commands LIBS xash3dpp_input cmd_cvar_test_stubs)
xash3dpp_add_test(test_joy_gyro LIBS xash3dpp_input cmd_cvar_test_stubs)
xash3dpp_add_test(test_move_assembly LIBS xash3dpp_input cmd_cvar_test_stubs)
xash3dpp_add_test(test_touch_osk LIBS xash3dpp_input cmd_cvar_test_stubs)
xash3dpp_add_test(test_event_script_integration LIBS xash3dpp_input cmd_cvar_test_stubs)
