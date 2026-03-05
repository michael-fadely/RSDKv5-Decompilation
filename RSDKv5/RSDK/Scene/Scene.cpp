#include "RSDK/Core/RetroEngine.hpp"

#ifdef KOS_HARDWARE_RENDERER
#include <RSDK/Graphics/KallistiOS/AniTileTracker.hpp>

//! Calculates 1.0f/sqrtf( \p x ), using a fast approximation.
__always_inline float shz_inverse_sqrtf(float x) {
    asm("fsrra %0" : "+f" (x));
    return x;
}

__always_inline float shz_invf(float x) {
    float rx = shz_inverse_sqrtf(x * x);
    return x < 0 ? -rx : rx;
}

__always_inline float shz_divf(float num, float denom) {
    return num * shz_invf(denom);
}
#endif

using namespace RSDK;

#if RETRO_REV0U
#include "Legacy/SceneLegacy.cpp"
#endif

#if !defined(KOS_HARDWARE_RENDERER)
uint8 RSDK::tilesetPixels[TILESET_SIZE * 4];
#endif

ScanlineInfo *RSDK::scanlines = NULL;
TileLayer RSDK::tileLayers[LAYER_COUNT];
CollisionMask RSDK::collisionMasks[CPATH_COUNT][TILE_COUNT * 4];
TileInfo RSDK::tileInfo[CPATH_COUNT][TILE_COUNT * 4];

#if RETRO_REV02
bool32 RSDK::forceHardReset = false;
#endif
char RSDK::currentSceneFolder[0x10];
char RSDK::currentSceneID[0x10];

SceneInfo RSDK::sceneInfo;

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
// this is used to hold a copy of tilesetPixels as loaded directly from 16x16Tiles.gif
// just long enough to generate LOD map texture
uint8_t* persistTiles = nullptr;
// a GFXSurface used to draw LOD polys with RenderDevice
GFXSurface mapSurf{};

uint16 firstNonEmptyRow = 0xffff;
uint16 lastNonEmptyRow = 0xffff;
uint16 __attribute__((aligned(32))) firstNonemptyTileInRow[512];
uint16 __attribute__((aligned(32))) lastNonemptyTileInRow[512];

// are we in a ufo stage and if so, which one
static int ufoNum = 0;

// LOD background quad state
static bool lodTextureReady = false;
static int lodTexWidth = 0;
static int lodTexHeight = 0;
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
static void ReleaseLODTexture()
{
    if (mapSurf.texture != nullptr) {
        pvr_mem_free(mapSurf.texture);
        mapSurf.texture = nullptr;
    }
}

static void GenerateLODTexture()
{
    lodTextureReady = false;

    if (persistTiles == nullptr) {
        printf("[LOD] can't generate LOD texture when tileset not staged in RAM\n");
        return;
    }

    ReleaseLODTexture();

    if (ufoNum) {
        uint8 *tilesetData = persistTiles;

        mapSurf.isARGB = 1;
        mapSurf.isVq = 0;
        mapSurf.width = 512;
        mapSurf.height = 512;

        // need to check for this name hash to be sure it is the correct rotozoom layer
        const char *layerName = "Playfield";
        RETRO_HASH_MD5(hash);
        GEN_HASH_MD5(layerName, hash);

        // Find the rotozoom layer
        TileLayer *rotoLayer = nullptr;
        for (int l = 0; l < LAYER_COUNT; l++) {
            if (tileLayers[l].type == LAYER_ROTOZOOM && tileLayers[l].layout) {
                if (HASH_MATCH_MD5(tileLayers[l].name, hash)) {
                    rotoLayer = &tileLayers[l];
                    break;
                }
            }
        }

        if (!rotoLayer) {
            // clean up the tileset copy
            UnPinStorage(reinterpret_cast<void**>(&persistTiles));
            RemoveStorageEntry(reinterpret_cast<void**>(&persistTiles));
            persistTiles = nullptr;
            return;
        }

        int mapW = rotoLayer->xsize;
        int mapH = rotoLayer->ysize;
        if (mapW == 0 || mapH == 0) {
            // clean up the tileset copy
            UnPinStorage(reinterpret_cast<void**>(&persistTiles));
            RemoveStorageEntry(reinterpret_cast<void**>(&persistTiles));
            persistTiles = nullptr;
            return;
        }
        // Power-of-2 texture dimensions
        lodTexWidth = 512;
        if (mapW > 512) lodTexWidth = 1024;
        lodTexHeight = 512;
        if (mapH > 512) lodTexHeight = 1024;
        mapSurf.width = lodTexWidth;
        mapSurf.height = lodTexHeight;
        mapSurf.texture = pvr_mem_malloc(lodTexWidth * lodTexHeight * 2);
        if (mapSurf.texture == nullptr) {
            printf("[LOD] Failed to allocate vram for LOD texture\n");
            // clean up the tileset copy
            UnPinStorage(reinterpret_cast<void**>(&persistTiles));
            RemoveStorageEntry(reinterpret_cast<void**>(&persistTiles));
            persistTiles = nullptr;
            return;
        }

        size_t texSize = (size_t)lodTexWidth * lodTexHeight * sizeof(uint16);

        // Allocate temp buffer
        uint16 *lodPixels = nullptr;
        AllocateStorage(reinterpret_cast<void **>(&lodPixels), texSize, DATASET_TMP, true);
        if (!lodPixels) {
            printf("[LOD] Failed to allocate temp buffer for LOD texture\n");
            // clean up the tileset copy
            UnPinStorage(reinterpret_cast<void**>(&persistTiles));
            RemoveStorageEntry(reinterpret_cast<void**>(&persistTiles));
            persistTiles = nullptr;
            return;
        }

        // Build LOD texture: one texel per tile
        uint16 *layout = rotoLayer->layout;
        int widthShift = rotoLayer->widthShift;
        uint16 *activePalette = fullPalette[0];

        for (int32 y = 0; y < mapH; ++y) {
            for (int32 x = 0; x < mapW; ++x) {
                uint16_t tile = layout[x + (y << widthShift)];
                if (tile == 0xFFFF) {
                    lodPixels[x + (y*lodTexWidth)] = 0;
                } else {
                    uint32_t r=0;
                    uint32_t g=0;
                    uint32_t b=0;
                    int nonzero = 0;
                    for (int h=0;h<16;h++) {
                        for(int w=0;w<16;w++) {
                            uint8_t index = tilesetData[TILE_SIZE * (h + TILE_SIZE * (tile&0x3ff)) + w];
                            nonzero+=8;

                            uint16 c = activePalette[index];
                            uint8_t r5 = (c >> 11) & 0x1F;
                            uint8_t g6 = (c >>  5) & 0x3F;
                            uint8_t b5 = (c      ) & 0x1F;
                            r5 = (r5 * 255) / 31;
                            g6 = (g6 * 255) / 63;
                            b5 = (b5 * 255) / 31;

                            if (r5 == 255 && g6 == 0 && b5 == 255) {
                                nonzero-=8;
                                continue;
                            } else if (r5 < 16 && g6 > 224 && b5 < 16) {
                                nonzero-=8;
                                continue;
                            }

                            r += r5;
                            g += g6;
                            b += b5;
                        }
                    }
                    if (!nonzero) {
                        lodPixels[x + (y*lodTexWidth)] = 0;
                    } else {
                        r = (r / nonzero);
                        g = (g / nonzero);
                        b = (b / nonzero);
                        lodPixels[x + (y*lodTexWidth)] = 0x8000 | (r << 10) | (g << 5) | b;
                    }
                }
            }
        }

        // Upload with auto-twiddling for 16bpp
        pvr_txr_load_ex(lodPixels, mapSurf.texture, lodTexWidth, lodTexHeight, PVR_TXRLOAD_16BPP);

        printf("[LOD] Generated %dx%d LOD texture for %dx%d map (%zu bytes VRAM)\n",
            lodTexWidth, lodTexHeight, mapW, mapH, texSize);

        lodTextureReady = true;

        // Free temp buffer
        RemoveStorageEntry(reinterpret_cast<void **>(&lodPixels));
        // Free tileset copy
        UnPinStorage(reinterpret_cast<void**>(&persistTiles));
        RemoveStorageEntry(reinterpret_cast<void**>(&persistTiles));
        persistTiles = nullptr;
    }
}
#endif

void RSDK::LoadSceneFolder()
{
#if RETRO_PLATFORM == RETRO_ANDROID
    ShowLoadingIcon();
#endif

#if RETRO_USE_MOD_LOADER
    // run this before the game actually unloads all the objects & scene assets
    RunModCallbacks(MODCB_ONSTAGEUNLOAD, NULL);
#endif

    sceneInfo.timeCounter  = 0;
    sceneInfo.minutes      = 0;
    sceneInfo.seconds      = 0;
    sceneInfo.milliseconds = 0;

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    ReleaseLODTexture();
    // book-keeping, are we in a ufo stage
    ufoNum = 0;
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO1")) {
        ufoNum = 1;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO2")) {
        ufoNum = 2;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO3")) {
        ufoNum = 3;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO4")) {
        ufoNum = 4;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO5")) {
        ufoNum = 5;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO6")) {
        ufoNum = 6;
    }
    if (strstr(sceneInfo.listData[sceneInfo.listPos].folder, "UFO7")) {
        ufoNum = 7;
    }
#endif

    // clear draw groups
    for (int32 i = 0; i < DRAWGROUP_COUNT; ++i) {
        drawGroups[i].entityCount = 0;
        drawGroups[i].layerCount  = 0;
    }

    // Clear Type groups
    for (int32 i = 0; i < TYPEGROUP_COUNT; ++i) {
        typeGroups[i].entryCount = 0;
    }

#if RETRO_REV02
    // Unload debug values
    ClearViewableVariables();

    // "unload" tint table
    tintLookupTable = NULL;
#endif

    // Unload TileLayers
    for (int32 l = 0; l < LAYER_COUNT; ++l) {
        MEM_ZERO(tileLayers[l]);
        for (int32 c = 0; c < CAMERA_COUNT; ++c) tileLayers[l].drawGroup[c] = -1;
    }

    SceneListInfo *list = &sceneInfo.listCategory[sceneInfo.activeCategory];
#if RETRO_REV02
    if (strcmp(currentSceneFolder, sceneInfo.listData[sceneInfo.listPos].folder) == 0 && !forceHardReset) {
        // Reload
        DefragmentAndGarbageCollectStorage(DATASET_STG);
        sceneInfo.filter = sceneInfo.listData[sceneInfo.listPos].filter;
        // DCWIP: added leading \n
        PrintLog(PRINT_NORMAL, "\nReloading Scene \"%s - %s\" with filter %d", list->name, sceneInfo.listData[sceneInfo.listPos].name,
                 sceneInfo.listData[sceneInfo.listPos].filter);

#if RETRO_USE_MOD_LOADER
        // reload object hooks
        for (int32 h = 0; h < (int32)objectHookList.size(); ++h) {
            for (int32 i = 0; i < objectClassCount; ++i) {
                if (HASH_MATCH_MD5(objectClassList[i].hash, objectHookList[h].hash)) {
                    if (objectHookList[h].staticVars && objectClassList[i].staticVars)
                        *objectHookList[h].staticVars = *objectClassList[i].staticVars;
                    break;
                }
            }
        }
#endif

        return;
    }
#endif

#if !RETRO_REV02
    if (strcmp(currentSceneFolder, sceneInfo.listData[sceneInfo.listPos].folder) == 0) {
        // Reload
        DefragmentAndGarbageCollectStorage(DATASET_STG);
        PrintLog(PRINT_NORMAL, "Reloading Scene \"%s - %s\"", list->name, sceneInfo.listData[sceneInfo.listPos].name);

#if RETRO_USE_MOD_LOADER
        // reload object hooks
        for (int32 h = 0; h < (int32)objectHookList.size(); ++h) {
            for (int32 i = 0; i < objectClassCount; ++i) {
                if (HASH_MATCH_MD5(objectClassList[i].hash, objectHookList[h].hash)) {
                    if (objectHookList[h].staticVars && objectClassList[i].staticVars)
                        *objectHookList[h].staticVars = *objectClassList[i].staticVars;
                    break;
                }
            }
        }
#endif
        return;
    }
#endif

    // Unload stage 3DScenes & models
    Clear3DScenes();

    // Unload stage sprite animations
    ClearSpriteAnimations();

    // Unload stage surfaces
    ClearGfxSurfaces();

    // Unload stage sfx & audio channels
    ClearStageSfx();

    // Unload stage objects
    ClearStageObjects();

    // Clear draw groups
    for (int32 l = 0; l < DRAWGROUP_COUNT; ++l) {
        MEM_ZERO(drawGroups[l]);
        drawGroups[l].sorted = false;
    }

    // Clear stage storage
    DefragmentAndGarbageCollectStorage(DATASET_STG);
#if RETRO_PLATFORM != RETRO_KALLISTIOS
     DefragmentAndGarbageCollectStorage(DATASET_SFX);
#endif

    for (int32 s = 0; s < SCREEN_COUNT; ++s) {
        screens[s].position.x = 0;
        screens[s].position.y = 0;
    }

    SceneListEntry *sceneEntry = &sceneInfo.listData[sceneInfo.listPos];
    strcpy(currentSceneFolder, sceneEntry->folder);

#if RETRO_REV02
    forceHardReset   = false;
    sceneInfo.filter = sceneEntry->filter;
    // DCWIP: added leading \n
    PrintLog(PRINT_NORMAL, "\nLoading Scene \"%s - %s\" with filter %d", list->name, sceneEntry->name, sceneEntry->filter);
#endif

#if !RETRO_REV02
    PrintLog(PRINT_NORMAL, "Loading Scene \"%s - %s\"", list->name, sceneEntry->name);
