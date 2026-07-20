# xash3dpp — input subsystem
# Keyboard, mouse, gamepad, gyroscope, touch, on-screen keyboard.
# Legacy reference: engine/client/input/input.c, in_*.c (keys, joy, gyro, touch, osk)
#
# Satellite-component decision (Q-11): touch/OSK/joystick/device-gyro all
# funnel into the same IN_CollectInput/Key_Event dispatch points and none has
# an external dependency beyond IEventSource/filesystem — none clears the
# Q-11 bar for a separate CMake target. Organized as sibling source
# directories (keys/, joy/, touch/, osk/) inside one xash3dpp_input target
# (input-boundary.md "Satellite components").
#
# Dedicated-linkage rule (Q-23): xash3dpp_input is a LEAF static lib — no
# host/server/launcher edge. Depends only on xash3dpp_cmd_cvar (command/cvar
# registration) and xash3dpp_memory (Q-22 pool-owned Input factory).

add_library(xash3dpp_input STATIC
    input.cpp
    move.cpp
    keys/key_table.cpp
    keys/key_routing.cpp
    keys/commands.cpp
    joy/joy_model.cpp
    touch/touch_model.cpp
    touch/touch.cpp
    osk/osk_model.cpp
)

target_include_directories(xash3dpp_input
    PUBLIC ${PROJECT_SOURCE_DIR}/include
)

target_compile_features(xash3dpp_input PUBLIC cxx_std_23)

target_link_libraries(xash3dpp_input
    PUBLIC  xash3dpp_memory
    PUBLIC  xash3dpp_cmd_cvar
    PRIVATE xash3dpp_core
)
