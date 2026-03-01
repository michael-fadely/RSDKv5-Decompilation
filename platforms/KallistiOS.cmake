add_executable(RetroEngine ${RETRO_FILES}
    RSDKv5/RSDK/Graphics/KallistiOS/AniTileTracker.cpp
)

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

option(KOS_USER_DIR "Root directory for the KOS VFS which get set as the RSDK User directory." "/cd/")
target_compile_definitions(RetroEngine PUBLIC KOS_USER_DIR="${KOS_USER_DIR}")

# Disable some unneeded features to reduce executable size
set(RETRO_MOD_LOADER OFF CACHE BOOL "Disable mod loader" FORCE)
set(GAME_INCLUDE_EDITOR OFF CACHE BOOL "Disable unused editor code" FORCE)

if(NOT RSDK_DEBUG)
    # Translation units with hot code which needs high optimization levels.
    set(CRITICAL_FILES
        RSDKv5/RSDK/Core/Math.cpp
        RSDKv5/RSDK/Graphics/Drawing.cpp
        RSDKv5/RSDK/Graphics/Scene3D.cpp
        RSDKv5/RSDK/Scene/Collision.cpp
        RSDKv5/RSDK/Scene/Scene.cpp
    )

    set(OTHER_FILES
        RSDKv5/main.cpp
        RSDKv5/RSDK/Core/RetroEngine.cpp
        RSDKv5/RSDK/Core/Reader.cpp
        RSDKv5/RSDK/Core/Link.cpp
        RSDKv5/RSDK/Core/ModAPI.cpp
        RSDKv5/RSDK/Dev/Debug.cpp
        RSDKv5/RSDK/Storage/Storage.cpp
        RSDKv5/RSDK/Storage/Text.cpp
        RSDKv5/RSDK/Graphics/Animation.cpp
        RSDKv5/RSDK/Graphics/Sprite.cpp
        RSDKv5/RSDK/Graphics/Palette.cpp
        RSDKv5/RSDK/Graphics/Video.cpp
        RSDKv5/RSDK/Audio/Audio.cpp
        RSDKv5/RSDK/Input/Input.cpp
        RSDKv5/RSDK/Scene/Object.cpp
        RSDKv5/RSDK/Scene/Objects/DefaultObject.cpp
        RSDKv5/RSDK/Scene/Objects/DevOutput.cpp
        RSDKv5/RSDK/User/Core/UserAchievements.cpp
        RSDKv5/RSDK/User/Core/UserCore.cpp
        RSDKv5/RSDK/User/Core/UserLeaderboards.cpp
        RSDKv5/RSDK/User/Core/UserPresence.cpp
        RSDKv5/RSDK/User/Core/UserStats.cpp
        RSDKv5/RSDK/User/Core/UserStorage.cpp
        dependencies/all/tinyxml2/tinyxml2.cpp
        dependencies/all/iniparser/iniparser.cpp
        dependencies/all/iniparser/dictionary.cpp
        dependencies/all/miniz/miniz.c
    )

    # Force our list of critical files to get built with -O3 optimizaiton levels.
    set_source_files_properties(${CRITICAL_FILES} PROPERTIES COMPILE_FLAGS "-O3")

    # Force our list of other files to get built with -Os optimizaiton levels.
    set_source_files_properties(${OTHER_FILES} PROPERTIES COMPILE_FLAGS "-Os")
endif()