#endif

    char fullFilePath[0x40];

    // Load TileConfig
    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Stages/%s/TileConfig.bin", currentSceneFolder);
    LoadTileConfig(fullFilePath);

    // Load StageConfig
    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Stages/%s/StageConfig.bin", currentSceneFolder);

    FileInfo info;
    InitFileInfo(&info);
    if (LoadFile(&info, fullFilePath, FMODE_RB)) {
        uint32 sig = ReadInt32(&info, false);

        if (sig != RSDK_SIGNATURE_CFG) {
            CloseFile(&info);
            return;
        }

        sceneInfo.useGlobalObjects = ReadInt8(&info);
        sceneInfo.classCount       = 0;

        if (sceneInfo.useGlobalObjects) {
            for (int32 o = 0; o < globalObjectCount; ++o) stageObjectIDs[o] = globalObjectIDs[o];
            sceneInfo.classCount = globalObjectCount;
        }
        else {
            for (int32 o = 0; o < TYPE_DEFAULT_COUNT; ++o) stageObjectIDs[o] = globalObjectIDs[o];

            sceneInfo.classCount = TYPE_DEFAULT_COUNT;
        }

        uint8 objectCount = ReadInt8(&info);
        for (int32 o = 0; o < objectCount; ++o) {
            ReadString(&info, textBuffer);

            RETRO_HASH_MD5(hash);
            GEN_HASH_MD5(textBuffer, hash);

            stageObjectIDs[sceneInfo.classCount] = 0;
            for (int32 id = 0; id < objectClassCount; ++id) {
                if (HASH_MATCH_MD5(hash, objectClassList[id].hash)) {
                    stageObjectIDs[sceneInfo.classCount] = id;
                    sceneInfo.classCount++;
                }
            }
        }

        for (int32 o = 0; o < sceneInfo.classCount; ++o) {
            ObjectClass *objClass = &objectClassList[stageObjectIDs[o]];
            if (objClass->staticVars && !*objClass->staticVars) {
                AllocateStorage((void **)objClass->staticVars, objClass->staticClassSize, DATASET_STG, true);

#if RETRO_REV0U
                if (objClass->staticLoad)
                    objClass->staticLoad(*objClass->staticVars);
                else
                    LoadStaticVariables((uint8 *)*objClass->staticVars, objClass->hash, sizeof(Object));

#else
                LoadStaticVariables((uint8 *)*objClass->staticVars, objClass->hash, sizeof(Object));
#endif

#if RETRO_USE_MOD_LOADER
                // even though the static load event is rev0U only, this point in the engine is "static loading"
                RunModCallbacks(MODCB_ONSTATICLOAD, (void *)objClass);
#endif

#if RETRO_USE_MOD_LOADER
                for (ModInfo &mod : modList) {
                    if (mod.staticVars.find(objClass->hash) != mod.staticVars.end()) {
                        auto sVars = mod.staticVars.at(objClass->hash);
                        RegisterStaticVariables((void **)sVars.staticVars, sVars.name.c_str(), sVars.size);
                    }
                }
#endif

                (*objClass->staticVars)->classID = o;
                if (o >= TYPE_DEFAULT_COUNT)
                    (*objClass->staticVars)->active = ACTIVE_NORMAL;
            }
        }

        for (int32 p = 0; p < PALETTE_BANK_COUNT; ++p) {
            activeStageRows[p] = ReadInt16(&info);

            for (int32 r = 0; r < 0x10; ++r) {
                if ((activeStageRows[p] >> r & 1)) {
                    for (int32 c = 0; c < 0x10; ++c) {
                        uint8 red                     = ReadInt8(&info);
                        uint8 green                   = ReadInt8(&info);
                        uint8 blue                    = ReadInt8(&info);
                        #if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
                        stagePalette[p][(r << 4) + c] = rgb32To16_B[blue] | rgb32To16_G[green] | rgb32To16_R[red];
                        #else
                        stagePalette[p][(r << 4) + c] = PACK_RGB888(red, green, blue);
                        #endif
                    }
                }
                else {
                    for (int32 c = 0; c < 0x10; ++c) stagePalette[p][(r << 4) + c] = 0;
                }
            }
        }

        uint8 sfxCount = ReadInt8(&info);
        char sfxPath[0x100];
        for (int32 i = 0; i < sfxCount; ++i) {
            ReadString(&info, sfxPath);
            uint8 maxConcurrentPlays = ReadInt8(&info);
            LoadSfx(sfxPath, maxConcurrentPlays, SCOPE_STAGE);
        }

        CloseFile(&info);
    }

#ifdef KOS_HARDWARE_RENDERER
    AniTileTracker::ResetAllTiles();
#endif

    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Stages/%s/16x16Tiles.gif", currentSceneFolder);
    LoadStageGIF(fullFilePath);

#if RETRO_USE_MOD_LOADER
    for (int32 h = 0; h < (int32)objectHookList.size(); ++h) {
        for (int32 i = 0; i < objectClassCount; ++i) {
            if (HASH_MATCH_MD5(objectClassList[i].hash, objectHookList[h].hash)) {
                if (objectHookList[h].staticVars && objectClassList[i].staticVars)
                    *objectHookList[h].staticVars = *objectClassList[i].staticVars;
                break;
            }
        }
    }
#endif
}
void RSDK::LoadSceneAssets()
{
#if RETRO_PLATFORM == RETRO_ANDROID
    ShowLoadingIcon();
#endif

    memset(objectEntityList, 0, ENTITY_COUNT * sizeof(EntityBase));

    SceneListEntry *sceneEntry = &sceneInfo.listData[sceneInfo.listPos];

    char fullFilePath[0x40];
    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Stages/%s/Scene%s.bin", currentSceneFolder, sceneEntry->id);

    dataStorage[DATASET_TMP].usedStorage = 0;

    for (int32 s = 0; s < SCREEN_COUNT; ++s) screens[s].waterDrawPos = screens[s].size.y;

    if (screens[0].size.y > 0)
        memset(gfxLineBuffer, 0, screens[0].size.y * sizeof(uint8));

    memset(tileLayers, 0, LAYER_COUNT * sizeof(TileLayer));

#if RETRO_PLATFORM == RETRO_KALLISTIOS
    PaletteFlags::MarkAllDirty();
#endif

    // Reload palette
    for (int32 b = 0; b < 8; ++b) {
        for (int32 r = 0; r < 0x10; ++r) {
            if ((activeGlobalRows[b] >> r & 1)) {
                for (int32 c = 0; c < 0x10; ++c) fullPalette[b][(r << 4) + c] = globalPalette[b][(r << 4) + c];
            }

            if ((activeStageRows[b] >> r & 1)) {
                for (int32 c = 0; c < 0x10; ++c) fullPalette[b][(r << 4) + c] = stagePalette[b][(r << 4) + c];
            }
        }
    }

    FileInfo info;
    InitFileInfo(&info);
    if (LoadFile(&info, fullFilePath, FMODE_RB)) {
        uint32 sig = ReadInt32(&info, false);

        if (sig != RSDK_SIGNATURE_SCN) {
            CloseFile(&info);
            return;
        }

        // Editor Metadata

        // I'm leaving this section here so that the "format" can be documented, since the official code is 3 lines and just skips it lol

        /*
        uint8 unknown1 = ReadInt8(&info); // usually 3, sometimes 4, LRZ1 (old) is 2

        uint8 b                = ReadInt8(&info);
        uint8 g                = ReadInt8(&info);
        uint8 r                = ReadInt8(&info);
        uint8 a                = ReadInt8(&info);
        color backgroundColor1 = (a << 24) | (r << 16) | (g << 8) | (b << 0);

        b                      = ReadInt8(&info);
        g                      = ReadInt8(&info);
        r                      = ReadInt8(&info);
        a                      = ReadInt8(&info);
        color backgroundColor2 = (a << 24) | (r << 16) | (g << 8) | (b << 0);

        uint8 unknown2 = ReadInt8(&info); // always 1 afaik
        uint8 unknown3 = ReadInt8(&info); // always 1 afaik
        uint8 unknown4 = ReadInt8(&info); // always 4 afaik
        uint8 unknown5 = ReadInt8(&info); // always 0 afaik
        uint8 unknown6 = ReadInt8(&info); // always 1 afaik
        uint8 unknown7 = ReadInt8(&info); // always 4 afaik
        uint8 unknown8 = ReadInt8(&info); // always 0 afaik

        char stampName[0x20];
        ReadString(&info, stampName);

        uint8 unknown9 = ReadInt8(&info); // usually 3, 4, or 5
        */

        // Skip over Metadata, since we won't be using it at all in-game
        //uint8 unknown1 = ReadInt8(&info); // usually 3, sometimes 4, LRZ1 (old) is 2
        // bg color
        //uint8 r                = ReadInt8(&info);
        //uint8 g                = ReadInt8(&info);
        //uint8 b                = ReadInt8(&info);
        //uint8 a                = ReadInt8(&info);
        //color backgroundColor1 = (a << 24) | (r << 16) | (g << 8) | (b << 0);

        //b                      = ReadInt8(&info);
        //g                      = ReadInt8(&info);
        //r                      = ReadInt8(&info);
        //a                      = ReadInt8(&info);
        //color backgroundColor2 = (a << 24) | (r << 16) | (g << 8) | (b << 0);
        //Seek_Cur(&info, 0x10 - 4);
        Seek_Cur(&info, 0x10);
        uint8 strLen = ReadInt8(&info);
        Seek_Cur(&info, strLen + 1);

        // Tile Layers
        uint8 layerCount = ReadInt8(&info);
        for (int32 l = 0; l < layerCount; ++l) {
            TileLayer *layer = &tileLayers[l];

            // Tests in RetroED & comparing images of the RSDKv5 editor we have puts this as the most likely use for this (otherwise unused) variable
            bool32 visibleInEditor = ReadInt8(&info) != 0;
            (void)visibleInEditor; // unused

            ReadString(&info, textBuffer);
            GEN_HASH_MD5(textBuffer, layer->name);

            layer->type         = ReadInt8(&info);
            layer->drawGroup[0] = ReadInt8(&info);
            for (int32 s = 1; s < CAMERA_COUNT; ++s) layer->drawGroup[s] = layer->drawGroup[0];

            layer->xsize = ReadInt16(&info);
            int32 shift  = 1;
            int32 shift2 = 1;
            int32 val    = 0;
            do {
                shift = shift2;
                val   = 1 << shift2++;
            } while (val < layer->xsize);
            layer->widthShift = shift;

            layer->ysize = ReadInt16(&info);
            shift        = 1;
            shift2       = 1;
            val          = 0;
            do {
                shift = shift2;
                val   = 1 << shift2++;
            } while (val < layer->ysize);
            layer->heightShift = shift;

            layer->parallaxFactor = ReadInt16(&info);
            layer->scrollSpeed    = ReadInt16(&info) << 8;
            layer->scrollPos      = 0;

            layer->layout = NULL;
            if (layer->xsize || layer->ysize) {
                AllocateStorage((void **)&layer->layout, sizeof(uint16) * (1UL << layer->widthShift) * (1UL << layer->heightShift), DATASET_STG, true);
                memset(layer->layout, 0xFF, sizeof(uint16) * (1UL << layer->widthShift) * (1UL << layer->heightShift));
            }

            int32 size = layer->xsize;
            if (size <= layer->ysize)
                size = layer->ysize;
            AllocateStorage((void **)&layer->lineScroll, TILE_SIZE * size, DATASET_STG, true);

            layer->scrollInfoCount = ReadInt16(&info);
            for (int32 s = 0; s < layer->scrollInfoCount; ++s) {
                layer->scrollInfo[s].parallaxFactor = ReadInt16(&info);
                layer->scrollInfo[s].scrollSpeed    = ReadInt16(&info) << 8;
                layer->scrollInfo[s].scrollPos      = 0;
                layer->scrollInfo[s].tilePos        = 0;
                layer->scrollInfo[s].deform         = ReadInt8(&info);

                // this isn't used anywhere in-engine, and is never set in the files. so as you might expect, no one knows what it is for!
                layer->scrollInfo[s].unknown = ReadInt8(&info);
            }

            uint8 *scrollIndexes = NULL;
            ReadCompressed(&info, (uint8 **)&scrollIndexes);
            memcpy(layer->lineScroll, scrollIndexes, TILE_SIZE * size * sizeof(uint8));
#if !RETRO_USE_ORIGINAL_CODE
            RemoveStorageEntry((void **)&scrollIndexes);
#endif
            scrollIndexes = NULL;

            uint8 *tileLayout = NULL;
            ReadCompressed(&info, (uint8 **)&tileLayout);

            int32 id = 0;
            for (int32 y = 0; y < layer->ysize; ++y) {
                for (int32 x = 0; x < layer->xsize; ++x) {
                    layer->layout[x + (y << layer->widthShift)] = (tileLayout[id + 1] << 8) + tileLayout[id + 0];
                    id += 2;
                }
            }

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
            firstNonEmptyRow = 0xffff;
            lastNonEmptyRow = 0xffff;

            // record the x position of the first non-empty tile in each row
            for (int32 y = 0; y < layer->ysize; y++) {
                bool empty = true;
                for (int32 x = 0; x < layer->xsize; x++) {
                    uint16 tile = layer->layout[x + (y << layer->widthShift)];
                    if (tile == 0xffff) {
                        continue;
                    }
                    empty = false;
                    firstNonemptyTileInRow[y] = x;
                    break;
                }
                if(!empty && firstNonEmptyRow == 0xffff) {
                    firstNonEmptyRow = y;
                }
            }

            // record the x position of the last non-empty tile in each row
            for (int32 y = layer->ysize - 1; y >= 0; y--) {
                bool empty = true;
                for (int32 x = layer->xsize - 1; x >= 0; x--) {
                    uint16 tile = layer->layout[x + (y << layer->widthShift)];
                    if (tile == 0xffff) {
                        continue;
                    }
                    lastNonemptyTileInRow[y] = x;
                    empty = false;
                    break;
                }
                if(!empty && lastNonEmptyRow == 0xffff) {
                    lastNonEmptyRow = y;
                }
            }

#endif

#if !RETRO_USE_ORIGINAL_CODE
            RemoveStorageEntry((void **)&tileLayout);
#endif
            tileLayout = NULL;
        }

        // Objects
        uint8 objectCount = ReadInt8(&info);
        editableVarList   = NULL;
        AllocateStorage((void **)&editableVarList, sizeof(EditableVarInfo) * EDITABLEVAR_COUNT, DATASET_TMP, false);

#if RETRO_REV02
        EntityBase *tempEntityList = NULL;
        AllocateStorage((void **)&tempEntityList, SCENEENTITY_COUNT * sizeof(EntityBase), DATASET_TMP, true);
#endif

        for (int32 i = 0; i < objectCount; ++i) {
            RETRO_HASH_MD5(objHash);
            objHash[0] = ReadInt32(&info, false);
            objHash[1] = ReadInt32(&info, false);
            objHash[2] = ReadInt32(&info, false);
            objHash[3] = ReadInt32(&info, false);

            int32 classID = 0;
            for (int32 o = 0; o < sceneInfo.classCount; ++o) {
                if (HASH_MATCH_MD5(objHash, objectClassList[stageObjectIDs[o]].hash)) {
                    classID = o;
                    break;
                }
            }

#if !RETRO_USE_ORIGINAL_CODE
            if (!classID && i >= TYPE_DEFAULT_COUNT)
                PrintLog(PRINT_NORMAL, "Object Class %d is unimplemented!", i);
#endif

            ObjectClass *objectClass = &objectClassList[stageObjectIDs[classID]];

            uint8 varCount           = ReadInt8(&info);
            EditableVarInfo *varList = NULL;
            AllocateStorage((void **)&varList, sizeof(EditableVarInfo) * varCount, DATASET_TMP, false);
            editableVarCount = 0;
            if (classID) {
#if RETRO_REV02
                SetEditableVar(VAR_UINT8, "filter", classID, offsetof(Entity, filter));
#endif

#if RETRO_USE_MOD_LOADER
                currentObjectID = classID;
#endif

                if (objectClass->serialize)
                    objectClass->serialize();
            }

            for (int32 e = 1; e < varCount; ++e) {
                RETRO_HASH_MD5(varHash);
                varHash[0] = ReadInt32(&info, false);
                varHash[1] = ReadInt32(&info, false);
                varHash[2] = ReadInt32(&info, false);
                varHash[3] = ReadInt32(&info, false);

                int32 varID = 0;
                MEM_ZERO(varList[e]);
                for (int32 v = 0; v < editableVarCount; ++v) {
                    if (HASH_MATCH_MD5(varHash, editableVarList[v].hash)) {
                        varID = v;
                        HASH_COPY_MD5(varList[e].hash, editableVarList[v].hash);
                        varList[e].offset = editableVarList[v].offset;
                        varList[e].active = true;
                        break;
                    }
                }

                editableVarList[varID].type = varList[e].type = ReadInt8(&info);
            }

            uint16 entityCount = ReadInt16(&info);
            for (int32 e = 0; e < entityCount; ++e) {
                uint16 slotID = ReadInt16(&info);

                EntityBase *entity = NULL;

#if RETRO_REV02
                if (slotID < SCENEENTITY_COUNT)
                    entity = &objectEntityList[slotID + RESERVE_ENTITY_COUNT];
                else
                    entity = &tempEntityList[slotID - SCENEENTITY_COUNT];
#else
                entity = &objectEntityList[slotID + RESERVE_ENTITY_COUNT];
#endif

                entity->classID = classID;
#if RETRO_REV02
                entity->filter = 0xFF;
#endif
                entity->position.x = ReadInt32(&info, false);
                entity->position.y = ReadInt32(&info, false);

                uint8 *entityBuffer = (uint8 *)entity;

                uint8 tempBuffer[0x10];
                for (int32 v = 1; v < varCount; ++v) {
                    switch (varList[v].type) {
                        case VAR_UINT8:
                        case VAR_INT8:
                            if (varList[v].active)
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(int8));
                            else
                                ReadBytes(&info, tempBuffer, sizeof(int8));
                            break;

                        case VAR_UINT16:
                        case VAR_INT16:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(int16 *)&entityBuffer[varList[v].offset] = ReadInt16(&info);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(int16));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(int16));
                            break;

                        case VAR_UINT32:
                        case VAR_INT32:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(int32 *)&entityBuffer[varList[v].offset] = ReadInt32(&info, false);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(int32));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(int32));
                            break;

                        // not entirely sure on specifics here, should always be sizeof(int32) but it having a unique type implies it isn't always
                        case VAR_ENUM:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(int32 *)&entityBuffer[varList[v].offset] = ReadInt32(&info, false);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(int32));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(int32));
                            break;

                        case VAR_BOOL:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(bool32 *)&entityBuffer[varList[v].offset] = (bool32)ReadInt32(&info, false);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(bool32));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(bool32));
                            break;

                        case VAR_STRING:
                            if (varList[v].active) {
                                String *string = (String *)&entityBuffer[varList[v].offset];
                                uint16 len     = ReadInt16(&info);

                                InitString(string, "", len);
                                for (string->length = 0; string->length < len; ++string->length) string->chars[string->length] = ReadInt16(&info);
                            }
                            else {
                                Seek_Cur(&info, ReadInt16(&info) * sizeof(uint16));
                            }
                            break;

                        case VAR_VECTOR2:
                            if (varList[v].active) {
#if !RETRO_USE_ORIGINAL_CODE
                                *(int32 *)&entityBuffer[varList[v].offset]                 = ReadInt32(&info, false);
                                *(int32 *)&entityBuffer[varList[v].offset + sizeof(int32)] = ReadInt32(&info, false);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(int32));
                                ReadBytes(&info, &entityBuffer[varList[v].offset + sizeof(int32)], sizeof(int32));
