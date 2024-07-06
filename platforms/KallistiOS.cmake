project(RetroEngine)

add_executable(RetroEngine ${RETRO_FILES})

if(NOT GAME_STATIC)
    message(FATAL_ERROR "GAME_STATIC must be on")
endif()

# RETRO_REVISION 3 (v5U) will most likely never fit
if(RETRO_REVISION EQUAL 3)
    message(FATAL_ERROR "RETRO_REVISION=${RETRO_REVISION} is not supported, use 2 instead")
endif()

target_include_directories(RetroEngine INTERFACE "${KOS_PORTS}")

# Enable debugging by default on both the engine and game
option(RSDK_DEBUG "Enable debugging" ON)
target_compile_definitions(RetroEngine PUBLIC RSDK_DEBUG=$<BOOL:${RSDK_DEBUG}>)
target_compile_definitions(${GAME_NAME} PUBLIC RSDK_DEBUG=$<BOOL:${RSDK_DEBUG}>)

option(KOS_USER_DIR "Root directory fo the KOS VFS which get set as the RSDK User directory." "/pc/")
target_compile_definitions(RetroEngine PUBLIC KOS_USER_DIR="${KOS_USER_DIR}")

# Disable some unneeded features to reduce executable size
set(RETRO_MOD_LOADER OFF CACHE BOOL "Disable mod loader" FORCE)
set(GAME_INCLUDE_EDITOR OFF CACHE BOOL "Disable unused editor code" FORCE)
