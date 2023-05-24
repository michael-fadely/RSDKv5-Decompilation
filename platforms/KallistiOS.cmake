project(RetroEngine)

add_executable(RetroEngine ${RETRO_FILES})

set(GAME_STATIC ON CACHE BOOL "Dreamcast may only use static binaries.")

target_link_libraries(RetroEngine m)
target_include_directories(RetroEngine INTERFACE "${KOS_PORTS}")

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(RSDK_DEBUG ON)
    target_compile_definitions(RetroEngine PUBLIC RSDK_DEBUG=1)
endif()

set(RETRO_MOD_LOADER OFF)