#endif
                            }
                            else {
                                ReadBytes(&info, tempBuffer, sizeof(int32)); // x
                                ReadBytes(&info, tempBuffer, sizeof(int32)); // y
                            }
                            break;

                        // Never used in mania so we don't know for sure, but it's our best guess!
                        case VAR_FLOAT:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(float *)&entityBuffer[varList[v].offset] = ReadSingle(&info);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(float));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(float));
                            break;

                        case VAR_COLOR:
                            if (varList[v].active)
#if !RETRO_USE_ORIGINAL_CODE
                                *(color *)&entityBuffer[varList[v].offset] = ReadInt32(&info, false);
#else
                                // This only works as intended on little-endian CPUs.
                                ReadBytes(&info, &entityBuffer[varList[v].offset], sizeof(color));
#endif
                            else
                                ReadBytes(&info, tempBuffer, sizeof(color));
                            break;
                    }
                }
            }

#if !RETRO_USE_ORIGINAL_CODE
            RemoveStorageEntry((void **)&varList);
            varList = NULL;
#endif
        }

#if RETRO_REV02
        // handle filter and stuff
        EntityBase *entity = &objectEntityList[RESERVE_ENTITY_COUNT];
        int32 activeSlot   = RESERVE_ENTITY_COUNT;
        for (int32 i = RESERVE_ENTITY_COUNT; i < SCENEENTITY_COUNT + RESERVE_ENTITY_COUNT; ++i) {
            if (sceneInfo.filter & entity->filter) {
                if (i != activeSlot) {
                    memcpy(&objectEntityList[activeSlot], entity, sizeof(EntityBase));
                    memset(entity, 0, sizeof(EntityBase));
                }

                ++activeSlot;
            }
            else {
                memset(entity, 0, sizeof(EntityBase));
            }

            entity++;
        }

        for (int32 i = 0; i < SCENEENTITY_COUNT; ++i) {
            if (sceneInfo.filter & tempEntityList[i].filter)
                memcpy(&objectEntityList[activeSlot++], &tempEntityList[i], sizeof(EntityBase));

            // DCWIP
#if RETRO_PLATFORM == RETRO_KALLISTIOS
            if (activeSlot >= SCENEENTITY_COUNT + RESERVE_ENTITY_COUNT) {
                printf("\t[NG] SLOT OVERFLOW: activeSlot >= SCENEENTITY_COUNT + RESERVE_ENTITY_COUNT (%ld >= %d)\n",
                       activeSlot, SCENEENTITY_COUNT + RESERVE_ENTITY_COUNT);
                break;
            }
#else
            if (activeSlot >= SCENEENTITY_COUNT + RESERVE_ENTITY_COUNT)
                break;
#endif
        }

#if !RETRO_USE_ORIGINAL_CODE
        RemoveStorageEntry((void **)&tempEntityList);
#endif
        tempEntityList = NULL;
#endif

#if !RETRO_USE_ORIGINAL_CODE
        RemoveStorageEntry((void **)&editableVarList);
#endif
        editableVarList = NULL;

        CloseFile(&info);
    }
#if RETRO_USE_MOD_LOADER
    LoadGameXML(true); // override the stage palette *somewhere* idfk
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
    if (ufoNum)
        GenerateLODTexture();
#endif

}
void RSDK::LoadTileConfig(char *filepath)
{
    FileInfo info;
    InitFileInfo(&info);

    if (LoadFile(&info, filepath, FMODE_RB)) {
        uint32 sig = ReadInt32(&info, false);
        if (sig != RSDK_SIGNATURE_TIL) {
            CloseFile(&info);
            return;
        }

        uint8 *buffer = NULL;
        ReadCompressed(&info, &buffer);

        int32 bufPos = 0;
        for (int32 p = 0; p < CPATH_COUNT; ++p) {
            // No Flip/Stored in file
            for (int32 t = 0; t < TILE_COUNT; ++t) {
                uint8 maskHeights[0x10];
                uint8 maskActive[0x10];

                memcpy(maskHeights, buffer + bufPos, TILE_SIZE * sizeof(uint8));
                bufPos += TILE_SIZE;
                memcpy(maskActive, buffer + bufPos, TILE_SIZE * sizeof(uint8));
                bufPos += TILE_SIZE;

                bool32 yFlip              = buffer[bufPos++];
                tileInfo[p][t].floorAngle = buffer[bufPos++];
                tileInfo[p][t].lWallAngle = buffer[bufPos++];
                tileInfo[p][t].rWallAngle = buffer[bufPos++];
                tileInfo[p][t].roofAngle  = buffer[bufPos++];
                tileInfo[p][t].flag       = buffer[bufPos++];

                if (yFlip) {
                    for (int32 c = 0; c < TILE_SIZE; c++) {
                        if (maskActive[c]) {
                            collisionMasks[p][t].floorMasks[c] = 0x00;
                            collisionMasks[p][t].roofMasks[c]  = maskHeights[c];
                        }
                        else {
                            collisionMasks[p][t].floorMasks[c] = 0xFF;
                            collisionMasks[p][t].roofMasks[c]  = 0xFF;
                        }
                    }

                    // LWall rotations
                    for (int32 c = 0; c < TILE_SIZE; ++c) {
                        int32 h = 0;
                        while (true) {
                            if (h == TILE_SIZE) {
                                collisionMasks[p][t].lWallMasks[c] = 0xFF;
                                break;
                            }

                            uint8 m = collisionMasks[p][t].roofMasks[h];
                            if (m != 0xFF && c <= m) {
                                collisionMasks[p][t].lWallMasks[c] = h;
                                break;
                            }
                            else {
                                ++h;
                                if (h <= -1)
                                    break;
                            }
                        }
                    }

                    // RWall rotations
                    for (int32 c = 0; c < TILE_SIZE; ++c) {
                        int32 h = TILE_SIZE - 1;
                        while (true) {
                            if (h == -1) {
                                collisionMasks[p][t].rWallMasks[c] = 0xFF;
                                break;
                            }

                            uint8 m = collisionMasks[p][t].roofMasks[h];
                            if (m != 0xFF && c <= m) {
                                collisionMasks[p][t].rWallMasks[c] = h;
                                break;
                            }
                            else {
                                --h;
                                if (h >= TILE_SIZE)
                                    break;
                            }
                        }
                    }
                }
                else // Regular Tile
                {
                    // Collision heights
                    for (int32 c = 0; c < TILE_SIZE; ++c) {
                        if (maskActive[c]) {
                            collisionMasks[p][t].floorMasks[c] = maskHeights[c];
                            collisionMasks[p][t].roofMasks[c]  = 0x0F;
                        }
                        else {
                            collisionMasks[p][t].floorMasks[c] = 0xFF;
                            collisionMasks[p][t].roofMasks[c]  = 0xFF;
                        }
                    }

                    // LWall rotations
                    for (int32 c = 0; c < TILE_SIZE; ++c) {
                        int32 h = 0;
                        while (true) {
                            if (h == TILE_SIZE) {
                                collisionMasks[p][t].lWallMasks[c] = 0xFF;
                                break;
                            }

                            uint8 m = collisionMasks[p][t].floorMasks[h];
                            if (m != 0xFF && c >= m) {
                                collisionMasks[p][t].lWallMasks[c] = h;
                                break;
                            }
                            else {
                                ++h;
                                if (h <= -1)
                                    break;
                            }
                        }
                    }

                    // RWall rotations
                    for (int32 c = 0; c < TILE_SIZE; ++c) {
                        int32 h = TILE_SIZE - 1;
                        while (true) {
                            if (h == -1) {
                                collisionMasks[p][t].rWallMasks[c] = 0xFF;
                                break;
                            }

                            uint8 m = collisionMasks[p][t].floorMasks[h];
                            if (m != 0xFF && c >= m) {
                                collisionMasks[p][t].rWallMasks[c] = h;
                                break;
                            }
                            else {
                                --h;
                                if (h >= TILE_SIZE)
                                    break;
                            }
                        }
                    }
                }
            }

            // FlipX
            for (int32 t = 0; t < TILE_COUNT; ++t) {
                int32 off                       = (FLIP_X * TILE_COUNT);
                tileInfo[p][t + off].flag       = tileInfo[p][t].flag;
                tileInfo[p][t + off].floorAngle = -tileInfo[p][t].floorAngle;
                tileInfo[p][t + off].lWallAngle = -tileInfo[p][t].rWallAngle;
                tileInfo[p][t + off].roofAngle  = -tileInfo[p][t].roofAngle;
                tileInfo[p][t + off].rWallAngle = -tileInfo[p][t].lWallAngle;

                for (int32 c = 0; c < TILE_SIZE; ++c) {
                    int32 h = collisionMasks[p][t].lWallMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].rWallMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].rWallMasks[c] = 0xF - h;

                    h = collisionMasks[p][t].rWallMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].lWallMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].lWallMasks[c] = 0xF - h;

                    collisionMasks[p][t + off].floorMasks[c] = collisionMasks[p][t].floorMasks[0xF - c];
                    collisionMasks[p][t + off].roofMasks[c]  = collisionMasks[p][t].roofMasks[0xF - c];
                }
            }

            // FlipY
            for (int32 t = 0; t < TILE_COUNT; ++t) {
                int32 off                       = (FLIP_Y * TILE_COUNT);
                tileInfo[p][t + off].flag       = tileInfo[p][t].flag;
                tileInfo[p][t + off].floorAngle = -0x80 - tileInfo[p][t].roofAngle;
                tileInfo[p][t + off].lWallAngle = -0x80 - tileInfo[p][t].lWallAngle;
                tileInfo[p][t + off].roofAngle  = -0x80 - tileInfo[p][t].floorAngle;
                tileInfo[p][t + off].rWallAngle = -0x80 - tileInfo[p][t].rWallAngle;

                for (int32 c = 0; c < TILE_SIZE; ++c) {
                    int32 h = collisionMasks[p][t].roofMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].floorMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].floorMasks[c] = 0xF - h;

                    h = collisionMasks[p][t].floorMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].roofMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].roofMasks[c] = 0xF - h;

                    collisionMasks[p][t + off].lWallMasks[c] = collisionMasks[p][t].lWallMasks[0xF - c];
                    collisionMasks[p][t + off].rWallMasks[c] = collisionMasks[p][t].rWallMasks[0xF - c];
                }
            }

            // FlipXY
            for (int32 t = 0; t < TILE_COUNT; ++t) {
                int32 off                       = (FLIP_XY * TILE_COUNT);
                int32 offY                      = (FLIP_Y * TILE_COUNT);
                tileInfo[p][t + off].flag       = tileInfo[p][t + offY].flag;
                tileInfo[p][t + off].floorAngle = -tileInfo[p][t + offY].floorAngle;
                tileInfo[p][t + off].lWallAngle = -tileInfo[p][t + offY].rWallAngle;
                tileInfo[p][t + off].roofAngle  = -tileInfo[p][t + offY].roofAngle;
                tileInfo[p][t + off].rWallAngle = -tileInfo[p][t + offY].lWallAngle;

                for (int32 c = 0; c < TILE_SIZE; ++c) {
                    int32 h = collisionMasks[p][t + offY].lWallMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].rWallMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].rWallMasks[c] = 0xF - h;

                    h = collisionMasks[p][t + offY].rWallMasks[c];
                    if (h == 0xFF)
                        collisionMasks[p][t + off].lWallMasks[c] = 0xFF;
                    else
                        collisionMasks[p][t + off].lWallMasks[c] = 0xF - h;

                    collisionMasks[p][t + off].floorMasks[c] = collisionMasks[p][t + offY].floorMasks[0xF - c];
                    collisionMasks[p][t + off].roofMasks[c]  = collisionMasks[p][t + offY].roofMasks[0xF - c];
                }
            }
        }

#if !RETRO_USE_ORIGINAL_CODE
        RemoveStorageEntry((void **)&buffer);
        buffer = NULL;
#endif
        CloseFile(&info);
    }
}

void RSDK::LoadStageGIF(char *filepath)
{
    ImageGIF tileset;

    if (tileset.Load(filepath, true) && tileset.width == TILE_SIZE && tileset.height <= TILE_COUNT * TILE_SIZE) {
#if defined(KOS_HARDWARE_RENDERER)
        uint8_t *tilesetPixels = nullptr;
        // we need to make a copy of original tileset when loading UFO stages
        // to make LOD map texture
        if (ufoNum) {
            if (persistTiles == nullptr) {
                AllocateStorage(reinterpret_cast<void**>(&persistTiles), TILESET_SIZE, DATASET_STG, false);
                if (persistTiles == nullptr) {
                    printf("[NG] Failed to allocate for persistent tile set!!!: %s\n", filepath);
                } else {
                    PinStorage((void**)&persistTiles);
                }
            }
        }
        AllocateStorage(reinterpret_cast<void**>(&tilesetPixels), 2 * TILESET_SIZE, DATASET_TMP, true);
        PinStorage(reinterpret_cast<void**>(&tilesetPixels));

        if (tilesetPixels == nullptr) {
            printf("[NG] Failed to allocate for tile set!!!: %s\n", filepath);
            return;
        }
#endif
        tileset.pixels = tilesetPixels;
        tileset.Load(NULL, false);

#if RETRO_PLATFORM == RETRO_KALLISTIOS
        PaletteFlags::MarkDirtyNoCheck(0);
#endif

        for (int32 r = 0; r < 0x10; ++r) {
            // only overwrite inactive rows
            if (!(activeStageRows[0] >> r & 1) && !(activeGlobalRows[0] >> r & 1)) {
                for (int32 c = 0; c < 0x10; ++c) {
                    uint8 red                    = (tileset.palette[(r << 4) + c] >> 0x10);
                    uint8 green                  = (tileset.palette[(r << 4) + c] >> 0x08);
                    uint8 blue                   = (tileset.palette[(r << 4) + c] >> 0x00);
                    #if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
                    fullPalette[0][(r << 4) + c] = rgb32To16_B[blue] | rgb32To16_G[green] | rgb32To16_R[red];
                    #else
                    fullPalette[0][(r << 4) + c] = PACK_RGB888(red, green, blue);
                    #endif
                }
            }
        }

#if defined(KOS_HARDWARE_RENDERER)
        // use one of the additional tileset buffers to re-orient the tileset into a 512x512 texture
        // DCTODO: reduce size of tilesetPixels to just what we need
        // one surface per tile might be nice in the future - jn64
        uint8* const srcTiles = &tilesetPixels[0];
        uint8* const dstTiles = &tilesetPixels[TILESET_SIZE];
        constexpr size_t tilesPerRow = KOS_ATLAS_WIDTH_TILES;

        for (size_t i = 0; i < TILE_COUNT; ++i) {
            const size_t dstColumn = i % tilesPerRow;
            const size_t dstRow = i / tilesPerRow;

            uint8* srcPixels = &srcTiles[TILE_DATASIZE * i];
            uint8* dstPixels = &dstTiles[(dstColumn * TILE_SIZE) + (dstRow * tilesPerRow * TILE_DATASIZE)];

            for (size_t tileY = 0; tileY < TILE_SIZE; ++tileY) {
                memcpy(dstPixels, srcPixels, TILE_SIZE);
                srcPixels += TILE_SIZE;
                dstPixels += TILE_SIZE * tilesPerRow;
            }
        }
        // we need to make a copy of original tileset when loading UFO stages
        // to make LOD map texture
        if (ufoNum) {
            memcpy(persistTiles, &tilesetPixels[0], TILESET_SIZE);
        }

        auto* surface = &gfxSurface[0];

        if (surface->texture == nullptr) {
            pvr_ptr_t texture = pvr_mem_malloc(surface->width * surface->height);

            if (texture == nullptr) {
                printf("[pvr] [NG] WARNING: failed to allocate tileset texture!\n");
            } else {
                surface->texture = texture;
            }
        }

        if (surface->texture != nullptr) {
            surface->isVq = 0;
            // pvr_txr_load_ex is used instead of pvr_txr_load because _ex twiddles automatically,
            // which is useful since PVR palettized textures must be twiddled (apparently? see pvr.h)
            pvr_txr_load_ex(
                dstTiles,
                surface->texture,
                surface->width,
                surface->height,
                PVR_TXRLOAD_8BPP
            );
        }
#else
        // Flip X
        uint8 *srcPixels = tilesetPixels;
        uint8 *dstPixels = &tilesetPixels[(FLIP_X * TILESET_SIZE) + (TILE_SIZE - 1)];
        for (int32 t = 0; t < 0x400 * TILE_SIZE; ++t) {
            for (int32 r = 0; r < TILE_SIZE; ++r) {
                *dstPixels-- = *srcPixels++;
            }

            dstPixels += (TILE_SIZE * 2);
        }

        // Flip Y
        srcPixels = tilesetPixels;
        for (int32 t = 0; t < 0x400; ++t) {
            dstPixels = &tilesetPixels[(FLIP_Y * TILESET_SIZE) + (t * TILE_DATASIZE) + (TILE_DATASIZE - TILE_SIZE)];
            for (int32 y = 0; y < TILE_SIZE; ++y) {
                for (int32 x = 0; x < TILE_SIZE; ++x) {
                    *dstPixels++ = *srcPixels++;
                }

                dstPixels -= (TILE_SIZE * 2);
            }
        }

        // Flip XY
        srcPixels = &tilesetPixels[(FLIP_Y * TILESET_SIZE)];
        dstPixels = &tilesetPixels[(FLIP_XY * TILESET_SIZE) + (TILE_SIZE - 1)];
        for (int32 t = 0; t < 0x400 * TILE_SIZE; ++t) {
            for (int32 r = 0; r < TILE_SIZE; ++r) {
                *dstPixels-- = *srcPixels++;
            }

            dstPixels += (TILE_SIZE * 2);
        }
#endif

#if RETRO_USE_ORIGINAL_CODE
        tileset.palette = NULL;
        tileset.decoder = NULL;
#endif
        tileset.pixels  = NULL;
    }
}

void RSDK::ProcessParallaxAutoScroll()
{
    for (int32 l = 0; l < LAYER_COUNT; ++l) {
        TileLayer *layer = &tileLayers[l];

        if (layer->layout) {
            layer->scrollPos += layer->scrollSpeed;

            for (int32 s = 0; s < layer->scrollInfoCount; ++s) layer->scrollInfo[s].scrollPos += layer->scrollInfo[s].scrollSpeed;
        }
    }
}
void RSDK::ProcessParallax(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

    int32 pixelWidth       = TILE_SIZE * layer->xsize;
    int32 pixelHeight      = TILE_SIZE * layer->ysize;
    ScanlineInfo *scanline = scanlines;
    ScrollInfo *scrollInfo = layer->scrollInfo;

    switch (layer->type) {
        default: break;

        case LAYER_HSCROLL: {
            for (int32 i = 0; i < layer->scrollInfoCount; ++i) {
                scrollInfo->tilePos = scrollInfo->scrollPos + (currentScreen->position.x * scrollInfo->parallaxFactor << 8);

                int16 tilePos = FROM_FIXED(scrollInfo->tilePos) % pixelWidth;
                if (tilePos < 0)
                    tilePos += pixelWidth;
                scrollInfo->tilePos = TO_FIXED(tilePos);

                ++scrollInfo;
            }

            int16 scrollPos =
                FROM_FIXED((int32)((layer->scrollPos + (layer->parallaxFactor * currentScreen->position.y << 8)) & 0xFFFF0000)) % pixelHeight;
            if (scrollPos < 0)
                scrollPos += pixelHeight;

            uint8 *lineScrollPtr = &layer->lineScroll[scrollPos];

            // Above water
            int32 *deformationData = &layer->deformationData[(scrollPos + (uint16)layer->deformationOffset) & 0x1FF];
            for (int32 i = 0; i < currentScreen->waterDrawPos; ++i) {
                scanline->position.x = layer->scrollInfo[*lineScrollPtr].tilePos;
#if RETRO_PLATFORM != RETRO_KALLISTIOS || !defined(KOS_HARDWARE_RENDERER)
                if (layer->scrollInfo[*lineScrollPtr].deform)
                    scanline->position.x += TO_FIXED(*deformationData);
#else
                // on KOS, just hold onto the deform info so we can manually apply it on draw
                if (layer->scrollInfo[*lineScrollPtr].deform) {
                    scanline->deform.x = TO_FIXED(*deformationData);
                } else {
                    scanline->deform.x = 0;
                }
#endif

                scanline->position.y = TO_FIXED(scrollPos++);

                deformationData++;
                if (scrollPos == pixelHeight) {
                    lineScrollPtr = layer->lineScroll;
                    scrollPos     = 0;
                }
                else {
                    ++lineScrollPtr;
                }
                scanline++;
            }

            // Under water
            deformationData = &layer->deformationDataW[(scrollPos + (uint16)layer->deformationOffsetW) & 0x1FF];
            for (int32 i = currentScreen->waterDrawPos; i < currentScreen->size.y; ++i) {
                scanline->position.x = layer->scrollInfo[*lineScrollPtr].tilePos;
#if RETRO_PLATFORM != RETRO_KALLISTIOS || !defined(KOS_HARDWARE_RENDERER)
                if (layer->scrollInfo[*lineScrollPtr].deform)
                    scanline->position.x += TO_FIXED(*deformationData);
#else
                // on KOS, just hold onto the deform info so we can manually apply it on draw
                if (layer->scrollInfo[*lineScrollPtr].deform) {
                    scanline->deform.x = TO_FIXED(*deformationData);
                } else {
                    scanline->deform.x = 0;
                }
#endif

                scanline->position.y = TO_FIXED(scrollPos++);

                deformationData++;
                if (scrollPos == pixelHeight) {
                    lineScrollPtr = layer->lineScroll;
                    scrollPos     = 0;
                }
                else {
                    ++lineScrollPtr;
                }
                scanline++;
            }
            break;
        }

        case LAYER_VSCROLL: {
            for (int32 i = 0; i < layer->scrollInfoCount; ++i) {
                scrollInfo->tilePos = scrollInfo->scrollPos + (currentScreen->position.y * scrollInfo->parallaxFactor << 8);
                scrollInfo->tilePos = TO_FIXED(FROM_FIXED(scrollInfo->tilePos) % pixelHeight);

                ++scrollInfo;
            }

            int16 scrollPos =
                FROM_FIXED((int32)((layer->scrollPos + (layer->parallaxFactor * currentScreen->position.x << 8)) & 0xFFFF0000)) % pixelWidth;
            if (scrollPos < 0)
                scrollPos += pixelWidth;

            uint8 *lineScrollPtr = &layer->lineScroll[scrollPos];

            // Above water
            for (int32 i = 0; i < currentScreen->size.x; ++i) {
                scanline->position.x = TO_FIXED(scrollPos++);
                scanline->position.y = layer->scrollInfo[*lineScrollPtr].tilePos;

                if (scrollPos == pixelWidth) {
                    lineScrollPtr = layer->lineScroll;
                    scrollPos     = 0;
                }
                else {
                    ++lineScrollPtr;
                }

                scanline++;
            }
            break;
        }

        case LAYER_ROTOZOOM: {
            int16 scrollPosX =
                FROM_FIXED((int32)((layer->scrollPos + (layer->parallaxFactor * currentScreen->position.x << 8)) & 0xFFFF0000)) % pixelWidth;
            if (scrollPosX < 0)
                scrollPosX += pixelWidth;

            int16 scrollPosY =
                FROM_FIXED((int32)((layer->scrollPos + (layer->parallaxFactor * currentScreen->position.y << 8)) & 0xFFFF0000)) % pixelHeight;
            if (scrollPosY < 0)
                scrollPosY += pixelHeight;

            for (int32 i = 0; i < currentScreen->size.y; ++i) {
                scanline->position.x = TO_FIXED(scrollPosX);
                scanline->position.y = TO_FIXED(scrollPosY++);
                scanline->deform.x   = TO_FIXED(1);
                scanline->deform.y   = TO_FIXED(0);

                scanline++;
            }
            break;
        }

        case LAYER_BASIC: {
            for (int32 i = 0; i < layer->scrollInfoCount; ++i) {
                scrollInfo->tilePos = scrollInfo->scrollPos + (currentScreen->position.x * scrollInfo->parallaxFactor << 8);

                int16 tilePos = FROM_FIXED(scrollInfo->tilePos) % pixelWidth;
                if (tilePos < 0)
                    tilePos += pixelWidth;
                scrollInfo->tilePos = TO_FIXED(tilePos);

                ++scrollInfo;
            }

            int16 scrollPos =
                FROM_FIXED((int32)((layer->scrollPos + (layer->parallaxFactor * currentScreen->position.y << 8)) & 0xFFFF0000)) % pixelHeight;
            if (scrollPos < 0)
                scrollPos += pixelHeight;

            uint8 *lineScrollPtr = &layer->lineScroll[scrollPos];

            // Above water
            for (int32 i = 0; i < currentScreen->waterDrawPos; ++i) {
                scanline->position.x = layer->scrollInfo[*lineScrollPtr].tilePos;
                scanline->position.y = TO_FIXED(scrollPos++);

                if (scrollPos == pixelHeight) {
                    lineScrollPtr = layer->lineScroll;
                    scrollPos     = 0;
                }
                else {
                    ++lineScrollPtr;
                }
                scanline++;
            }

            // Under water
            for (int32 i = currentScreen->waterDrawPos; i < currentScreen->size.y; ++i) {
                scanline->position.x = layer->scrollInfo[*lineScrollPtr].tilePos;
                scanline->position.y = TO_FIXED(scrollPos++);

                if (scrollPos == pixelHeight) {
                    lineScrollPtr = layer->lineScroll;
                    scrollPos     = 0;
                }
                else {
                    ++lineScrollPtr;
                }

                scanline++;
            }
            break;
        }
    }
}

void RSDK::ProcessSceneTimer()
{
    if (sceneInfo.timeEnabled) {
        sceneInfo.timeCounter += 100;

        if (sceneInfo.timeCounter >= 6000) {
            sceneInfo.timeCounter -= 6025;

            if (++sceneInfo.seconds >= 60) {
                sceneInfo.seconds = 0;

                if (++sceneInfo.minutes >= 60)
                    sceneInfo.minutes = 0;
            }
        }

        sceneInfo.milliseconds = sceneInfo.timeCounter / 60; // 60 == refreshRate
    }
}

void RSDK::SetScene(const char *categoryName, const char *sceneName)
{
    RETRO_HASH_MD5(catHash);
    GEN_HASH_MD5(categoryName, catHash);

    RETRO_HASH_MD5(scnHash);
    GEN_HASH_MD5(sceneName, scnHash);

    for (int32 i = 0; i < sceneInfo.categoryCount; ++i) {
        if (HASH_MATCH_MD5(sceneInfo.listCategory[i].hash, catHash)) {
            sceneInfo.activeCategory = i;
            sceneInfo.listPos        = sceneInfo.listCategory[i].sceneOffsetStart;

            for (int32 s = 0; s < sceneInfo.listCategory[i].sceneCount; ++s) {
                if (HASH_MATCH_MD5(sceneInfo.listData[sceneInfo.listCategory[i].sceneOffsetStart + s].hash, scnHash)) {
                    sceneInfo.listPos = sceneInfo.listCategory[i].sceneOffsetStart + s;
                    break;
                }
            }

            break;
        }
    }
}

void RSDK::CopyTileLayer(uint16 dstLayerID, int32 dstStartX, int32 dstStartY, uint16 srcLayerID, int32 srcStartX, int32 srcStartY, int32 countX,
                         int32 countY)
{
    if (dstLayerID < LAYER_COUNT && srcLayerID < LAYER_COUNT) {
        TileLayer *dstLayer = &tileLayers[dstLayerID];
        TileLayer *srcLayer = &tileLayers[srcLayerID];

        if (dstStartX >= 0 && dstStartX < dstLayer->xsize && dstStartY >= 0 && dstStartY < dstLayer->ysize) {
            if (srcStartX >= 0 && srcStartX < srcLayer->xsize && srcStartY >= 0 && srcStartY < srcLayer->ysize) {
                if (dstStartX + countX > dstLayer->xsize)
                    countX = dstLayer->xsize - dstStartX;

                if (dstStartY + countY > dstLayer->ysize)
                    countY = dstLayer->ysize - dstStartY;

                if (srcStartX + countX > srcLayer->xsize)
                    countX = srcLayer->xsize - srcStartX;

                if (srcStartY + countY > srcLayer->ysize)
                    countY = srcLayer->ysize - srcStartY;

                for (int32 y = 0; y < countY; ++y) {
                    for (int32 x = 0; x < countX; ++x) {
                        uint16 tile = srcLayer->layout[(x + srcStartX) + ((y + srcStartY) << srcLayer->widthShift)];
                        dstLayer->layout[(x + dstStartX) + ((y + dstStartY) << dstLayer->widthShift)] = tile;
                    }
                }
            }
        }
    }
}

#ifdef KOS_HARDWARE_RENDERER
void DrawByLayout(uint16 layout, int32 screenX, int32 screenY) {
    layout &= 0xFFF;
    const int32 flip = layout / TILE_COUNT;
    layout %= TILE_COUNT;

    bool usePoly;
    const GFXSurface* surface;
    const AniTileState* aniTileState = AniTileTracker::GetAniTile(layout);

    int32 sprX0;
    int32 sprX1;
    int32 sprY0;
    int32 sprY1;

    if (aniTileState != nullptr && aniTileState->sheetID != 0) {
        usePoly = !aniTileState->IsTileAligned();
        surface = &gfxSurface[aniTileState->sheetID];

        sprX0 = aniTileState->u;
        sprX1 = aniTileState->u;
        sprY0 = aniTileState->v;
        sprY1 = aniTileState->v;
    }
    else {
        usePoly = false;
        surface = &gfxSurface[0];

        const int32 tilesetX = TILE_SIZE * (static_cast<int32>(layout) % KOS_ATLAS_WIDTH_TILES);
        const int32 tilesetY = TILE_SIZE * (static_cast<int32>(layout) / KOS_ATLAS_WIDTH_TILES);

        sprX0 = tilesetX;
        sprX1 = tilesetX;
        sprY0 = tilesetY;
        sprY1 = tilesetY;
    }

    if (flip & FLIP_X) {
        sprX0 += TILE_SIZE;
    }
    else {
        sprX1 += TILE_SIZE;
    }

    if (flip & FLIP_Y) {
        sprY0 += TILE_SIZE;
    }
    else {
        sprY1 += TILE_SIZE;
    }

    const int32 prepY = std::max<int32>(0, screenY);

    if (usePoly) {
        RenderDevice::PrepareTexturedPolyPT(prepY, INK_NONE, surface);

        RenderDevice::DrawTexturedPolyPT(
            screenX, screenY,
            screenX, screenY,
            TILE_SIZE, TILE_SIZE,
            sprX0, sprX1,
            sprY0, sprY1,
            0,
            255,
            surface
        );
    }
    else {
        RenderDevice::PrepareTexturedQuadPT(prepY, surface);

        RenderDevice::DrawTexturedQuadPT(
            screenX, screenY,
            TILE_SIZE, TILE_SIZE,
            sprX0, sprX1,
            sprY0, sprY1,
            &gfxSurface[0]
        );
    }
}

void DrawByLayoutEx(uint16 layout,
                    const Vector2& upperLeft, const Vector2& upperRight,
                    const Vector2& lowerLeft, const Vector2& lowerRight) {
    layout &= 0xFFF;
    const int32 flip = layout / TILE_COUNT;
    layout %= TILE_COUNT;

    bool usePoly;
    const GFXSurface* surface;
    const AniTileState* aniTileState = AniTileTracker::GetAniTile(layout);

    int32 sprX0;
    int32 sprX1;
    int32 sprY0;
    int32 sprY1;

    if (aniTileState != nullptr && aniTileState->sheetID != 0) {
        usePoly = !aniTileState->IsTileAligned();
        surface = &gfxSurface[aniTileState->sheetID];

        sprX0 = aniTileState->u;
        sprX1 = aniTileState->u;
        sprY0 = aniTileState->v;
        sprY1 = aniTileState->v;
    }
    else {
        usePoly = false;
        surface = &gfxSurface[0];

        const int32 tilesetX = TILE_SIZE * (static_cast<int32>(layout) % KOS_ATLAS_WIDTH_TILES);
        const int32 tilesetY = TILE_SIZE * (static_cast<int32>(layout) / KOS_ATLAS_WIDTH_TILES);

        sprX0 = tilesetX;
        sprX1 = tilesetX;
        sprY0 = tilesetY;
        sprY1 = tilesetY;
    }

    if (flip & FLIP_X) {
        sprX0 += TILE_SIZE;
    }
    else {
        sprX1 += TILE_SIZE;
    }

    if (flip & FLIP_Y) {
        sprY0 += TILE_SIZE;
    }
    else {
        sprY1 += TILE_SIZE;
    }

    const int32 prepY = std::max<int32>(0, std::min(upperLeft.y, upperRight.y));

    if (usePoly) {
        RenderDevice::PrepareTexturedPolyPT(prepY, INK_NONE, surface);

        RenderDevice::DrawTexturedPolyPTEx(
            upperLeft, upperRight,
            lowerLeft, lowerRight,
            sprX0, sprX1,
            sprY0, sprY1,
            surface);
    }
    else {
        RenderDevice::PrepareTexturedQuadPT(prepY, surface);

        RenderDevice::DrawTexturedQuadPTEx(
            upperLeft, upperRight,
            lowerLeft, lowerRight,
            sprX0, sprX1,
            sprY0, sprY1,
            surface
        );
    }
}

int32 GetLayerWrappedFromFixedX(const TileLayer* layer, int32 fixed_x) {
    const int32 tileX = FROM_FIXED(fixed_x);

    if (tileX >= TILE_SIZE * layer->xsize) {
        fixed_x = TO_FIXED(tileX - TILE_SIZE * layer->xsize);
    }
    else if (tileX < 0) {
        fixed_x = TO_FIXED(tileX + TILE_SIZE * layer->xsize);
    }

    return FROM_FIXED(fixed_x);
}
#endif

void RSDK::DrawLayerHScroll(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

#if defined(KOS_HARDWARE_RENDERER)
    const ScanlineInfo* upperScanline = &scanlines[currentScreen->clipBound_Y1];

    const int32 sheetY = FROM_FIXED(upperScanline->position.y) & 0xF;
    int32 scanlineIncrement = TILE_SIZE - sheetY;

    const int32 screenTileAligned = AlignUp(currentScreen->size.x, TILE_SIZE) / TILE_SIZE;

    for (int32 cy = currentScreen->clipBound_Y1 - sheetY; cy < currentScreen->clipBound_Y2; cy += TILE_SIZE) {
        const int32 yRemainder = currentScreen->clipBound_Y2 - cy;
        const int32 fromFixedUpperY = FROM_FIXED(upperScanline->position.y);
        const ScanlineInfo* lowerScanline = upperScanline + std::min<int32>(scanlineIncrement - 1, yRemainder - 1);

        int32 tilesToDraw = screenTileAligned;
        int32 upperScanlineFixedX = upperScanline->position.x;

        int32 fromFixedUpperX = GetLayerWrappedFromFixedX(layer, upperScanlineFixedX + upperScanline->deform.x) % currentScreen->pitch;

        const int32 offsetDelta = FROM_FIXED(upperScanline->deform.x - lowerScanline->deform.x);
        int32 upperScreenOffsetX = -(fromFixedUpperX & 0xF);
        int32 lowerScreenOffsetX = upperScreenOffsetX + offsetDelta;

        int32 extraTilesToDraw = abs(AlignUp(std::max(upperScreenOffsetX, lowerScreenOffsetX), TILE_SIZE));

        upperScanlineFixedX -= TO_FIXED(extraTilesToDraw);
        upperScreenOffsetX -= extraTilesToDraw;
        lowerScreenOffsetX -= extraTilesToDraw;

        extraTilesToDraw += abs(AlignDown(std::min(upperScreenOffsetX, lowerScreenOffsetX), TILE_SIZE));

        tilesToDraw += extraTilesToDraw / TILE_SIZE;

        fromFixedUpperX = GetLayerWrappedFromFixedX(layer, upperScanlineFixedX + upperScanline->deform.x);

        const int32 screenUpperY = cy;
        const int32 screenLowerY = cy + TILE_SIZE;

        const int32 ty = fromFixedUpperY >> 4;
        int32 tx = fromFixedUpperX >> 4;

        for (int32 t = 0; t < tilesToDraw; ++t) {
            const int32 cx = TILE_SIZE * t;
            const int32 screenUpperX = cx + upperScreenOffsetX;
            const int32 screenLowerX = cx + lowerScreenOffsetX;

            const uint16 layout = layer->layout[tx + (ty << layer->widthShift)];
            tx = (tx + 1) % layer->xsize;

            if (layout == 0xFFFF) {
                continue;
            }

            const Vector2 screenUpperLeft {
                screenUpperX,
                screenUpperY,
            };

            const Vector2 screenUpperRight {
                screenUpperX + TILE_SIZE,
                screenUpperY,
            };

            const Vector2 screenLowerLeft {
                screenLowerX,
                screenLowerY,
            };

            const Vector2 screenLowerRight {
                screenLowerX + TILE_SIZE,
                screenLowerY,
            };

            DrawByLayoutEx(layout,
                           screenUpperLeft, screenUpperRight,
                           screenLowerLeft, screenLowerRight);
        }

        upperScanline += scanlineIncrement;
        scanlineIncrement = TILE_SIZE;
    }
#else
    int32 lineTileCount    = (currentScreen->pitch >> 4) - 1;
    uint8 *lineBuffer      = &gfxLineBuffer[currentScreen->clipBound_Y1];
    ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_Y1];
    uint16 *frameBuffer    = &currentScreen->frameBuffer[currentScreen->pitch * currentScreen->clipBound_Y1];

    for (int32 cy = currentScreen->clipBound_Y1; cy < currentScreen->clipBound_Y2; ++cy) {
        int32 x               = scanline->position.x;
        int32 y               = scanline->position.y;
        int32 tileX           = FROM_FIXED(x);
        uint16 *activePalette = fullPalette[*lineBuffer++];

        if (tileX >= TILE_SIZE * layer->xsize)
            x = TO_FIXED(tileX - TILE_SIZE * layer->xsize);
        else if (tileX < 0)
            x = TO_FIXED(tileX + TILE_SIZE * layer->xsize);

        int32 tileRemain = TILE_SIZE - (FROM_FIXED(x) & 0xF);
        int32 sheetX     = FROM_FIXED(x) & 0xF;
        int32 sheetY     = TILE_SIZE * (FROM_FIXED(y) & 0xF);
        int32 lineRemain = currentScreen->pitch;

        int32 tx       = x >> 20;
        uint16 *layout = &layer->layout[tx + ((y >> 20) << layer->widthShift)];
        lineRemain -= tileRemain;

        if (*layout >= 0xFFFF) {
            frameBuffer += tileRemain;
        }
        else {
            uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetY + sheetX];
            for (int32 i = 0; i < tileRemain; ++i) {
                if (*pixels)
                    *frameBuffer = activePalette[*pixels];
                ++pixels;
                ++frameBuffer;
            }
        }

        for (int32 l = 0; l < lineTileCount; ++l) {
            ++layout;

            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            if (*layout < 0xFFFF) {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetY];

                uint8 index = *pixels;
                if (index)
                    *frameBuffer = activePalette[index];

                index = pixels[1];
                if (index)
                    frameBuffer[1] = activePalette[index];

                index = pixels[2];
                if (index)
                    frameBuffer[2] = activePalette[index];

                index = pixels[3];
                if (index)
                    frameBuffer[3] = activePalette[index];

                index = pixels[4];
                if (index)
                    frameBuffer[4] = activePalette[index];

                index = pixels[5];
                if (index)
                    frameBuffer[5] = activePalette[index];

                index = pixels[6];
                if (index)
                    frameBuffer[6] = activePalette[index];

                index = pixels[7];
                if (index)
                    frameBuffer[7] = activePalette[index];

                index = pixels[8];
                if (index)
                    frameBuffer[8] = activePalette[index];

                index = pixels[9];
                if (index)
                    frameBuffer[9] = activePalette[index];

                index = pixels[10];
                if (index)
                    frameBuffer[10] = activePalette[index];

                index = pixels[11];
                if (index)
                    frameBuffer[11] = activePalette[index];

                index = pixels[12];
                if (index)
                    frameBuffer[12] = activePalette[index];

                index = pixels[13];
                if (index)
                    frameBuffer[13] = activePalette[index];

                index = pixels[14];
                if (index)
                    frameBuffer[14] = activePalette[index];

                index = pixels[15];
                if (index)
                    frameBuffer[15] = activePalette[index];
            }

            frameBuffer += TILE_SIZE;
            lineRemain -= TILE_SIZE;
        }

        while (lineRemain > 0) {
            ++layout;

            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            tileRemain = lineRemain >= TILE_SIZE ? TILE_SIZE : lineRemain;

            if (*layout >= 0xFFFF) {
                frameBuffer += tileRemain;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetY];
                for (int32 i = 0; i < tileRemain; ++i) {
                    if (*pixels)
                        *frameBuffer = activePalette[*pixels];
                    ++pixels;
                    ++frameBuffer;
                }
            }

            lineRemain -= TILE_SIZE;
        }

        ++scanline;
    }
#endif
}
void RSDK::DrawLayerVScroll(TileLayer *layer)
{
    // DCTODO: DrawLayerVScroll
    // (using early return so I can still statically analyze stuff!)
    return;
#if !defined(KOS_HARDWARE_RENDERER)
    if (!layer->xsize || !layer->ysize)
        return;

    int32 lineTileCount    = (currentScreen->size.y >> 4) - 1;
    uint16 *frameBuffer    = &currentScreen->frameBuffer[currentScreen->clipBound_X1];
    ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_X1];
    uint16 *activePalette  = fullPalette[gfxLineBuffer[0]];

    for (int32 cx = currentScreen->clipBound_X1; cx < currentScreen->clipBound_X2; ++cx) {
        int32 x  = scanline->position.x;
        int32 y  = scanline->position.y;
        int32 ty = FROM_FIXED(y);

        if (ty >= TILE_SIZE * layer->ysize)
            y -= TO_FIXED(TILE_SIZE * layer->ysize);
        else if (ty < 0)
            y += TO_FIXED(TILE_SIZE * layer->ysize);

        int32 tileRemain = TILE_SIZE - (FROM_FIXED(y) & 0xF);
        int32 sheetX     = FROM_FIXED(x) & 0xF;
        int32 sheetY     = FROM_FIXED(y) & 0xF;
        int32 lineRemain = currentScreen->size.y;

        uint16 *layout = &layer->layout[(x >> 20) + ((y >> 20) << layer->widthShift)];
        lineRemain -= tileRemain;

        if (*layout >= 0xFFFF) {
            frameBuffer += currentScreen->pitch * tileRemain;
        }
        else {
            uint8 *pixels = &tilesetPixels[TILE_SIZE * (sheetY + TILE_SIZE * (*layout & 0xFFF)) + sheetX];
            for (int32 y = 0; y < tileRemain; ++y) {
                if (*pixels)
                    *frameBuffer = activePalette[*pixels];
                pixels += TILE_SIZE;
                frameBuffer += currentScreen->pitch;
            }
        }

        ty = y >> 20;
        for (int32 l = 0; l < lineTileCount; ++l) {
            layout += layer->xsize;

            if (++ty == layer->ysize) {
                ty = 0;
                layout -= layer->ysize << layer->widthShift;
            }

            if (*layout >= 0xFFFF) {
                frameBuffer += TILE_SIZE * currentScreen->pitch;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetX];

                if (*pixels)
                    *frameBuffer = activePalette[*pixels];

                if (pixels[0x10])
                    frameBuffer[currentScreen->pitch * 1] = activePalette[pixels[0x10]];

                if (pixels[0x20])
                    frameBuffer[currentScreen->pitch * 2] = activePalette[pixels[0x20]];

                if (pixels[0x30])
                    frameBuffer[currentScreen->pitch * 3] = activePalette[pixels[0x30]];

                if (pixels[0x40])
                    frameBuffer[currentScreen->pitch * 4] = activePalette[pixels[0x40]];

                if (pixels[0x50])
                    frameBuffer[currentScreen->pitch * 5] = activePalette[pixels[0x50]];

                if (pixels[0x60])
                    frameBuffer[currentScreen->pitch * 6] = activePalette[pixels[0x60]];

                if (pixels[0x70])
                    frameBuffer[currentScreen->pitch * 7] = activePalette[pixels[0x70]];

                if (pixels[0x80])
                    frameBuffer[currentScreen->pitch * 8] = activePalette[pixels[0x80]];

                if (pixels[0x90])
                    frameBuffer[currentScreen->pitch * 9] = activePalette[pixels[0x90]];

                if (pixels[0xA0])
                    frameBuffer[currentScreen->pitch * 10] = activePalette[pixels[0xA0]];

                if (pixels[0xB0])
                    frameBuffer[currentScreen->pitch * 11] = activePalette[pixels[0xB0]];

                if (pixels[0xC0])
                    frameBuffer[currentScreen->pitch * 12] = activePalette[pixels[0xC0]];

                if (pixels[0xD0])
                    frameBuffer[currentScreen->pitch * 13] = activePalette[pixels[0xD0]];

                if (pixels[0xE0])
                    frameBuffer[currentScreen->pitch * 14] = activePalette[pixels[0xE0]];

                if (pixels[0xF0])
                    frameBuffer[currentScreen->pitch * 15] = activePalette[pixels[0xF0]];

                frameBuffer += currentScreen->pitch * TILE_SIZE;
            }

            lineRemain -= TILE_SIZE;
        }

        while (lineRemain > 0) {
            layout += layer->xsize;

            if (++ty == layer->ysize) {
                ty = 0;
                layout -= layer->ysize << layer->widthShift;
            }

            tileRemain = lineRemain >= TILE_SIZE ? TILE_SIZE : lineRemain;
            if (*layout >= 0xFFFF) {
                frameBuffer += currentScreen->pitch * sheetY;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetX];
                for (int32 y = 0; y < tileRemain; ++y) {
                    if (*pixels)
                        *frameBuffer = activePalette[*pixels];

                    pixels += TILE_SIZE;
                    frameBuffer += currentScreen->pitch;
                }
            }

            lineRemain -= TILE_SIZE;
        }

        frameBuffer -= currentScreen->pitch * currentScreen->size.y;

        ++scanline;
        ++frameBuffer;
    }
#endif
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS
// epsilon for small/negative value rejections
#define EPS 1e-3f
// 1/200, for ray intersect projection scaling
#define recip200 0.005f
// 1/16, for anything tile-dimensions related
#define recip16 0.0625f
// 1/65536, fixed to float
#define recip64k 0.00001526f
// 1/1024, to convert the fixed point sin/cos to float
#define recip1k 0.00097656f

void rayIntersect(Vector4f cam, Vector4f forward, Vector4f right, Vector4f up, int screenX, int screenY, float *hitX, float *hitZ) {
    float px = (float)screenX - ((float)DEFAULT_PIXWIDTH*0.5f);
    float py = 120.0f - (float)screenY;
    float nx = px * recip200;
    float ny = py * recip200;

    Vector4f dir;
    dir.x = forward.x + right.x * nx + up.x * ny;
    dir.y = forward.y + right.y * nx + up.y * ny;
    dir.z = forward.z + right.z * nx + up.z * ny;

    // intersect with ground plane (y == 0)
    // prevent div by zero
    if (dir.y == 0) {
        dir.y = 1;
    }
    float t = -cam.y / dir.y;
    *hitX = (cam.x + dir.x * t);
    *hitZ = (cam.z + dir.z * t);
}

// four corners of a projected tile
static Vector4f __attribute__((aligned(32))) tileVerts[4];

// indices into tileVerts
// upper/lower left/right
#define TILE_UL 0
#define TILE_LL 1
#define TILE_UR 2
#define TILE_LR 3
#endif

void RSDK::DrawLayerRotozoom(TileLayer *layer)
{
#if defined(KOS_HARDWARE_RENDERER)
    const GFXSurface* surface = &gfxSurface[0];
    bool pinball = false;

    int32 width    = (TILE_SIZE << layer->widthShift) - 1;
    int32 height   = (TILE_SIZE << layer->heightShift) - 1;

    uint16 *layout         = layer->layout;
    // hijacked scanlineinfo to store camera pos and orientation info
    ScanlineInfo *scanline = &scanlines[0];

    if ((uint32)scanline->deform.x != (uint32)0xFEDCBA09) return;
    if ((uint32)scanline->deform.y == (uint32)0xCDEF0123)
        pinball = true;

    // NOTE: tileset PrepareTexturedPolyPTEX moved to just before tile loop
    // to avoid "not consumed" RenderDevice warning when LOD quad prepares a different texture first

    scanline++;

    // 0.10 fixed-point yaw and pitch sin/cos pairs from UFO_Setup.c, PBL_Setup.c
    int32 scsin = scanline->deform.x;
    int32 sccos = scanline->deform.y;
    int32 scsinX = scanline->position.x;
    int32 sccosX = scanline->position.y;

    scanline++;

    // 16.16 fixed-point camera position from UFO_Setup.c, PBL_Setup.c
    int32 camX = scanline->deform.x;
    int32 camY = scanline->deform.y;
    int32 camZ = scanline->position.x;

    // convert all of the camera stuff to float
    Vector4f cam;
    cam.x = (float)camX * recip64k;
    cam.y = (float)camY * recip64k;
    cam.z = (float)camZ * recip64k;

    float sy = (float)scsin * recip1k;
    float cy = (float)sccos * recip1k;
    float sp = (float)scsinX * recip1k;
    float cp = (float)sccosX * recip1k;

    // build the camera basis
    Vector4f forward;
    Vector4f right;
    Vector4f up;

    forward.x = sy * cp;
    forward.y = -sp;
    forward.z = cy * cp;

    right.x = cy;
    right.y = 0.0f;
    right.z = -sy;

    // derived, orthonormal up vector
    up.x = sy * sp;
    up.y = cp;
    up.z = cy * sp;

    // normalize camera basis for next computations
    vec3f_normalize(forward.x, forward.y, forward.z);
    vec3f_normalize(right.x, right.y, right.z);
    vec3f_normalize(up.x, up.y, up.z);

    // dot the basis vectors with camera position
    // this is to build the translation row
    float drc = fipr(right.x,   right.y,   right.z,   0, cam.x, cam.y, cam.z, 0);
    float duc = fipr(up.x,      up.y,      up.z,      0, cam.x, cam.y, cam.z, 0);
    float dfc = fipr(forward.x, forward.y, forward.z, 0, cam.x, cam.y, cam.z, 0);

    // leaving this here to show how f and foa were derived
    const float aspect = (float)DEFAULT_PIXWIDTH / 240.0f;
    const float fovy   = 47.5f * RSDK_PI / 180.0f;
    const float f      = 1.0f / tanf(fovy * 0.5f);
    const float foa    = f / aspect;

    // projection matrix
    const float __attribute__((aligned(32))) viewproj[4][4] = {
        {   -right.x * foa,     up.x * f,    forward.x,  forward.x },
        {   -right.y * foa,     up.y * f,    forward.y,  forward.y },
        {   -right.z * foa,     up.z * f,    forward.z,  forward.z },
        {      drc   * foa,   -duc   * f,       -dfc  ,     -dfc   },
    };

    // ndc to screenspace matrix
    const float __attribute__((aligned(32))) screenspace[4][4] = {
        {  ((float)DEFAULT_PIXWIDTH*0.5f),    0.0f, 0.0f, 0.0f },
        {                            0.0f, -120.0f, 0.0f, 0.0f, },
        {                            0.0f,    0.0f, 0.5f, 0.0f, },
        {  ((float)DEFAULT_PIXWIDTH*0.5f),  120.0f, 0.5f, 1.0f, },
    };

    // store result of load/apply to use for early-out -w test
    float __attribute__((aligned(32))) finalmat[4][4];

    // put combined matrices into XMTRX
    mat_load(&screenspace);
    mat_apply(&viewproj);
    mat_store(&finalmat);

    // used to push out the bounding box to deal with some close-up disappearing sections
    size_t dimensionAdjust = 34;

    // four corner tiles of the screen clip bounds and origin tile
    float hits[5][2];
    rayIntersect(cam, forward, right, up, currentScreen->clipBound_X1, currentScreen->clipBound_Y1, &hits[0][0], &hits[0][1]);
    rayIntersect(cam, forward, right, up, currentScreen->clipBound_X2, currentScreen->clipBound_Y1, &hits[1][0], &hits[1][1]);
    rayIntersect(cam, forward, right, up, currentScreen->clipBound_X1, currentScreen->clipBound_Y2, &hits[2][0], &hits[2][1]);
    rayIntersect(cam, forward, right, up, currentScreen->clipBound_X2, currentScreen->clipBound_Y2, &hits[3][0], &hits[3][1]);

    // camera pitching down toward plane
    if (sp != 0.0f) {
        // increase the amount we push out the bounding box after the corner intersections
        // fixes the flickering on jumps
        dimensionAdjust = 55;

        // move origin toward middle of screen
        rayIntersect(cam, forward, right, up, 159, 144, &hits[4][0], &hits[4][1]);
    } else {
        // origin should be roughly where the player is on screen (by their feet)
        rayIntersect(cam, forward, right, up, 159, 200, &hits[4][0], &hits[4][1]);
    }

    // Save unwrapped tile coords for LOD bound calculations
    float hitsRawX[4];
    float hitsRawZ[4];
    for (int i = 0; i < 4; i++) {
        float tileX = floor(hits[i][0] * recip16);
        float tileZ = floor(hits[i][1] * recip16);
        hitsRawX[i] = tileX;
        hitsRawZ[i] = tileZ;
        hits[i][0] = (float)((int)tileX & (width >> 4));
        hits[i][1] = (float)((int)tileZ & (height >> 4));
    }

    // this is the origin in tilemap coords
    hits[4][0] = (float)((int)floor(hits[4][0] * recip16) & (width >> 4));
    hits[4][1] = (float)((int)floor(hits[4][1] * recip16) & (height >> 4));

    // build a tilemap coord AABB for the visible area
    float minX = hits[0][0];
    if (hits[1][0] < minX)
        minX = hits[1][0];
    if (hits[2][0] < minX)
        minX = hits[2][0];
    if (hits[3][0] < minX)
        minX = hits[3][0];

    float minZ = hits[0][1];
    if (hits[1][1] < minZ)
        minZ = hits[1][1];
    if (hits[2][1] < minZ)
        minZ = hits[2][1];
    if (hits[3][1] < minZ)
        minZ = hits[3][1];

    float maxX = hits[0][0];
    if (hits[1][0] > maxX)
        maxX = hits[1][0];
    if (hits[2][0] > maxX)
        maxX = hits[2][0];
    if (hits[3][0] > maxX)
        maxX = hits[3][0];

    float maxZ = hits[0][1];
    if (hits[1][1] > maxZ)
        maxZ = hits[1][1];
    if (hits[2][1] > maxZ)
        maxZ = hits[2][1];
    if (hits[3][1] > maxZ)
        maxZ = hits[3][1];

    int minTileX = (int)minX;
    int maxTileX = (int)maxX;
    int minTileZ = (int)minZ;
    int maxTileZ = (int)maxZ;

    // expand the corners of the AABB a bit so geometry doesn't disappear
    minTileX -= dimensionAdjust;
    if (minTileX < 0) minTileX = 0;
    // special stage 7 is wider
    if (ufoNum == 7) {
        maxTileX += dimensionAdjust;
        if (maxTileX > 767)
            maxTileX = 767;
    } else {
        maxTileX += dimensionAdjust;
        if (maxTileX > 511)
            maxTileX = 511;
    }

    minTileZ -= dimensionAdjust;
    if (minTileZ < 0) minTileZ = 0;

    maxTileZ += dimensionAdjust;
    if (maxTileZ > 511) maxTileZ = 511;

    int originTX = (int)hits[4][0];
    int originTZ = (int)hits[4][1];

    // LOD bounds from unwrapped ray-cast coords
    float lodRawMinX = hitsRawX[0];
    float lodRawMaxX = hitsRawX[0];
    float lodRawMinZ = hitsRawZ[0];
    float lodRawMaxZ = hitsRawZ[0];
    for (int i = 1; i < 4; i++) {
        if (hitsRawX[i] < lodRawMinX)
            lodRawMinX = hitsRawX[i];
        if (hitsRawX[i] > lodRawMaxX)
            lodRawMaxX = hitsRawX[i];
        if (hitsRawZ[i] < lodRawMinZ)
            lodRawMinZ = hitsRawZ[i];
        if (hitsRawZ[i] > lodRawMaxZ)
            lodRawMaxZ = hitsRawZ[i];
    }

    const int mapTilesX = (width >> 4) + 1;
    const int mapTilesZ = (height >> 4) + 1;

    int lodMinTileX = (int)lodRawMinX - (int)dimensionAdjust;
    int lodMaxTileX = (int)lodRawMaxX + (int)dimensionAdjust;
    int lodMinTileZ = (int)lodRawMinZ - (int)dimensionAdjust;
    int lodMaxTileZ = (int)lodRawMaxZ + (int)dimensionAdjust;

    // clamp to at most one full map extent to avoid drawing duplicate tiles
    if (lodMaxTileX - lodMinTileX >= mapTilesX)
        lodMaxTileX = lodMinTileX + mapTilesX - 1;
    if (lodMaxTileZ - lodMinTileZ >= mapTilesZ)
        lodMaxTileZ = lodMinTileZ + mapTilesZ - 1;

    // real tiles always draw out to tileRadius; LOD extends beyond this
    const int tileRadius = 40;
    const float tileRadSq = (float)(tileRadius * tileRadius);

    // refine tilemap AABB, shouldn't be taller or wider than 2x tileRadius
    if ((originTX - minTileX) > tileRadius)
        minTileX = originTX - tileRadius;

    if ((maxTileX - originTX) > tileRadius)
        maxTileX = originTX + tileRadius;

    if ((originTZ - minTileZ) > tileRadius)
        minTileZ = originTZ - tileRadius;

    if ((maxTileZ - originTZ) > tileRadius)
        maxTileZ = originTZ + tileRadius;

    // this will be the first discard test if set to 1
#define DO_EUCLIDEAN_DISTANCE_TEST 1

    // everything in the game is drawn in layer groups, base depth of the group
    const float baseZ = RenderDevice::GetDepth();

    // --- LOD background quads ---
    // covers the visible area with a pre-rendered minimap texture (1 texel per tile)
    // real tiles render on top within the smaller tileRadius region
    // uses a shared vertex grid so adjacent cells have identical edge positions
    // only draw LOD quads when camera is not pitched down toward the floor (jumps)
    if (sp == 0.0f && lodTextureReady)  {
        bool lodPrepared = false;
        const int wrapMaskX = width >> 4;
        const int wrapMaskZ = height >> 4;

        const int lodTileSpanX = lodMaxTileX - lodMinTileX + 1;
        const int lodTileSpanZ = lodMaxTileZ - lodMinTileZ + 1;

        const int LOD_GRID = 16;

        // tile-aligned grid boundaries: each grid point sits on an exact tile edge
        // so LOD texels map 1:1 to tile positions
        int gridTileX[LOD_GRID + 1];
        int gridTileZ[LOD_GRID + 1];
        for (int i = 0; i <= LOD_GRID; i++) {
            gridTileX[i] = lodMinTileX + (i * lodTileSpanX) / LOD_GRID;
            gridTileZ[i] = lodMinTileZ + (i * lodTileSpanZ) / LOD_GRID;
        }

        // compute shared vertex grid at tile-aligned world positions
        // adjacent cells reference the same vertex, hopefully eliminating seams
        Vector4f gridVerts[LOD_GRID + 1][LOD_GRID + 1];
        uint8 gridValid[LOD_GRID + 1][LOD_GRID + 1];

        for (int gz = 0; gz <= LOD_GRID; gz++) {
            for (int gx = 0; gx <= LOD_GRID; gx++) {
                float wx = (float)(gridTileX[gx] * TILE_SIZE);
                float wz = (float)(gridTileZ[gz] * TILE_SIZE);

                gridVerts[gz][gx] = { wx, 0.0f, wz, 1.0f };
                mat_trans_nodiv(gridVerts[gz][gx].x, gridVerts[gz][gx].y,
                                gridVerts[gz][gx].z, gridVerts[gz][gx].w);

                if (gridVerts[gz][gx].w <= EPS) {
                    gridValid[gz][gx] = 0;
                } else {
                    float invW = shz_invf(gridVerts[gz][gx].w);
                    gridVerts[gz][gx].x *= invW;
                    gridVerts[gz][gx].y *= invW;
                    gridVerts[gz][gx].z *= invW;
                    gridVerts[gz][gx].z += baseZ - 0.001f;
                    gridVerts[gz][gx].w = invW;
                    gridValid[gz][gx] = 1;
                }
            }
        }

        // draw cells from shared vertices, only where real tiles don't cover
        const float lodOriginX = (float)originTX;
        const float lodOriginZ = (float)originTZ;

        for (int gz = 0; gz < LOD_GRID; gz++) {
            for (int gx = 0; gx < LOD_GRID; gx++) {
                if (!gridValid[gz][gx] || !gridValid[gz][gx + 1] ||
                    !gridValid[gz + 1][gx] || !gridValid[gz + 1][gx + 1])
                    continue;

                // skip cells fully inside the real tile radius to avoid z-fighting
                float cx0 = (float)gridTileX[gx] - lodOriginX;
                float cx1 = (float)gridTileX[gx + 1] - lodOriginX;
                float cz0 = (float)gridTileZ[gz] - lodOriginZ;
                float cz1 = (float)gridTileZ[gz + 1] - lodOriginZ;
                // if tiles are outside of the manhattan distance from origin
                // we can skip the more expensive euclidean distance test
                // so let's try the cheaper test first
                if (((cx0 + cz0) < (tileRadius)) &&
                    ((cx1 + cz1) < (tileRadius)) &&
                    ((cx0 + cz1) < (tileRadius)) &&
                    ((cx1 + cz1) < (tileRadius))
                ) {
                    if (((cx0*cx0 + cz0*cz0) < tileRadSq) &&
                        ((cx1*cx1 + cz0*cz0) < tileRadSq) &&
                        ((cx0*cx0 + cz1*cz1) < tileRadSq) &&
                        ((cx1*cx1 + cz1*cz1) < tileRadSq))
                        continue;
                }

                Vector4f &ul = gridVerts[gz][gx];
                Vector4f &ur = gridVerts[gz][gx + 1];
                Vector4f &ll = gridVerts[gz + 1][gx];
                Vector4f &lr = gridVerts[gz + 1][gx + 1];

                // screen edge culling
                if (ul.x < 0 && ur.x < 0 && ll.x < 0 && lr.x < 0) continue;
                if (ul.x > DEFAULT_PIXWIDTH && ur.x > DEFAULT_PIXWIDTH &&
                    ll.x > DEFAULT_PIXWIDTH && lr.x > DEFAULT_PIXWIDTH) continue;
                if (ul.y > 240 && ur.y > 240 && ll.y > 240 && lr.y > 240) continue;

                // UVs from tile-aligned grid boundaries (exact tile indices)
                int iu0 = gridTileX[gx] & wrapMaskX;
                int iu1 = gridTileX[gx + 1] & wrapMaskX;
                int iv0 = gridTileZ[gz] & wrapMaskZ;
                int iv1 = gridTileZ[gz + 1] & wrapMaskZ;

                // skip cells that straddle wrap boundary
                if (iu1 <= iu0 || iv1 <= iv0)
                    continue;

                // defer prepare until first drawable cell to avoid "not consumed" RenderDevice warning
                if (!lodPrepared) {
                    RenderDevice::PrepareTexturedPolyPTEX(currentScreen->clipBound_Y1, INK_NONE, &mapSurf);
                    lodPrepared = true;
                }

                // simulated floor distance shading, experimentally derived
                uint8 comp = (uint8)((1.0f - ((ur.w*250.0f) < 1.0f ? (ur.w*250.0f) : 1.0f)) * 84.0f);
                // uses vertex offset color to increase lightness (additive color)
                uint32 ocolor = 0xFF000000 | (comp << 16) | (comp << 8) | comp;
                const uint32 color = 0xFFEEEEEE;
                RenderDevice::DrawFloorTexturedPolyPTEx(
                    ul, ur, ll, lr,
                    iu0, iu1, iv0, iv1,
                    &mapSurf, color, ocolor
                );
            }
        }
    }

    // Prepare tileset texture for the real tile loop (moved from earlier to avoid
    // "not consumed" RenderDevice warning when LOD quad prepare comes between the two)
    RenderDevice::PrepareTexturedPolyPTEX(currentScreen->clipBound_Y1, INK_NONE, surface);

    // get some of the tilemap into cache, hopefully we use more than once cached tile
    __builtin_prefetch(&layout[(minTileX) + (minTileZ << layer->widthShift)]);

    int firstZ = firstNonEmptyRow > minTileZ ? firstNonEmptyRow : minTileZ;
    int lastZ = lastNonEmptyRow < maxTileZ ? lastNonEmptyRow : maxTileZ;

    for (int tz = firstZ; tz <= lastZ; tz++) {
        // only need to compute these once per row
        float z0 = (float)((uint32)(tz * TILE_SIZE));
        float z1 = z0 + TILE_SIZE;
#if DO_EUCLIDEAN_DISTANCE_TEST
        float zdiff = (originTZ - tz);
#endif
        bool cantReuse = true;

        // we did a few rounds of bounds refinements earlier
        // but for rows, there's one more that can be done
        // every row has different first and last non-empty tile
        // some rows are much narrower in these bounds
        // so use them
        int firstX = (minTileX < firstNonemptyTileInRow[tz]) ? firstNonemptyTileInRow[tz] : minTileX;
        int lastX  = (maxTileX >  lastNonemptyTileInRow[tz]) ?  lastNonemptyTileInRow[tz] : maxTileX;

        // so we can just increment to index next tile
        size_t layout_offset = firstX + (tz << layer->widthShift) - 1;

        for (int tx = firstX; tx <= lastX; tx++) {
            bool reuseRights = false;
            layout_offset++;

#if DO_EUCLIDEAN_DISTANCE_TEST
            // only test if camera is not pitched down to floor
            if (sp == 0.0f) {
                float xdiff = (originTX - tx);
                float xzdist = ((xdiff * xdiff) + (zdiff * zdiff));
                if (xzdist > tileRadSq) {
                    cantReuse = true;
                    continue;
                }
            }
#endif

            // raw tile number (0 - 1023)
            uint16 tile = layout[layout_offset] & 0xFFF;
            // FFF is an empty tile
            if (tile == 0xFFF) {
                cantReuse = true;
                continue;
            }

            // if the previous iteration of inner loop did not process empty tile,
            // we can reuse the upper/lower right verts to skip two sets of transforms
            if (!cantReuse) {
                reuseRights = true;
            }

            // processing a visible tile
            cantReuse = false;

            // u flip and v flip come from upper 2 bits of tile number
            int uf = 0;
            int vf = 0;
            if ((tile > 0x3FF) && (tile < 0x800)) {
                uf = 1;
            } else if ((tile > 0x7FF) && (tile < 0xC00)) {
                vf = 1;
            } else if (tile > 0xBFF) {
                uf = 1;
                vf = 1;
            }
            tile &= 0x3FF;

            // now we are going back to world coords from tile coords
            float x0 = (float)((uint32)(tx * TILE_SIZE));
            float x1 = x0 + TILE_SIZE;
            uint32 color;

            // prefetch a new cache-line sized run of tiles to speed up later iterations of inner loop
            __builtin_prefetch(&layout[layout_offset + 16]);

            if (reuseRights) {
                // early out test
                // one FIPR and one branch can skip all of the work for this tile
                float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                    x1, 0.0f, z0, 1.0f);
                // new upper-right vert would be very close to, or behind camera
                // skip the entire tile
                if (wTest <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // reuse previous iteration upper/lower right tiles
                tileVerts[TILE_UL].x = tileVerts[TILE_UR].x;
                tileVerts[TILE_UL].y = tileVerts[TILE_UR].y;
                tileVerts[TILE_UL].z = tileVerts[TILE_UR].z;
                tileVerts[TILE_UL].w = tileVerts[TILE_UR].w;

                tileVerts[TILE_LL].x = tileVerts[TILE_LR].x;
                tileVerts[TILE_LL].y = tileVerts[TILE_LR].y;
                tileVerts[TILE_LL].z = tileVerts[TILE_LR].z;
                tileVerts[TILE_LL].w = tileVerts[TILE_LR].w;

                // now create new upper/lower right tiles
                tileVerts[TILE_UR].x = x1;
                tileVerts[TILE_UR].y = 0.0f;
                tileVerts[TILE_UR].z = z0;
                tileVerts[TILE_UR].w = 1.0f;

                tileVerts[TILE_LR].x = x1;
                tileVerts[TILE_LR].y = 0.0f;
                tileVerts[TILE_LR].z = z1;
                tileVerts[TILE_LR].w = 1.0f;

                // transform new upper right
                mat_trans_nodiv(tileVerts[TILE_UR].x, tileVerts[TILE_UR].y, tileVerts[TILE_UR].z, tileVerts[TILE_UR].w);
                // redundant now
#if 0
                if (tileVerts[TILE_UR].w <= EPS) {
                    cantReuse = true;
                    continue;
                }
#endif

                // transform new lower right
                mat_trans_nodiv(tileVerts[TILE_LR].x, tileVerts[TILE_LR].y, tileVerts[TILE_LR].z, tileVerts[TILE_LR].w);
                // new lower-right vert would be very close to, or behind camera
                // skip the entire tile
                if (tileVerts[TILE_LR].w <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // perspective divide
                float wUR = tileVerts[TILE_UR].w;
                float wLR = tileVerts[TILE_LR].w;
                tileVerts[TILE_UR].w = shz_invf(wUR);
                tileVerts[TILE_LR].w = shz_invf(wLR);

                tileVerts[TILE_UR].x *= tileVerts[TILE_UR].w;
                tileVerts[TILE_UR].y *= tileVerts[TILE_UR].w;
                tileVerts[TILE_UR].z *= tileVerts[TILE_UR].w;
                tileVerts[TILE_UR].z += baseZ;

                tileVerts[TILE_LR].x *= tileVerts[TILE_LR].w;
                tileVerts[TILE_LR].y *= tileVerts[TILE_LR].w;
                tileVerts[TILE_LR].z *= tileVerts[TILE_LR].w;
                tileVerts[TILE_LR].z += baseZ;
            } else {
                // early out test
                // one FIPR and one branch can skip all of the work for this tile
                float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                    x0, 0.0f, z0, 1.0f);

                // upper-left vert would be very close to, or behind camera
                // skip the entire tile
                if (wTest < EPS) {
                    cantReuse = true;
                    continue;
                }

                // create all four tile verts
                tileVerts[TILE_UL].x = x0;
                tileVerts[TILE_UL].y = 0.0f;
                tileVerts[TILE_UL].z = z0;
                tileVerts[TILE_UL].w = 1.0f;

                tileVerts[TILE_UR].x = x1;
                tileVerts[TILE_UR].y = 0.0f;
                tileVerts[TILE_UR].z = z0;
                tileVerts[TILE_UR].w = 1.0f;

                tileVerts[TILE_LL].x = x0;
                tileVerts[TILE_LL].y = 0.0f;
                tileVerts[TILE_LL].z = z1;
                tileVerts[TILE_LL].w = 1.0f;

                tileVerts[TILE_LR].x = x1;
                tileVerts[TILE_LR].y = 0.0f;
                tileVerts[TILE_LR].z = z1;
                tileVerts[TILE_LR].w = 1.0f;

                // loop to transform and near-plane test all tile verts
                int negw = 0;
                for (int i=0;i<4;i++) {
                    mat_trans_nodiv(tileVerts[i].x, tileVerts[i].y, tileVerts[i].z, tileVerts[i].w);
                    float wi = tileVerts[i].w;
                    // vert `i` would be very close to, or behind camera
                    // set flag and break
                    if (wi <= EPS) {
                        negw = 1;
                        break;
                    }
                }

                // skip the entire tile if flag was set
                if (negw) {
                    cantReuse = true;
                    continue;
                }

                // perspective divide
                for (int i=0;i<4;i++) {
                    float wi = shz_invf(tileVerts[i].w);
                    tileVerts[i].w = wi;
                    tileVerts[i].x *= tileVerts[i].w;
                    tileVerts[i].y *= tileVerts[i].w;
                    tileVerts[i].z *= tileVerts[i].w;
                    tileVerts[i].z += baseZ;
                }
            }

            // we might still be able to discard tile if it is outside of screenspace bounds
            // off left side of screen
            if (tileVerts[0].x < 0 && tileVerts[1].x < 0 && tileVerts[2].x < 0 && tileVerts[3].x < 0)  {
                cantReuse = true;
                continue;
            }

            // off right side of screen
            if (tileVerts[0].x > DEFAULT_PIXWIDTH && tileVerts[1].x > DEFAULT_PIXWIDTH &&
                tileVerts[2].x > DEFAULT_PIXWIDTH && tileVerts[3].x > DEFAULT_PIXWIDTH)  {
                cantReuse = true;
                continue;
            }

            // off bottom of screen
            if (tileVerts[0].y > 240 && tileVerts[1].y > 240 && tileVerts[2].y > 240  && tileVerts[3].y > 240) {
                cantReuse = true;
                continue;
            }

            // I don't know if this actually happens, so this is disabled
#if 0
            // off top of screen
            if (sp != 0.0f) {
                if (tileVerts[0].y > 240 && tileVerts[1].y > 240 && tileVerts[2].y > 240  && tileVerts[3].y > 240) {
                    cantReuse = true;
                    continue;
                }
            }
#endif

            // simulated floor distance shading, experimentally derived
            uint8 comp = (uint8)((1.0f - ((tileVerts[TILE_UR].w*250.0f) < 1.0f ? (tileVerts[TILE_UR].w*250.0f) : 1.0f)) * 84.0f);
            uint32 ocolor;
            uint32 color;
            if (!pinball) {
                ocolor = 0xFF000000 | (comp << 16) | (comp << 8) | comp;
                color = 0xFFEEEEEE;
            } else {
                // no additive fade for pinball stage
                // instead, try to recreate the darkness fade at the upper end of table
                // applied to vertex color
                ocolor = 0x00000000;
                float invW = 1.0f / tileVerts[TILE_UR].w;
#define INVW_MAX 1000.0f
#define INVW_SHADE_THRESHOLD 475.0f
#define INVW_SHADE_FACTOR 478.0f
                // the invw stuff
                // 1/w == 475 nearing the uppermost end of the table, below the signage
                // past that point, it quickly approaches a value of around 1000
                // so past our invw threshold
                // and for the color component
                // scaling from 0xEE to black
                // 512 because its a nice computer number
                // ee/ff == 0.933333  * 512 == ~478
                if (invW > INVW_SHADE_THRESHOLD) {
                    comp = 0xEE - (((invW - INVW_SHADE_THRESHOLD) / (INVW_MAX - INVW_SHADE_THRESHOLD)) * 478.0f);
                    // clamp to 0
                    if (comp < 0) comp = 0;
                    // shading light to dark from threshold to top
                    color = 0xFF000000 | (comp << 16) | (comp << 8) | comp;
                }
            }
            // tile index in 32x32 square atlas
            int atlasX = (tile % KOS_ATLAS_WIDTH_TILES ) * TILE_SIZE;
            int atlasY = (tile / KOS_ATLAS_HEIGHT_TILES) * TILE_SIZE;

            int u0,u1,v0,v1;

            // handle u/v flip without recompiling headers
            int uoff0 = ((uf)  * TILE_SIZE);
            int uoff1 = ((!uf) * TILE_SIZE);
            int voff0 = ((vf)  * TILE_SIZE);
            int voff1 = ((!vf) * TILE_SIZE);

            // pull in edges by one texel to avoid adjacent tile pixels being drawn
            // ("holes" in the grid)
            if (uoff0 < uoff1) {
                uoff0 += 1;
                uoff1 -= 1;
            } else {
                uoff0 -= 1;
                uoff1 += 1;
            }

            if (voff0 < voff1) {
                voff0 += 1;
                voff1 -= 1;
            } else {
                voff0 -= 1;
                voff1 += 1;
            }

            u0 = atlasX + uoff0;
            u1 = atlasX + uoff1;
            v0 = atlasY + voff0;
            v1 = atlasY + voff1;

            RenderDevice::DrawFloorTexturedPolyPTEx(
                tileVerts[TILE_UL], tileVerts[TILE_UR],
                tileVerts[TILE_LL], tileVerts[TILE_LR],
                u0, u1,
                v0, v1,
                surface, color, ocolor
            );
        }
    }
    return;
#endif
#if !defined(KOS_HARDWARE_RENDERER)
    if (!layer->xsize || !layer->ysize)
        return;

    uint16 *layout         = layer->layout;
    uint8 *lineBuffer      = &gfxLineBuffer[currentScreen->clipBound_Y1];
    ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_Y1];
    uint16 *frameBuffer    = &currentScreen->frameBuffer[currentScreen->clipBound_X1 + currentScreen->clipBound_Y1 * currentScreen->pitch];

    int32 width    = (TILE_SIZE << layer->widthShift) - 1;
    int32 height   = (TILE_SIZE << layer->heightShift) - 1;
    int32 lineSize = currentScreen->clipBound_X2 - currentScreen->clipBound_X1;

    for (int32 cy = currentScreen->clipBound_Y1; cy < currentScreen->clipBound_Y2; ++cy) {
        int32 posX = scanline->position.x;
        int32 posY = scanline->position.y;

        uint16 *activePalette = fullPalette[*lineBuffer];
        ++lineBuffer;
        int32 fbOffset = currentScreen->pitch - lineSize;

        for (int32 cx = 0; cx < lineSize; ++cx) {
            int32 tx = posX >> 20;
            int32 ty = posY >> 20;
            int32 x  = FROM_FIXED(posX) & 0xF;
            int32 y  = FROM_FIXED(posY) & 0xF;

            uint16 tile = layout[((width >> 4) & tx) + (((height >> 4) & ty) << layer->widthShift)] & 0xFFF;
            uint8 idx   = tilesetPixels[TILE_SIZE * (y + TILE_SIZE * tile) + x];

            if (idx)
                *frameBuffer = activePalette[idx];

            posX += scanline->deform.x;
            posY += scanline->deform.y;
            ++frameBuffer;
        }

        frameBuffer += fbOffset;
        ++scanline;
    }
#endif
}
void RSDK::DrawLayerBasic(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

    if (currentScreen->clipBound_X1 >= currentScreen->clipBound_X2 || currentScreen->clipBound_Y1 >= currentScreen->clipBound_Y2)
        return;

    uint16 *activePalette = fullPalette[0];
    if (currentScreen->clipBound_X1 < currentScreen->clipBound_X2 && currentScreen->clipBound_Y1 < currentScreen->clipBound_Y2) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
        ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_Y1];

        int32 ty = FROM_FIXED(scanline->position.y) >> 4;

        const int32 offsetY = FROM_FIXED(scanline->position.y) & 0xF;
        int32 scanlineIncrement = TILE_SIZE - offsetY;

        for (int32 screenY = currentScreen->clipBound_Y1 - offsetY; screenY < currentScreen->clipBound_Y2; screenY += TILE_SIZE) {
            int32 tx = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) >> 4;
            uint16* layout = &layer->layout[tx + (ty << layer->widthShift)];

            const int32 offsetX = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) & 0xF;

            for (int32 screenX = currentScreen->clipBound_X1 - offsetX; screenX < currentScreen->clipBound_X2; screenX += TILE_SIZE) {
                if (*layout != 0xFFFF) {
                    DrawByLayout(*layout, screenX, screenY);
                }

                ++layout;
                if (++tx == layer->xsize) {
                    tx = 0;
                    layout -= layer->xsize;
                }
            }

            ty = (ty + 1) % layer->ysize;

            scanline += scanlineIncrement;
            scanlineIncrement = TILE_SIZE;
        }
#else
        int32 lineSize = (currentScreen->clipBound_X2 - currentScreen->clipBound_X1) >> 4;

        ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_Y1];

        int32 tx          = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) >> 4;
        int32 ty          = FROM_FIXED(scanline->position.y) >> 4;
        int32 sheetY      = FROM_FIXED(scanline->position.y) & 0xF;
        int32 sheetX      = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) & 0xF;
        int32 tileRemainX = TILE_SIZE - sheetX;
        int32 tileRemainY = TILE_SIZE - sheetY;

        uint16 *frameBuffer = &currentScreen->frameBuffer[currentScreen->clipBound_X1 + currentScreen->clipBound_Y1 * currentScreen->pitch];
        uint16 *layout      = &layer->layout[tx + (ty << layer->widthShift)];

        // Remaining pixels on top
        {
            if (*layout == 0xFFFF) {
                frameBuffer += TILE_SIZE - sheetX;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + TILE_SIZE * sheetY + sheetX];

                for (int32 y = 0; y < tileRemainY; ++y) {
                    for (int32 x = 0; x < tileRemainX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += sheetX;
                    frameBuffer += currentScreen->pitch - tileRemainX;
                }

                frameBuffer += tileRemainX - currentScreen->pitch * tileRemainY;
            }

            ++layout;
            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            for (int32 x = 0; x < lineSize; ++x) {
                if (*layout == 0xFFFF) {
                    frameBuffer += TILE_SIZE;
                }
                else {
                    uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + TILE_SIZE * sheetY];
                    for (int32 y = 0; y < tileRemainY; ++y) {
                        uint8 index = *pixels;
                        if (index)
                            *frameBuffer = activePalette[index];

                        index = pixels[1];
                        if (index)
                            frameBuffer[1] = activePalette[index];

                        index = pixels[2];
                        if (index)
                            frameBuffer[2] = activePalette[index];

                        index = pixels[3];
                        if (index)
                            frameBuffer[3] = activePalette[index];

                        index = pixels[4];
                        if (index)
                            frameBuffer[4] = activePalette[index];

                        index = pixels[5];
                        if (index)
                            frameBuffer[5] = activePalette[index];

                        index = pixels[6];
                        if (index)
                            frameBuffer[6] = activePalette[index];

                        index = pixels[7];
                        if (index)
                            frameBuffer[7] = activePalette[index];

                        index = pixels[8];
                        if (index)
                            frameBuffer[8] = activePalette[index];

                        index = pixels[9];
                        if (index)
                            frameBuffer[9] = activePalette[index];

                        index = pixels[10];
                        if (index)
                            frameBuffer[10] = activePalette[index];

                        index = pixels[11];
                        if (index)
                            frameBuffer[11] = activePalette[index];

                        index = pixels[12];
                        if (index)
                            frameBuffer[12] = activePalette[index];

                        index = pixels[13];
                        if (index)
                            frameBuffer[13] = activePalette[index];

                        index = pixels[14];
                        if (index)
                            frameBuffer[14] = activePalette[index];

                        index = pixels[15];
                        if (index)
                            frameBuffer[15] = activePalette[index];

                        frameBuffer += currentScreen->pitch;
                        pixels += TILE_SIZE;
                    }

                    frameBuffer += TILE_SIZE - currentScreen->pitch * tileRemainY;
                }

                ++layout;
                if (++tx == layer->xsize) {
                    tx = 0;
                    layout -= layer->xsize;
                }
            }

            if (*layout == 0xFFFF) {
                frameBuffer += currentScreen->pitch * tileRemainY;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + TILE_SIZE * sheetY];

                for (int32 y = 0; y < tileRemainY; ++y) {
                    for (int32 x = 0; x < sheetX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += tileRemainX;
                    frameBuffer += currentScreen->pitch - sheetX;
                }
            }
        }

        // We've drawn a single line of pixels, increase our variables
        frameBuffer += sheetX + -TILE_SIZE * lineSize - TILE_SIZE;
        scanline += tileRemainY;
        if (++ty == layer->ysize)
            ty = 0;

        // Draw the bulk of the tiles
        int32 lineTileCount = ((currentScreen->clipBound_Y2 - currentScreen->clipBound_Y1) >> 4) - 1;
        for (int32 l = 0; l < lineTileCount; ++l) {
            sheetX      = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) & 0xF;
            tx          = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) >> 4;
            tileRemainX = TILE_SIZE - sheetX;
            layout      = &layer->layout[tx + (ty << layer->widthShift)];

            // Draw any stray pixels on the left
            if (*layout == 0xFFFF) {
                frameBuffer += tileRemainX;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetX];

                for (int32 y = 0; y < TILE_SIZE; ++y) {
                    for (int32 x = 0; x < tileRemainX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += sheetX;
                    frameBuffer += currentScreen->pitch - tileRemainX;
                }

                frameBuffer += tileRemainX - TILE_SIZE * currentScreen->pitch;
            }
            ++layout;
            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            // Draw the bulk of the tiles on this line
            for (int32 x = 0; x < lineSize; ++x) {
                if (*layout == 0xFFFF) {
                    frameBuffer += TILE_SIZE;
                }
                else {
                    uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF)];

                    for (int32 y = 0; y < TILE_SIZE; ++y) {
                        uint8 index = *pixels;
                        if (index)
                            *frameBuffer = activePalette[index];

                        index = pixels[1];
                        if (index)
                            frameBuffer[1] = activePalette[index];

                        index = pixels[2];
                        if (index)
                            frameBuffer[2] = activePalette[index];

                        index = pixels[3];
                        if (index)
                            frameBuffer[3] = activePalette[index];

                        index = pixels[4];
                        if (index)
                            frameBuffer[4] = activePalette[index];

                        index = pixels[5];
                        if (index)
                            frameBuffer[5] = activePalette[index];

                        index = pixels[6];
                        if (index)
                            frameBuffer[6] = activePalette[index];

                        index = pixels[7];
                        if (index)
                            frameBuffer[7] = activePalette[index];

                        index = pixels[8];
                        if (index)
                            frameBuffer[8] = activePalette[index];

                        index = pixels[9];
                        if (index)
                            frameBuffer[9] = activePalette[index];

                        index = pixels[10];
                        if (index)
                            frameBuffer[10] = activePalette[index];

                        index = pixels[11];
                        if (index)
                            frameBuffer[11] = activePalette[index];

                        index = pixels[12];
                        if (index)
                            frameBuffer[12] = activePalette[index];

                        index = pixels[13];
                        if (index)
                            frameBuffer[13] = activePalette[index];

                        index = pixels[14];
                        if (index)
                            frameBuffer[14] = activePalette[index];

                        index = pixels[15];
                        if (index)
                            frameBuffer[15] = activePalette[index];

                        pixels += TILE_SIZE;
                        frameBuffer += currentScreen->pitch;
                    }

                    frameBuffer -= TILE_SIZE * currentScreen->pitch;
                    frameBuffer += TILE_SIZE;
                }

                ++layout;
                if (++tx == layer->xsize) {
                    tx = 0;
                    layout -= layer->xsize;
                }
            }

            // Draw any stray pixels on the right
            if (*layout == 0xFFFF) {
                frameBuffer += TILE_SIZE * currentScreen->pitch;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF)];

                for (int32 y = 0; y < TILE_SIZE; ++y) {
                    for (int32 x = 0; x < sheetX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += tileRemainX;
                    frameBuffer += currentScreen->pitch - sheetX;
                }
            }
            ++layout;
            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            // We've drawn a single line, increase our variables
            scanline += TILE_SIZE;
            frameBuffer += sheetX + -TILE_SIZE * lineSize - TILE_SIZE;
            if (++ty == layer->ysize)
                ty = 0;
        }

        // Remaining pixels on bottom
        {
            tx          = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) >> 4;
            sheetX      = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) & 0xF;
            tileRemainX = TILE_SIZE - sheetX;
            layout      = &layer->layout[tx + (ty << layer->widthShift)];

            if (*layout != 0xFFFF) {
                frameBuffer += tileRemainX;
            }
            else {
                uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF) + sheetX];

                for (int32 y = 0; y < sheetY; ++y) {
                    for (int32 x = 0; x < tileRemainX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += sheetX;
                    frameBuffer += currentScreen->pitch - tileRemainX;
                }

                frameBuffer += tileRemainX - currentScreen->pitch * sheetY;
            }
            ++layout;
            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }

            for (int32 x = 0; x < lineSize; ++x) {
                if (*layout == 0xFFFF) {
                    frameBuffer += TILE_SIZE;
                }
                else {
                    uint8 *pixels = &tilesetPixels[TILE_DATASIZE * (*layout & 0xFFF)];
                    for (int32 y = 0; y < sheetY; ++y) {
                        uint8 index = *pixels;
                        if (index)
                            *frameBuffer = activePalette[index];

                        index = pixels[1];
                        if (index)
                            frameBuffer[1] = activePalette[index];

                        index = pixels[2];
                        if (index)
                            frameBuffer[2] = activePalette[index];

                        index = pixels[3];
                        if (index)
                            frameBuffer[3] = activePalette[index];

                        index = pixels[4];
                        if (index)
                            frameBuffer[4] = activePalette[index];

                        index = pixels[5];
                        if (index)
                            frameBuffer[5] = activePalette[index];

                        index = pixels[6];
                        if (index)
                            frameBuffer[6] = activePalette[index];

                        index = pixels[7];
                        if (index)
                            frameBuffer[7] = activePalette[index];

                        index = pixels[8];
                        if (index)
                            frameBuffer[8] = activePalette[index];

                        index = pixels[9];
                        if (index)
                            frameBuffer[9] = activePalette[index];

                        index = pixels[10];
                        if (index)
                            frameBuffer[10] = activePalette[index];

                        index = pixels[11];
                        if (index)
                            frameBuffer[11] = activePalette[index];

                        index = pixels[12];
                        if (index)
                            frameBuffer[12] = activePalette[index];

                        index = pixels[13];
                        if (index)
                            frameBuffer[13] = activePalette[index];

                        index = pixels[14];
                        if (index)
                            frameBuffer[14] = activePalette[index];

                        index = pixels[15];
                        if (index)
                            frameBuffer[15] = activePalette[index];

                        pixels += TILE_SIZE;
                        frameBuffer += currentScreen->pitch;
                    }

                    frameBuffer += TILE_SIZE - currentScreen->pitch * sheetY;
                }

                ++layout;
                if (++tx == layer->xsize) {
                    tx = 0;
                    layout -= layer->xsize;
                }
            }

            if (*layout != 0xFFFF) {
                uint8 *pixels = &tilesetPixels[256 * (*layout & 0xFFF)];

                for (int32 y = 0; y < sheetY; ++y) {
                    for (int32 x = 0; x < sheetX; ++x) {
                        if (*pixels)
                            *frameBuffer = activePalette[*pixels];
                        ++pixels;
                        ++frameBuffer;
                    }

                    pixels += tileRemainX;
                }

                frameBuffer += currentScreen->pitch - sheetX;
            }
        }
#endif
    }
}
