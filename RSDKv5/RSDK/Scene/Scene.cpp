#include "RSDK/Core/RetroEngine.hpp"

#ifdef KOS_HARDWARE_RENDERER
#include <RSDK/Graphics/KallistiOS/AniTileTracker.hpp>
#include <sh4zam/shz_sh4zam.h>
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
static bool isMMZ1 = false;

// LOD background quad state
static bool lodTextureReady = false;
static int lodTexWidth = 0;
static int lodTexHeight = 0;

// 3D Floor composite texture: the entire 16x16 tilemap baked into a single 256x256 PAL8
// texture at scene load. Rendered as a grid of large quads with PVR UV repeat instead of
// hundreds of individual tile quads, giving the appearance of an infinite tiled plane.
static GFXSurface floorSurf{};
static bool floorTextureReady = false;
static int floorTilesW = 0;
static int floorTilesH = 0;

// 3D Roof: 16 pre-baked 128×128 PAL8 animation frames, one quad per frame with UV repeat.
#define ROOF_FRAME_COUNT 16
#define ROOF_TEX_SIZE    128
static GFXSurface roofFrames[ROOF_FRAME_COUNT]{};
static bool roofTextureReady = false;

// bump the tile radius a bit when checking if LOD overlaps high detail tiles
// it was too close before and there was too much overlap
#define LOD_RAD_OFFSET 6
#define LOD_RAD_OFF_SQ (LOD_RAD_OFFSET * LOD_RAD_OFFSET)
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS && defined(KOS_HARDWARE_RENDERER)
static void ReleaseLODTexture()
{
    if (mapSurf.texture != nullptr) {
        pvr_mem_free(mapSurf.texture);
        mapSurf.texture = nullptr;
    }
}

// Free the composite floor texture VRAM when leaving a UFO stage
static void ReleaseFloorTexture()
{
    if (floorSurf.texture != nullptr) {
        pvr_mem_free(floorSurf.texture);
        floorSurf.texture = nullptr;
    }
    floorTextureReady = false;
}

static void ReleaseRoofTextures()
{
    for (int i = 0; i < ROOF_FRAME_COUNT; i++) {
        if (roofFrames[i].texture != nullptr) {
            pvr_mem_free(roofFrames[i].texture);
            roofFrames[i].texture = nullptr;
        }
    }
    roofTextureReady = false;
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

// Build a single composite texture from the "3D Floor" tilemap at scene load.
// The floor layer is a small wrapping tilemap (typically 16x16 tiles = 256x256 px).
// On KOS, raw tile pixels only exist temporarily during GIF loading (persistTiles),
// so this must run before GenerateLODTexture which frees that buffer.
// The result is a PAL8 texture in VRAM that can be drawn with a few large quads
// instead of the hundreds of individual tile draws the old renderer required.
// Stages without a "3D Floor" layer (not all UFO stages have one) bail out early,
// and the renderer checks floorTextureReady before drawing.
static void GenerateFloorTexture()
{
    floorTextureReady = false;

    if (persistTiles == nullptr)
        return;

    ReleaseFloorTexture();

    // find the "3D Floor" layer by name hash
    const char *layerName = "3D Floor";
    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5(layerName, hash);

    TileLayer *floorLayer = nullptr;
    for (int l = 0; l < LAYER_COUNT; l++) {
        if (tileLayers[l].layout && HASH_MATCH_MD5(tileLayers[l].name, hash)) {
            floorLayer = &tileLayers[l];
            break;
        }
    }

    if (!floorLayer)
        return;

    floorTilesW = floorLayer->xsize;
    floorTilesH = floorLayer->ysize;
    int texW    = floorTilesW * TILE_SIZE;
    int texH    = floorTilesH * TILE_SIZE;

    if (texW == 0 || texH == 0)
        return;

    // temp RAM buffer to assemble the composite before uploading to VRAM
    uint8 *composite = nullptr;
    AllocateStorage(reinterpret_cast<void **>(&composite), texW * texH, DATASET_TMP, true);
    if (!composite)
        return;

    uint16 *layout   = floorLayer->layout;
    int widthShift   = floorLayer->widthShift;

    // blit each tile from persistTiles into its position in the composite,
    // applying horizontal (uf) and vertical (vf) flip flags from the layout
    for (int ty = 0; ty < floorTilesH; ty++) {
        for (int tx = 0; tx < floorTilesW; tx++) {
            uint16 tileVal = layout[tx + (ty << widthShift)];
            if (tileVal == 0xFFFF)
                continue;

            uint16 tile = tileVal & 0x3FF;
            int uf      = (tileVal >> 10) & 1;
            int vf      = (tileVal >> 11) & 1;

            uint8 *src    = &persistTiles[tile * TILE_DATASIZE];
            int dstBaseX  = tx * TILE_SIZE;
            int dstBaseY  = ty * TILE_SIZE;

            for (int py = 0; py < TILE_SIZE; py++) {
                int srcY      = vf ? (TILE_SIZE - 1 - py) : py;
                uint8 *srcRow = &src[srcY * TILE_SIZE];
                uint8 *dstRow = &composite[(dstBaseY + py) * texW + dstBaseX];

                if (uf) {
                    for (int px = 0; px < TILE_SIZE; px++)
                        dstRow[px] = srcRow[TILE_SIZE - 1 - px];
                } else {
                    memcpy(dstRow, srcRow, TILE_SIZE);
                }
            }
        }
    }

    // upload as a PAL8 (8bpp palettized) texture; pvr_txr_load_ex auto-twiddles
    floorSurf.isARGB = 0;
    floorSurf.isVq   = 0;
    floorSurf.width   = texW;
    floorSurf.height  = texH;
    floorSurf.pixels  = nullptr;

    pvr_ptr_t texture = pvr_mem_malloc(texW * texH);
    if (!texture) {
        RemoveStorageEntry(reinterpret_cast<void **>(&composite));
        return;
    }

    floorSurf.texture = texture;
    pvr_txr_load_ex(composite, texture, texW, texH, PVR_TXRLOAD_8BPP);

    RemoveStorageEntry(reinterpret_cast<void **>(&composite));

    floorTextureReady = true;
    printf("[FLOOR] Generated %dx%d composite floor texture (%d bytes VRAM)\n", texW, texH, texW * texH);
}

// Called from LoadSpriteSheet when Water.gif is loaded, before pixels are freed.
void BuildRoofTexturesFromSheet(uint8 *pixels, int32 width, int32 height)
{
    roofTextureReady = false;
    ReleaseRoofTextures();

    if (!pixels || width < 512 || height < 512)
        return;

    // 16 animation frames in a 4×4 grid of 128×128 regions on the 512×512 sheet
    // turn each frame into a dedicated 8bpp texture for PVR
    int32 frame = 0;
    for (int32 fy = 0; fy < 4; fy++) {
        for (int32 fx = 0; fx < 4; fx++) {
            int32 srcBaseX = fx * ROOF_TEX_SIZE;
            int32 srcBaseY = fy * ROOF_TEX_SIZE;

            uint8 composite[ROOF_TEX_SIZE * ROOF_TEX_SIZE];
            for (int32 py = 0; py < ROOF_TEX_SIZE; py++) {
                memcpy(&composite[py * ROOF_TEX_SIZE],
                       &pixels[(srcBaseY + py) * width + srcBaseX],
                       ROOF_TEX_SIZE);
            }

            pvr_ptr_t texture = pvr_mem_malloc(ROOF_TEX_SIZE * ROOF_TEX_SIZE);
            if (!texture) {
                printf("[ROOF] VRAM alloc failed for frame %d\n", frame);
                return;
            }

            roofFrames[frame].isARGB  = 0;
            roofFrames[frame].isVq    = 0;
            roofFrames[frame].width   = ROOF_TEX_SIZE;
            roofFrames[frame].height  = ROOF_TEX_SIZE;
            roofFrames[frame].pixels  = nullptr;
            roofFrames[frame].texture = texture;

            pvr_txr_load_ex(composite, texture, ROOF_TEX_SIZE, ROOF_TEX_SIZE, PVR_TXRLOAD_8BPP);
            frame++;
        }
    }

    roofTextureReady = true;
    printf("[ROOF] Generated %d x %dx%d PAL8 textures (%d bytes VRAM)\n",
           ROOF_FRAME_COUNT, ROOF_TEX_SIZE, ROOF_TEX_SIZE,
           ROOF_FRAME_COUNT * ROOF_TEX_SIZE * ROOF_TEX_SIZE);
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
    // DC_SILHOUETTE: remove regions before each level
    ClearSilhouetteRegions();
    ReleaseLODTexture();
    ReleaseFloorTexture();
    ReleaseRoofTextures();
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
    isMMZ1 = strcmp(currentSceneFolder, "MMZ") == 0 && sceneEntry->id[0] == '1';

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
    if (ufoNum) {
        GenerateFloorTexture();
        GenerateLODTexture();
    }
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
                AllocatePinnedStorage(reinterpret_cast<void**>(&persistTiles), TILESET_SIZE, DATASET_STG, false);
                if (persistTiles == nullptr) {
                    printf("[NG] Failed to allocate for persistent tile set!!!: %s\n", filepath);
                }
            }
        }
        AllocatePinnedStorage(reinterpret_cast<void**>(&tilesetPixels), 2 * TILESET_SIZE, DATASET_TMP, true);

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

        // Don't take any chances with the GC checking stack variables.
        RemoveStorageEntry((void**)&tilesetPixels);
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
    const int32 tileInk = forceBlackTileRender ? INK_BLACK : INK_NONE;

    if (usePoly || forceBlackTileRender) {
        RenderDevice::PrepareTexturedPolyPT(prepY, tileInk, surface);

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

int32 GetLayerWrappedFromFixedY(const TileLayer* layer, int32 fixed_y) {
    const int32 tileY = FROM_FIXED(fixed_y);

    if (tileY >= TILE_SIZE * layer->ysize) {
        fixed_y -= TO_FIXED(TILE_SIZE * layer->ysize);
    }
    else if (tileY < 0) {
        fixed_y += TO_FIXED(TILE_SIZE * layer->ysize);
    }

    return FROM_FIXED(fixed_y);
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

            const int32 tileLeft  = std::min(screenUpperX, screenLowerX);
            const int32 tileRight = std::max(screenUpperX, screenLowerX) + TILE_SIZE;
            if (tileRight <= currentScreen->clipBound_X1 || tileLeft >= currentScreen->clipBound_X2) {
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
#if defined(KOS_HARDWARE_RENDERER)
// VScroll layers with a scanlineCallback (e.g. SSZ2 tower) apply per-pixel-column
// X displacement to create deformation effects (SSZ2 tower has cylindrical perspective).
// The normal DC VScroll renderer draws tile-wide (16px) columns so it misses this entirely.
//
// This alternative renderer iterates pixel-by-pixel across the screen, grouping consecutive
// columns that map to the same tilemap tile x index. Each group becomes a variable-width
// strip. At the center of the tilemap, these are a full 16 pixels wide, narrowing at the edges
// where the perspective compresses the tilemap. The PVR handles the compression by
// mapping the full tile texture onto the narrower screen quad.
static void DrawTowerLayerVScroll(TileLayer *layer)
{
    const int32 pixelWidth  = TILE_SIZE * layer->xsize;
    const int32 clipX1      = currentScreen->clipBound_X1;
    const int32 clipX2      = currentScreen->clipBound_X2;
    const int32 clipY1      = currentScreen->clipBound_Y1;
    const int32 clipY2      = currentScreen->clipBound_Y2;
    const int32 screenH     = currentScreen->size.y;
    const GFXSurface *surface = &gfxSurface[0];

    RenderDevice::PrepareTexturedQuadPT(0, surface);

    int32 prevTileX  = -1;
    int32 stripStart = clipX1;
    int32 stripSubX0 = 0;

    // The scanline callback has already run, so scanlines[screenColumn].position.x
    // tells us which tilemap x pixel each screen column should show.
    //
    // We walk every screen column left to right. Each column maps to some tile in the
    // tilemap. As long as consecutive columns map to the SAME tilemap x index,
    // we accumulate them.
    // When the tilemap x index changes (or we hit the right edge of the screen),
    // we draw everything we accumulated as one quad per row (a vertical column of tiles)
    // at whatever width the group ended up being after the scanline deformation.
    // In the center of the effect that width is a full 16 pixels (normal tile).
    // At the edges where the deformation compresses the output tiles,
    // the width is smaller, and the PVR squishes the tile texture into that narrower space.
    //
    // It does a convincing job of recreating the original scanline deformation effect.
    for (int32 cx = clipX1; cx <= clipX2; cx++) {
        // What tilemap x does this screen column map to?
        int32 mapX = FROM_FIXED(scanlines[MIN(cx, clipX2 - 1)].position.x);
        if (mapX < 0)
            mapX += pixelWidth;
        mapX %= pixelWidth;

        int32 tileX = mapX >> 4; // which tile column in the tilemap
        int32 subX = mapX & 0xF; // pixel offset within that tile (0-15)

        // If we've crossed into a new tile (or hit the screen edge), draw the previous group
        bool endStrip = (cx == clipX2) || (tileX != prevTileX && prevTileX >= 0);

        if (endStrip && prevTileX >= 0) {
            int32 stripWidth = cx - stripStart;
            if (stripWidth <= 0)
                goto nextStrip;

            // Vertical scroll comes from the scanline at the center of this group
            int32 centerCx = stripStart + stripWidth / 2;
            int32 mapY = FROM_FIXED(scanlines[centerCx].position.y);
            int32 pixelHeight = TILE_SIZE * layer->ysize;
            if (mapY < 0)
                mapY += pixelHeight;
            mapY %= pixelHeight;

            int32 subY = mapY & 0xF;
            int32 startTY = mapY >> 4;
            int32 screenY = -subY;
            int32 tilesToDraw = (screenH + subY + TILE_SIZE - 1) / TILE_SIZE + 1;

            // The group's first column started at sub-tile offset stripSubX0,
            // and the last column is at endSubX. Together they define which
            // horizontal slice of the tile texture to show in this quad.
            int32 endSubX  = FROM_FIXED(scanlines[cx - 1].position.x);
            if (endSubX < 0)
                endSubX += pixelWidth;
            endSubX = (endSubX % pixelWidth) & 0xF;

            // Draw every tile in this vertical column, one per row
            for (int32 t = 0; t < tilesToDraw; t++) {
                int32 ty = (startTY + t) % layer->ysize;
                int32 sy = screenY + t * TILE_SIZE;

                if (sy + TILE_SIZE <= clipY1 || sy >= clipY2)
                    continue;

                uint16 layout = layer->layout[prevTileX + (ty << layer->widthShift)];
                if (layout == 0xFFFF)
                    continue;

                layout &= 0xFFF;
                int32 flip = layout / TILE_COUNT;
                layout %= TILE_COUNT;

                // Find this tile's position in the texture atlas
                int32 tilesetX = TILE_SIZE * (static_cast<int32>(layout) % KOS_ATLAS_WIDTH_TILES);
                int32 tilesetY = TILE_SIZE * (static_cast<int32>(layout) / KOS_ATLAS_WIDTH_TILES);

                // source UV
                // only the horizontal slice [stripSubX0..endSubX] of the tile,
                // mapped across stripWidth pixels on screen
                // always full height
                int32 srcX0 = tilesetX + stripSubX0;
                int32 srcX1 = tilesetX + endSubX + 1;
                int32 srcY0 = tilesetY;
                int32 srcY1 = tilesetY + TILE_SIZE;

                if (flip & FLIP_X) {
                    int32 tmpX0 = tilesetX + (TILE_SIZE - (endSubX + 1));
                    int32 tmpX1 = tilesetX + (TILE_SIZE - stripSubX0);
                    srcX0 = tmpX1;
                    srcX1 = tmpX0;
                }
                if (flip & FLIP_Y) {
                    int32 tmp = srcY0;
                    srcY0 = srcY1;
                    srcY1 = tmp;
                }

                RenderDevice::DrawTexturedQuadPT(stripStart, sy, stripWidth, TILE_SIZE,
                                                 srcX0, srcX1, srcY0, srcY1, surface);
            }
        }

    nextStrip:
        // Start tracking a new group whenever we enter a different tile
        if (tileX != prevTileX) {
            prevTileX  = tileX;
            stripStart = cx;
            stripSubX0 = subX;
        }
    }
}
#endif
void RSDK::DrawLayerVScroll(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

#if defined(KOS_HARDWARE_RENDERER)
    if (layer->scanlineCallback) {
        DrawTowerLayerVScroll(layer);
        return;
    }

    const ScanlineInfo* leftScanline = &scanlines[currentScreen->clipBound_X1];

    const int32 sheetX = FROM_FIXED(leftScanline->position.x) & 0xF;
    int32 scanlineIncrement = TILE_SIZE - sheetX;

    const int32 screenTileAligned = AlignUp(currentScreen->size.y, TILE_SIZE) / TILE_SIZE;

    for (int32 cx = currentScreen->clipBound_X1 - sheetX; cx < currentScreen->clipBound_X2; cx += TILE_SIZE) {
        const int32 xRemainder = currentScreen->clipBound_X2 - cx;
        const int32 fromFixedLeftX = FROM_FIXED(leftScanline->position.x);
        const ScanlineInfo* rightScanline = leftScanline + std::min<int32>(scanlineIncrement - 1, xRemainder - 1);

        int32 tilesToDraw = screenTileAligned;
        int32 leftScanlineFixedY = leftScanline->position.y;

        int32 fromFixedLeftY = GetLayerWrappedFromFixedY(layer, leftScanlineFixedY + leftScanline->deform.y) % currentScreen->pitch; // WIP: pitch?

        const int32 offsetDelta = FROM_FIXED(leftScanline->deform.y - rightScanline->deform.y);
        int32 leftScreenOffsetY = -(fromFixedLeftY & 0xF);
        int32 rightScreenOffsetY = leftScreenOffsetY + offsetDelta;

        int32 extraTilesToDraw = abs(AlignUp(std::max(leftScreenOffsetY, rightScreenOffsetY), TILE_SIZE));

        leftScanlineFixedY -= TO_FIXED(extraTilesToDraw);
        leftScreenOffsetY -= extraTilesToDraw;
        rightScreenOffsetY -= extraTilesToDraw;

        extraTilesToDraw += abs(AlignDown(std::min(leftScreenOffsetY, rightScreenOffsetY), TILE_SIZE));

        tilesToDraw += extraTilesToDraw / TILE_SIZE;

        fromFixedLeftY = GetLayerWrappedFromFixedY(layer, leftScanlineFixedY + leftScanline->deform.y);

        const int32 screenLeftX = cx;
        const int32 screenRightX = cx + TILE_SIZE;

        const int32 tx = fromFixedLeftX >> 4;
        int32 ty = fromFixedLeftY >> 4;

        for (int32 t = 0; t < tilesToDraw; ++t) {
            const int32 cy = TILE_SIZE * t;
            const int32 screenLeftY = cy + leftScreenOffsetY;
            const int32 screenRightY = cy + rightScreenOffsetY;

            const uint16 layout = layer->layout[tx + (ty << layer->widthShift)];
            ty = (ty + 1) % layer->ysize;

            if (layout == 0xFFFF) {
                continue;
            }

            const int32 tileTop = std::min(screenLeftY, screenRightY);
            const int32 tileBot = std::max(screenLeftY, screenRightY) + TILE_SIZE;
            if (tileBot <= currentScreen->clipBound_Y1 || tileTop >= currentScreen->clipBound_Y2) {
                continue;
            }

            const Vector2 screenUpperLeft {
                screenLeftX,
                screenLeftY,
            };

            const Vector2 screenUpperRight {
                screenRightX,
                screenRightY,
            };

            const Vector2 screenLowerLeft {
                screenLeftX,
                screenLeftY + TILE_SIZE,
            };

            const Vector2 screenLowerRight {
                screenRightX,
                screenRightY + TILE_SIZE,
            };

            DrawByLayoutEx(layout,
                           screenUpperLeft, screenUpperRight,
                           screenLowerLeft, screenLowerRight);
        }

        leftScanline += scanlineIncrement;
        scanlineIncrement = TILE_SIZE;
    }
#else
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
    float px = (float)screenX - (320.0f*0.5f);
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

#if defined(KOS_HARDWARE_RENDERER)
static void DrawRotozoomIsland(TileLayer *layer);
static void DrawLayerRotozoom2D(TileLayer *layer);
static void DrawLayerRotozoomAffine(TileLayer *layer);
static void DrawRotozoom3DRoof(TileLayer *layer);
static void DrawRotozoom3DFloor(TileLayer *layer);
static void DrawRotozoomPinballBG(TileLayer *layer);
static void DrawRotozoomPlayfield(TileLayer *layer, bool pinball);

// Shared camera state unpacked from hijacked scanline data and converted to floats.
// Used by Roof, Floor, PinballBG, and Playfield handlers.
struct RotozoomCamera {
    Vector4f cam;
    Vector4f forward, right, up;
    float drc, duc, dfc;
    float f, foa;
    float sp;
};

// Unpack camera orientation and position from scanline entries 1+2, build orthonormal
// basis vectors, and compute the view-projection dot products and FOV scalars.
// Advances scanline past the consumed entries.
static RotozoomCamera UnpackRotozoomCamera(ScanlineInfo *&scanline)
{
    RotozoomCamera rc;

    scanline++;
    float sy = (float)scanline->deform.x * recip1k;
    float cy = (float)scanline->deform.y * recip1k;
    float sp = (float)scanline->position.x * recip1k;
    float cp = (float)scanline->position.y * recip1k;
    rc.sp = sp;

    scanline++;
    rc.cam.x = (float)scanline->deform.x * recip64k;
    rc.cam.y = (float)scanline->deform.y * recip64k;
    rc.cam.z = (float)scanline->position.x * recip64k;

    rc.forward = { sy * cp, -sp, cy * cp };
    rc.right   = { cy, 0.0f, -sy };
    rc.up      = { sy * sp, cp, cy * sp };

    vec3f_normalize(rc.forward.x, rc.forward.y, rc.forward.z);
    vec3f_normalize(rc.right.x, rc.right.y, rc.right.z);
    vec3f_normalize(rc.up.x, rc.up.y, rc.up.z);

    rc.drc = fipr(rc.right.x,   rc.right.y,   rc.right.z,   0, rc.cam.x, rc.cam.y, rc.cam.z, 0);
    rc.duc = fipr(rc.up.x,      rc.up.y,      rc.up.z,      0, rc.cam.x, rc.cam.y, rc.cam.z, 0);
    rc.dfc = fipr(rc.forward.x, rc.forward.y, rc.forward.z, 0, rc.cam.x, rc.cam.y, rc.cam.z, 0);

    const float aspect = 320.0f / 240.0f;
    const float fovy   = 47.5f * RSDK_PI / 180.0f;
    rc.f   = 1.0f / tanf(fovy * 0.5f);
    rc.foa = rc.f / aspect;

    return rc;
}

// Build the combined screenspace * viewproj matrix and load it into the SH4 XMTRX
// for use with mat_trans_nodiv. Optionally stores a copy in finalmat (pass nullptr to skip).
// fHoriz/fVert allow per-handler FOV overrides (pass rc.foa/rc.f for the standard projection).
static void LoadRotozoomMatrix(const RotozoomCamera &rc, float fHoriz, float fVert,
                               void *finalmat)
{
    const float __attribute__((aligned(32))) viewproj[4][4] = {
        { -rc.right.x * fHoriz,  rc.up.x * fVert,  rc.forward.x, rc.forward.x },
        { -rc.right.y * fHoriz,  rc.up.y * fVert,  rc.forward.y, rc.forward.y },
        { -rc.right.z * fHoriz,  rc.up.z * fVert,  rc.forward.z, rc.forward.z },
        {  rc.drc     * fHoriz, -rc.duc  * fVert,  -rc.dfc,      -rc.dfc      },
    };

    const float __attribute__((aligned(32))) screenspace[4][4] = {
        { 320.0f * 0.5f,    0.0f, 0.0f, 0.0f },
        {          0.0f, -120.0f, 0.0f, 0.0f },
        {          0.0f,    0.0f, 0.5f, 0.0f },
        { 320.0f * 0.5f,  120.0f, 0.5f, 1.0f },
    };

    mat_load(&screenspace);
    mat_apply(&viewproj);
    if (finalmat)
        mat_store(reinterpret_cast<float (*)[4][4]>(finalmat));
}

// Compute atlas UVs for a tile, applying horizontal/vertical flip and 1-texel edge pull.
// Checks AniTileTracker for animated tile overrides; returns the surface to use.
static const GFXSurface *TileAtlasUV(uint16 tile, int32 uf, int32 vf,
                                      int32 &u0, int32 &u1, int32 &v0, int32 &v1,
                                      const GFXSurface *defaultSurface)
{
    const AniTileState *aniState = AniTileTracker::GetAniTile(tile);
    const GFXSurface *tileSurface;

    if (aniState != nullptr && aniState->sheetID != 0) {
        tileSurface = &gfxSurface[aniState->sheetID];
        u0 = aniState->u;
        u1 = aniState->u;
        v0 = aniState->v;
        v1 = aniState->v;
    } else {
        tileSurface = defaultSurface;
        u0 = (tile % KOS_ATLAS_WIDTH_TILES) * TILE_SIZE;
        u1 = u0;
        v0 = (tile / KOS_ATLAS_HEIGHT_TILES) * TILE_SIZE;
        v1 = v0;
    }

    if (uf) {
        u0 += TILE_SIZE;
    } else {
        u1 += TILE_SIZE;
    }

    if (vf) {
        v0 += TILE_SIZE;
    } else {
        v1 += TILE_SIZE;
    }

    if (u0 < u1) {
        u0++;
        u1--;
    } else {
        u0--;
        u1++;
    }

    if (v0 < v1) {
        v0++;
        v1--;
    } else {
        v0--;
        v1++;
    }

    return tileSurface;
}

// MMZ1 far plane rendering
// this does not require looking for tile crossings across scanlines,
// this is literally just a 50% scaled tile layer offset from origin
static void DrawRotozoomMMZ1(TileLayer *layer)
{
    ScanlineInfo *scanline = &scanlines[currentScreen->clipBound_Y1];

    int32 ty = FROM_FIXED(scanline->position.y) >> 4;

    const int32 offsetY = FROM_FIXED(scanline->position.y) & 0xF;
    int32 scanlineIncrement = (16 - offsetY)/2;

    const int32 clipX2w = currentScreen->clipBound_X2 << 1;
    const int32 clipY2w = currentScreen->clipBound_Y2 << 1;

    for (int32 screenY = currentScreen->clipBound_Y1 - offsetY; screenY < clipY2w; screenY += TILE_SIZE) {
        int32 tx = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) >> 4;

        uint16* layout = &layer->layout[tx + (ty << layer->widthShift)];

        const int32 offsetX = (currentScreen->clipBound_X1 + FROM_FIXED(scanline->position.x)) & 0xF;

        for (int32 screenX = currentScreen->clipBound_X1 - offsetX; screenX < clipX2w; screenX += TILE_SIZE) {
            if (*layout != 0xFFFF) {
                Vector2 ul;
                Vector2 ur;
                Vector2 ll;
                Vector2 lr;

                ul.x = (screenX * 0.5f);
                ll.x = ul.x;
                ur.x = ul.x + 8;
                lr.x = ur.x;
                ul.y = (screenY* 0.5f);
                ll.y = ul.y + 8;
                ur.y = ul.y;
                lr.y = ur.y + 8;

                DrawByLayoutEx(*layout, ul, ur, ll, lr);
            }

            ++layout;

            if (++tx == layer->xsize) {
                tx = 0;
                layout -= layer->xsize;
            }
        }

        ty = (ty + 1) % layer->ysize;

        scanline += scanlineIncrement;
        scanlineIncrement = TILE_SIZE / 2;
    }
    return;
}

// most rotozoom layers only deform in the x dimension
// some (the clouds) deform in in the y dimension also
// for the x-only deforms, we have a simplified function
// for others, we call out to something that can handle y deform
static void DrawLayerRotozoom2D(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

    const int32 clipY1 = currentScreen->clipBound_Y1;
    const int32 clipY2 = currentScreen->clipBound_Y2;
    if (clipY1 >= clipY2)
        return;
    const int32 lineSize = currentScreen->clipBound_X2 - currentScreen->clipBound_X1;
    if (lineSize <= 0)
        return;

    // if any visible scanline has deform.y != 0, use the general affine renderer
    for (int32 cy = clipY1; cy < clipY2; cy++) {
        if (scanlines[cy].deform.y != 0) {
            DrawLayerRotozoomAffine(layer);
            return;
        }
    }

    // All source coordinates in scanline data are 16.16 fixed-point.
    // Tiles are TILE_SIZE (16) pixels, so one tile = 16 << 16 = 1 << 20 source units.
    // widthMask/heightMask wrap in pixel space; >> 4 gives tile-index masks.
    const int32 widthMask  = (TILE_SIZE << layer->widthShift) - 1;
    const int32 heightMask = (TILE_SIZE << layer->heightShift) - 1;
    const int32 txMask     = widthMask >> 4;
    const int32 tyMask     = heightMask >> 4;
    uint16 *layout         = layer->layout;
    const int32 widthShift = layer->widthShift;

    const GFXSurface *atlasSurface = &gfxSurface[0];
    const float baseZ              = RenderDevice::GetDepth();
    const float fLineSize          = (float)lineSize;

    RenderDevice::PrepareTexturedPolyPTEX(clipY1, INK_NONE, atlasSurface);
    const GFXSurface *preparedSurface = atlasSurface;

    // Detect scanline discontinuities (deform.x jumps from AddDynamicBG overlays)
    // and build a list of band boundaries that avoids interpolating across them.
    // 16 slots: clipY1 + clipY2 + up to 10 overlay boundaries (5 overlays × enter/exit)
    // those overlays only exist on the FBZ1 rotozoom background layer
    int32 bandEdges[16];
    int32 bandEdgeCount = 0;
    bandEdges[bandEdgeCount++] = clipY1;

    // 0x1000 (16.16 fixed) = 1/16 pixel step difference per screen pixel.
    // Smooth zoom changes less than this between scanlines; overlay seams jump more.
    const int32 DEFORM_DISC_THRESH = 0x1000;
    // 0x30000 (16.16 fixed) = 3.0 source pixels. Normal vertical scroll advances
    // ~1 pixel per scanline; a 3+ pixel jump means an overlay boundary.
    const int32 POSY_DISC_THRESH   = 0x30000;
    for (int32 cy = clipY1 + 1; cy < clipY2; cy++) {
        int32 dxDiff  = scanlines[cy].deform.x - scanlines[cy - 1].deform.x;
        int32 pyDiff  = scanlines[cy].position.y - scanlines[cy - 1].position.y;
        if (dxDiff < 0) dxDiff = -dxDiff;
        if (pyDiff < 0) pyDiff = -pyDiff;
        if (dxDiff > DEFORM_DISC_THRESH || pyDiff > POSY_DISC_THRESH) {
            if (bandEdgeCount < 14) // 16 - 2: reserve slots for initial clipY1 and final clipY2
                bandEdges[bandEdgeCount++] = cy;
        }
    }
    bandEdges[bandEdgeCount++] = clipY2;

    // Subdivide each segment into uniform bands
    // balance quad count vs interpolation accuracy
    // 48 slots: variable-deform segments only (overlays take fast path)
    int32 allBands[48];
    int32 allBandCount = 0;

    for (int32 seg = 0; seg < bandEdgeCount - 1; seg++) {
        int32 segTop    = bandEdges[seg];
        int32 segBot    = bandEdges[seg + 1];
        int32 segHeight = segBot - segTop;
        if (segHeight <= 0)
            continue;

        // Fast path: deform.x == 0x10000 with constant posX (AddDynamicBG overlays on FBZ1)
        // 1:1 pixel mapping — tiles are axis-aligned screen rectangles
        // Eliminates shz_invf(), band subdivision, and per-tile affine math
        if (scanlines[segTop].deform.x == 0x10000 && scanlines[segBot - 1].deform.x == 0x10000
            && scanlines[segTop].position.x == scanlines[segBot - 1].position.x) {
            const int32 posX      = scanlines[segTop].position.x;
            const int32 posYStart = scanlines[segTop].position.y;
            const float screenOffX = (float)posX * (1.0f / 65536.0f);

            const float recipTF = 1.0f / (float)(1 << 20);
            float uStart        = (float)posX;
            float uEnd          = uStart + fLineSize * 65536.0f;
            int32 txStart       = (int32)floorf(uStart * recipTF);
            int32 txEnd         = (int32)floorf(uEnd * recipTF);

            int32 scanY = segTop;
            while (scanY < segBot) {
                int32 curPosY   = posYStart + ((scanY - segTop) << 16);
                int32 tyTop     = (curPosY >> 20) & tyMask;
                int32 vFracTop  = (curPosY >> 16) & 0xF;
                int32 subBot    = scanY + (TILE_SIZE - vFracTop);
                if (subBot > segBot)
                    subBot = segBot;

                int32 subPosYBot  = posYStart + ((subBot - 1 - segTop) << 16);
                int32 vFracBotEnd = (subBot < segBot) ? TILE_SIZE : (((subPosYBot >> 16) & 0xF) + 1);

                float fSubTop = (float)scanY;
                float fSubBot = (float)subBot;

                for (int32 tx = txStart; tx <= txEnd; tx++) {
                    int32 wrappedTx = tx & txMask;
                    uint16 tileVal  = layout[wrappedTx + (tyTop << widthShift)];
                    if (tileVal == 0xFFFF)
                        continue;

                    uint16 rawTile = tileVal & 0xFFF;
                    int32 flipBits = rawTile / TILE_COUNT;
                    uint16 tileIdx = rawTile % TILE_COUNT;

                    float leftX  = (float)(tx * TILE_SIZE) - screenOffX;
                    float rightX = leftX + (float)TILE_SIZE;
                    if (leftX >= fLineSize || rightX < 0.0f)
                        continue;

                    int32 u0, u1, v0, v1;
                    const GFXSurface *tileSurface = TileAtlasUV(
                        tileIdx,
                        flipBits & FLIP_X, flipBits & FLIP_Y,
                        u0, u1, v0, v1,
                        atlasSurface);

                    Vector4f ul = {  leftX, fSubTop, baseZ, 0.0f };
                    Vector4f ur = { rightX, fSubTop, baseZ, 0.0f };
                    Vector4f ll = {  leftX, fSubBot, baseZ, 0.0f };
                    Vector4f lr = { rightX, fSubBot, baseZ, 0.0f };

                    RenderDevice::DrawFloorTexturedPolyPTEx(
                        ul, ur, ll, lr,
                        u0, u1, v0, v1,
                        tileSurface, 0xFFFFFFFF, 0x00000000
                    );
                }
                scanY = subBot;
            }
            continue;
        } // end DynamicBg fast path

        int32 numBands = (segHeight + 31) / 32;
        if (numBands < 1)
            numBands = 1;
        for (int32 b = 0; b < numBands; b++) {
            int32 bTop = segTop + (segHeight * b) / numBands;
            int32 bBot = segTop + (segHeight * (b + 1)) / numBands;
            if (bBot > segBot) bBot = segBot;
            if (bTop >= bBot) continue;
            if (allBandCount < 46) { // 48 - 2: room for one more top+bot pair
                allBands[allBandCount++] = bTop;
                allBands[allBandCount++] = bBot;
            }
        }
    }

    for (int32 bi = 0; bi < allBandCount; bi += 2) {
        const int32 bandTop = allBands[bi];
        const int32 bandBot = allBands[bi + 1];

        int32 subTop = bandTop;
        while (subTop < bandBot) {
            ScanlineInfo *slTop = &scanlines[subTop];
            int32 posYTop       = slTop->position.y;
            int32 tyTop         = (posYTop >> 20) & tyMask; // >> 20 = >> 16 (fixed-to-pixel) >> 4 (pixel-to-tile)

            // find end of sub-band: where tile row changes or band ends
            int32 subBot = bandBot;
            for (int32 cy = subTop + 1; cy < bandBot; cy++) {
                int32 ty = (scanlines[cy].position.y >> 20) & tyMask;
                if (ty != tyTop) {
                    subBot = cy;
                    break;
                }
            }

            ScanlineInfo *slBot = &scanlines[subBot - 1];
            int32 deformTopI    = slTop->deform.x;
            int32 deformBotI    = slBot->deform.x;
            if (deformTopI == 0 || deformBotI == 0) {
                subTop = subBot;
                continue;
            }

            float fDeformTop = (float)deformTopI;
            float fDeformBot = (float)deformBotI;
            float xPositionTop   = (float)slTop->position.x;
            float xPositionBottom   = (float)slBot->position.x;

            // V fractional offset within the tile for UV mapping.
            // >> 16 = 16.16 fixed to integer pixels; & 0xF = pixel offset within 16px tile (0 to 15).
            int32 vFracTop    = (posYTop >> 16) & 0xF;
            int32 posYBotVal  = slBot->position.y;
            // If sub-band ends at a tile row boundary, use full TILE_SIZE; otherwise use actual pixel + 1
            int32 vFracBotEnd = (subBot < bandBot) ? TILE_SIZE : (((posYBotVal >> 16) & 0xF) + 1);

            // visible source U range across both top and bottom scanlines
            float uStartTop = xPositionTop;
            float uEndTop   = xPositionTop + fLineSize * fDeformTop;
            float uStartBot = xPositionBottom;
            float uEndBot   = xPositionBottom + fLineSize * fDeformBot;

            float uMin = uStartTop;
            if (uStartBot < uMin)
                uMin = uStartBot;
            if (uEndTop < uMin)
                uMin = uEndTop;
            if (uEndBot < uMin)
                uMin = uEndBot;

            float uMax = uStartTop;
            if (uStartBot > uMax)
                uMax = uStartBot;
            if (uEndTop > uMax)
                uMax = uEndTop;
            if (uEndBot > uMax)
                uMax = uEndBot;

            // tile column range: 1 << 20 = TILE_SIZE << 16, one tile in 16.16 source units
            const float recipTileFixed = 1.0f / (float)(1 << 20);
            int32 txStart = (int32)floorf(uMin * recipTileFixed);
            int32 txEnd   = (int32)floorf(uMax * recipTileFixed);

            float fSubTop = (float)subTop;
            float fSubBot = (float)subBot;
            float invDeformTop = shz_invf(fDeformTop);
            float invDeformBot = shz_invf(fDeformBot);

            for (int32 tx = txStart; tx <= txEnd; tx++) {
                int32 wrappedTx = tx & txMask;
                uint16 tileVal  = layout[wrappedTx + (tyTop << widthShift)];
                if (tileVal == 0xFFFF)
                    continue;

                uint16 rawTile  = tileVal & 0xFFF; // lower 12 bits: tile index + flip flags
                int32 flipBits  = rawTile / TILE_COUNT; // upper bits encode FLIP_X / FLIP_Y
                uint16 tileIdx  = rawTile % TILE_COUNT; // actual tile index into atlas

                // Tile edges in 16.16 source space: 65536.0f (1 << 16) converts pixels to 16.16.
                // Then invert the scanline equation (sourceU = posX + screenX * deform)
                // to solve for screenX = (sourceU - posX) / deform at each tile edge.
                float srcLeft  = (float)(tx * TILE_SIZE) * 65536.0f;
                float srcRight = (float)((tx + 1) * TILE_SIZE) * 65536.0f;

                float ulX = (srcLeft - xPositionTop) * invDeformTop;
                float urX = (srcRight - xPositionTop) * invDeformTop;
                float llX = (srcLeft - xPositionBottom) * invDeformBot;
                float lrX = (srcRight - xPositionBottom) * invDeformBot;

                if (ulX >= fLineSize && urX >= fLineSize && llX >= fLineSize && lrX >= fLineSize)
                    continue;
                if (ulX < 0 && urX < 0 && llX < 0 && lrX < 0)
                    continue;

                // resolve tile to atlas UV (handling anitiles)
                const AniTileState *aniState = AniTileTracker::GetAniTile(tileIdx);
                const GFXSurface *tileSurface;
                float u0, u1, v0, v1;

                if (aniState != nullptr && aniState->sheetID != 0) {
                    tileSurface = &gfxSurface[aniState->sheetID];
                    u0 = (float)aniState->u;
                    u1 = (float)(aniState->u + TILE_SIZE);
                    v0 = (float)(aniState->v + vFracTop);
                    v1 = (float)(aniState->v + vFracBotEnd);
                } else {
                    tileSurface = atlasSurface;
                    int32 col   = tileIdx % KOS_ATLAS_WIDTH_TILES;
                    int32 row   = tileIdx / KOS_ATLAS_HEIGHT_TILES;
                    u0 = (float)(col * TILE_SIZE);
                    u1 = (float)(col * TILE_SIZE + TILE_SIZE);
                    v0 = (float)(row * TILE_SIZE + vFracTop);
                    v1 = (float)(row * TILE_SIZE + vFracBotEnd);
                }

                if (flipBits & FLIP_X) {
                    float tmp = u0;
                    u0 = u1;
                    u1 = tmp;
                }

                if (flipBits & FLIP_Y) {
                    float tmp = v0;
                    v0 = v1;
                    v1 = tmp;
                }

                if (tileSurface != preparedSurface) {
                    RenderDevice::PrepareTexturedPolyPTEX(subTop, INK_NONE, tileSurface);
                    preparedSurface = tileSurface;
                }

                Vector4f ul = { ulX, fSubTop, baseZ, 0.0f };
                Vector4f ur = { urX, fSubTop, baseZ, 0.0f };
                Vector4f ll = { llX, fSubBot, baseZ, 0.0f };
                Vector4f lr = { lrX, fSubBot, baseZ, 0.0f };

                RenderDevice::DrawFloorTexturedPolyPTEx(
                    ul, ur, ll, lr,
                    u0, u1, v0, v1,
                    tileSurface, 0xFFFFFFFF, 0x00000000
                );
            }

            subTop = subBot;
        }
    }
}

// Don't get overwhelmed by this, just solving a math problem.
//
// Find the screen-X positions where an affine scanline crosses a tile boundary.
//
// In Mania's rotozoom/affine renderer, each scanline maps screen pixels to source texels via:
//   source = position + screenX * deform
// where pos is the starting source coordinate and def is the per-pixel step.
// A tile boundary occurs every tsf source units (= TILE_SIZE << 16 in 16.16 fixed).
//
// We need these crossings because the tile atlas stores each tile separately —
// a single quad can only reference one tile's UVs. Splitting at boundaries lets
// us emit one quad per tile with correct atlas coordinates.
//
// pos:      source-space start of the scanline (16.16 fixed, as float)
// def:      source-space step per screen pixel (16.16 fixed, as float)
// lineSize: screen-space width in pixels
// bounds:   output array of screen-X positions where tile crossings occur
// count:    in/out index into bounds array
static void CollectTileBoundaries(float pos, float def, float lineSize, float *bounds, int32 &count)
{
    const int32 maxCount = 250;
    const float tileSize = (float)(TILE_SIZE << 16);

    // no movement along this axis — no crossings possible
    if (def == 0.0f)
        return;

    // invert the step to solve for screen-X: cx = (boundary - pos) / def
    float invDef = shz_invf(def);

    float invTileSize = shz_invf(tileSize);

    // source-space range covered by this scanline
    float src0 = pos;
    float src1 = pos + lineSize * def;
    float srcMin = src0 < src1 ? src0 : src1;
    float srcMax = src0 > src1 ? src0 : src1;

    // tile indices spanned: first tile boundary crossed (firstCrossIndex) to last (lastCrossIndex)
    int32 firstCrossIndex = (int32)floorf(srcMin * invTileSize) + 1;
    int32 lastCrossIndex = (int32)floorf(srcMax * invTileSize);

    for (int32 crossIndex = firstCrossIndex; crossIndex <= lastCrossIndex && count < maxCount; crossIndex++) {
        float cx = (((float)crossIndex * tileSize) - pos) * invDef;
        // skip crossings at the very edge
        if (cx > 0.5f && cx < lineSize - 0.5f)
            bounds[count++] = cx;
    }
}

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#include <algorithm>
#endif

// The game has tile-based layers that can be rotated and scaled.
// The software renderer draws these pixel-by-pixel:
// for each screen pixel, it computes source texel to sample.
// We want to draw textured quads with the PVR.
// Any and all of the 1024 tiles that could make up the rotozoom layer
// live in a single 512×512 atlas texture.
// Each tile is a 16×16 region in that atlas.
// Doing this scanline-by-scanline like the software renderer would take too many quads.
// You would have to split each line where it crosses into a new tile.
// If the layer was zoomed out far enough to have one tile per screen pixel,
// you would end up needing to submit more than 75k quads (320x240) to cover the screen
//
// The scanline data gives a linear mapping for each row of the screen.
// If you know where across the width of the screen.
// and where along the height of the screen, the mapping crosses from one tile into the next,
// you can split the screen into segments that each fall entirely within one tile.
// Each segment becomes one quad with UVs that point to the correct 16×16 region of the atlas.
//
// That's what is done here.
static void DrawLayerRotozoomAffine(TileLayer *layer)
{
    if (!layer->xsize || !layer->ysize)
        return;

    const int32 clipY1 = currentScreen->clipBound_Y1;
    const int32 clipY2 = currentScreen->clipBound_Y2;
    if (clipY1 >= clipY2)
        return;

    const int32 lineSize = currentScreen->clipBound_X2 - currentScreen->clipBound_X1;
    if (lineSize <= 0)
        return;

    // masks for wrapping tile coordinates
    const int32 widthMask  = (TILE_SIZE << layer->widthShift) - 1;
    const int32 heightMask = (TILE_SIZE << layer->heightShift) - 1;
    // tile width, height are 16 pixels
    // tilemap coord masks for wrapping tilemap coords
    const int32 txMask     = widthMask >> 4;
    const int32 tyMask     = heightMask >> 4;

    uint16 *layout    = layer->layout;
    const int32 widthShift = layer->widthShift;

    const GFXSurface *atlasSurface = &gfxSurface[0];
    const float baseZ              = RenderDevice::GetDepth();
    const float fLineSize          = (float)lineSize;

    RenderDevice::PrepareTexturedPolyPTEX(clipY1, INK_NONE, atlasSurface);
    const GFXSurface *preparedSurface = atlasSurface;

    // converts a 16.16 source offset within a tile to a 0..TILE_SIZE texel coordinate
    const float tileFracScale  = (float)TILE_SIZE / (float)(1 << 20);

    // a height of 8 scanline was chosen because it minimizes warping
    // when deform would significantly change endpoints over taller screen space
    const int32 bandHeight = 8;
    const int32 clipHeight = clipY2 - clipY1;

    // create a dynamic number of bands that scales with the overall height of the clip area
    const int32 numBands = (clipHeight + bandHeight - 1) / bandHeight;

    // process the screen in horizontal bands of 8 scanlines each
    for (int32 band = 0; band < numBands; band++) {
        int32 cyTop = clipY1 + (clipHeight * band) / numBands;
        int32 cyBot = clipY1 + (clipHeight * (band + 1)) / numBands;

        if (cyBot > clipY2)
            cyBot = clipY2;

        if (cyTop >= cyBot)
            continue;

        // affine parameters at the top and bottom edges of this band
        ScanlineInfo *slTop = &scanlines[cyTop];
        ScanlineInfo *slBot = &scanlines[cyBot - 1];

        float xPositionTop = (float)slTop->position.x;
        float yPositionTop = (float)slTop->position.y;
        float xPositionBottom = (float)slBot->position.x;
        float yPositionBottom = (float)slBot->position.y;

        float xDeformTop = (float)slTop->deform.x;
        float yDeformTop = (float)slTop->deform.y;
        float xDeformBottom = (float)slBot->deform.x;
        float yDeformBottom = (float)slBot->deform.y;

        float topClipY = (float)cyTop;
        float bottomClipY = (float)cyBot;

        // collect every screen-X where the affine mapping crosses a tile edge
        // (both U and V axes, both top and bottom scanlines of the band)
        float bounds[256];
        int32 nBounds = 0;
        bounds[nBounds++] = 0.0f;
        bounds[nBounds++] = fLineSize;

        CollectTileBoundaries(xPositionTop, xDeformTop, fLineSize, bounds, nBounds);
        CollectTileBoundaries(xPositionBottom, xDeformBottom, fLineSize, bounds, nBounds);

        CollectTileBoundaries(yPositionTop, yDeformTop, fLineSize, bounds, nBounds);
        CollectTileBoundaries(yPositionBottom, yDeformBottom, fLineSize, bounds, nBounds);

        // need to sort so you can walk the bounds from left to right
        std::sort(bounds, bounds + nBounds);

        // each pair of adjacent bounds is one segment guaranteed to stay within a single tile
        for (int32 seg = 0; seg < nBounds - 1; seg++) {
            float leftClipX = bounds[seg];
            float rightClipX = bounds[seg + 1];
            if (rightClipX - leftClipX < 0.5f)
                continue;

            // sample the tile at the segment midpoint (safely inside one tile)
            float midpointClipX = (leftClipX + rightClipX) * 0.5f;
            float src_uMid = xPositionTop + midpointClipX * xDeformTop;
            float src_vMid = yPositionTop + midpointClipX * yDeformTop;
            int32 txMid = ((int32)src_uMid >> 20) & txMask;
            int32 tyMid = ((int32)src_vMid >> 20) & tyMask;

            uint16 tileVal = layout[txMid + (tyMid << widthShift)];
            if (tileVal == 0xFFFF)
                continue;

            uint16 rawTile = tileVal & 0xFFF;
            int32 flipBits = rawTile / TILE_COUNT;
            uint16 tileIdx = rawTile % TILE_COUNT;

            // origin of this tile in source space (snapped to tile grid)
            float tileOriginU = (float)(((int32)src_uMid & 0xFFFFF));// >> 20) << 20);
            float tileOriginV = (float)(((int32)src_vMid & 0xFFFFF));//>> 20) << 20);

            // compute source coords at all 4 screen corners of this segment
            float src_uTL = xPositionTop    +  leftClipX * xDeformTop;
            float src_vTL = yPositionTop    +  leftClipX * yDeformTop;
            float src_uTR = xPositionTop    + rightClipX * xDeformTop;
            float src_vTR = yPositionTop    + rightClipX * yDeformTop;
            float src_uBL = xPositionBottom +  leftClipX * xDeformBottom;
            float src_vBL = yPositionBottom +  leftClipX * yDeformBottom;
            float src_uBR = xPositionBottom + rightClipX * xDeformBottom;
            float src_vBR = yPositionBottom + rightClipX * yDeformBottom;

            // convert source coords to 0..TILE_SIZE texel offsets within this tile
            float fracUTL = ((float)((int32)src_uTL - (int32)tileOriginU)) * tileFracScale;
            float fracVTL = ((float)((int32)src_vTL - (int32)tileOriginV)) * tileFracScale;
            float fracUTR = ((float)((int32)src_uTR - (int32)tileOriginU)) * tileFracScale;
            float fracVTR = ((float)((int32)src_vTR - (int32)tileOriginV)) * tileFracScale;
            float fracUBL = ((float)((int32)src_uBL - (int32)tileOriginU)) * tileFracScale;
            float fracVBL = ((float)((int32)src_vBL - (int32)tileOriginV)) * tileFracScale;
            float fracUBR = ((float)((int32)src_uBR - (int32)tileOriginU)) * tileFracScale;
            float fracVBR = ((float)((int32)src_vBR - (int32)tileOriginV)) * tileFracScale;

            // apply tile flip flags
            if (flipBits & FLIP_X) {
                fracUTL = (float)TILE_SIZE - fracUTL;
                fracUTR = (float)TILE_SIZE - fracUTR;
                fracUBL = (float)TILE_SIZE - fracUBL;
                fracUBR = (float)TILE_SIZE - fracUBR;
            }
            if (flipBits & FLIP_Y) {
                fracVTL = (float)TILE_SIZE - fracVTL;
                fracVTR = (float)TILE_SIZE - fracVTR;
                fracVBL = (float)TILE_SIZE - fracVBL;
                fracVBR = (float)TILE_SIZE - fracVBR;
            }

            // bottom scanline can drift slightly past tile bounds due to band interpolation
            const float uvMin = 0.0f;
            const float uvMax = (float)TILE_SIZE;
            fracUTL = fmaxf(uvMin, fminf(uvMax, fracUTL));
            fracVTL = fmaxf(uvMin, fminf(uvMax, fracVTL));
            fracUTR = fmaxf(uvMin, fminf(uvMax, fracUTR));
            fracVTR = fmaxf(uvMin, fminf(uvMax, fracVTR));
            fracUBL = fmaxf(uvMin, fminf(uvMax, fracUBL));
            fracVBL = fmaxf(uvMin, fminf(uvMax, fracVBL));
            fracUBR = fmaxf(uvMin, fminf(uvMax, fracUBR));
            fracVBR = fmaxf(uvMin, fminf(uvMax, fracVBR));

            // resolve animated tile overrides, get atlas base position
            const AniTileState *aniState = AniTileTracker::GetAniTile(tileIdx);
            const GFXSurface *tileSurface;
            float baseU, baseV;

            if (aniState != nullptr && aniState->sheetID != 0) {
                tileSurface = &gfxSurface[aniState->sheetID];
                baseU = (float)aniState->u;
                baseV = (float)aniState->v;
            }
            else {
                tileSurface = atlasSurface;
                baseU = (float)((tileIdx % KOS_ATLAS_WIDTH_TILES) * TILE_SIZE);
                baseV = (float)((tileIdx / KOS_ATLAS_HEIGHT_TILES) * TILE_SIZE);
            }

            // final per-vertex UVs: atlas base + fractional offset within tile
            float ulU = baseU + fracUTL;
            float ulV = baseV + fracVTL;
            float urU = baseU + fracUTR;
            float urV = baseV + fracVTR;
            float llU = baseU + fracUBL;
            float llV = baseV + fracVBL;
            float lrU = baseU + fracUBR;
            float lrV = baseV + fracVBR;

            if (tileSurface != preparedSurface) {
                RenderDevice::PrepareTexturedPolyPTEX(cyTop, INK_NONE, tileSurface);
                preparedSurface = tileSurface;
            }

            // quad corners in screen space (x = screen-X, y = scanline, z = depth)
            Vector4f ul = {  leftClipX,    topClipY, baseZ, 0.0f };
            Vector4f ur = { rightClipX,    topClipY, baseZ, 0.0f };
            Vector4f ll = {  leftClipX, bottomClipY, baseZ, 0.0f };
            Vector4f lr = { rightClipX, bottomClipY, baseZ, 0.0f };

            RenderDevice::DrawFloorTexturedPolyPTExUV(
                ul, ur, ll, lr,
                ulU, ulV, urU, urV,
                llU, llV, lrU, lrV,
                tileSurface, 0xFFFFFFFF, 0x00000000
            );
        }
    }
}

// macros for common code among the various 3d rotozoom renderers

#define XFORM_WTEST(extrawork) \
                int32 negw = 0; \
                for (int32 i = 0; i < 4; i++) { \
                    mat_trans_nodiv(tileVerts[i].x, tileVerts[i].y, tileVerts[i].z, tileVerts[i].w); \
                    if (tileVerts[i].w <= EPS) { \
                        negw = 1; \
                        break; \
                    } \
                } \
                /* if any were behind camera, move on to next tile */ \
                if (negw) { \
                    (extrawork); \
                    continue; \
                }

#define PERSPDIV() \
            for (int32 i = 0; i < 4; i++) { \
                float invW = shz_invf(tileVerts[i].w); \
                tileVerts[i].x *= invW; \
                tileVerts[i].y *= invW; \
                tileVerts[i].z *= invW; \
                tileVerts[i].z += baseZ; \
                tileVerts[i].w = invW; \
            }

#define CULL(dim,dir,val) \
            if (tileVerts[0].dim dir (val) && tileVerts[1].dim dir (val) && tileVerts[2].dim dir (val) && tileVerts[3].dim dir (val))  { \
                continue; \
            }

#define VERTS(x0,x1,y,z0,z1) \
            tileVerts[TILE_UL] = { (x0),  (y), (z0), 1.0f }; \
            tileVerts[TILE_UR] = { (x1),  (y), (z0), 1.0f }; \
            tileVerts[TILE_LL] = { (x0),  (y), (z1), 1.0f }; \
            tileVerts[TILE_LR] = { (x1),  (y), (z1), 1.0f };

#define VERTS_LCOPY(x1, y, z0, z1) \
            tileVerts[TILE_UL] = tileVerts[TILE_UR]; \
            tileVerts[TILE_LL] = tileVerts[TILE_LR]; \
            tileVerts[TILE_UR] = { (x1),  (y), (z0), 1.0f }; \
            tileVerts[TILE_LR] = { (x1),  (y), (z1), 1.0f };

// similar to the playfield renderer
// dervie a camera basis, use it to render rotating tilemap
static void DrawRotozoomIsland(TileLayer *layer)
{
    const GFXSurface *surface = &gfxSurface[0];
    uint16 *layout            = layer->layout;
    ScanlineInfo *scanline    = &scanlines[0];
    const int32 clipY1 = currentScreen->clipBound_Y1;
    const int32 clipY2 = currentScreen->clipBound_Y2;

    int32 sine   = scanline->position.x;
    int32 cosine = scanline->position.y;

    // values passed in scanline position are Sin256/Cos256
    // sinA = -sine/256, cosA = cosine/256
    // floats for the matrix
    float sinA = (float)(-sine) / 256.0f;
    float cosA = (float)(-cosine) / 256.0f;

    // parameters derived from the scanline equations:
    //   srcU = (sine*0x140000 - sx*cosine*0x2800)/i - 0xA000*sine + 0x2000000
    //   srcV = (cosine*0x140000 + sx*sine*0x2800)/i - 0xA000*cosine + 0x2000000
    // orbit radius = 160 source pixels, camera height = 40, focal length = 128
    const float orbitR  = 160.0f;
    const float camH    = 40.0f;
    const float focalL  = 128.0f;
    const float horizY  = 152.0f;
    const float centerX = 320.0f * 0.5f;

    // build combined view-projection-screenspace matrix
    // maps world (x, y, z, 1) -> (sx*w, sy*w, z_clip, w)
    // where world x/z = source pixels centered at map center, y = tile height
    //
    // after rotation by angle a:
    //   depth = orbitR - (sinA*x + cosA*z)    distance from camera ground pos
    //   lateral = -(cosA*x - sinA*z)          perpendicular to depth
    //
    // projection:
    //   screen_x = centerX + focalL * lateral / depth
    //   screen_y = horizY + focalL * (camH - y) / depth
    //
    // in homogeneous form with w = depth:
    //   sx*w = centerX*w + focalL*lateral
    //        = centerX*(orbitR - sinA*x - cosA*z) + focalL*(-(cosA*x - sinA*z))
    //   sy*w = horizY*w + focalL*(camH - y)
    //        = horizY*(orbitR - sinA*x - cosA*z) + focalL*camH - focalL*y
    //   w    = orbitR - sinA*x - cosA*z

    const float __attribute__((aligned(32))) islandMat[4][4] = {
        { -centerX * sinA + focalL * cosA, -horizY * sinA,                   -0.5f * sinA,   -sinA   },
        {  0.0f,                           -focalL,                           0.0f,           0.0f   },
        { -centerX * cosA - focalL * sinA, -horizY * cosA,                   -0.5f * cosA,   -cosA   },
        {  centerX * orbitR,                horizY * orbitR + focalL * camH,  0.5f * orbitR,  orbitR },
    };

    mat_load(&islandMat);

    const float baseZ = RenderDevice::GetDepth();
    const int32 tilesW = 1 << layer->widthShift;
    const int32 tilesH = 1 << layer->heightShift;
    const float halfW = (float)(tilesW) * 0.5f;
    const float halfH = (float)(tilesH) * 0.5f;

    RenderDevice::PrepareTexturedPolyPTEX(currentScreen->clipBound_Y1, INK_NONE, surface);

    for (int32 ty = 0; ty < tilesH; ty++) {
        for (int32 tx = 0; tx < tilesW; tx++) {
            uint16 tileVal = layout[tx + (ty << layer->widthShift)];
            if (tileVal == 0xFFFF)
                continue;

            int32 uf = (tileVal >> 10) & 1;
            int32 vf = (tileVal >> 11) & 1;
            uint16 tile = tileVal & 0x3FF;

            int32 atlasX = (tile % KOS_ATLAS_WIDTH_TILES) * TILE_SIZE;
            int32 atlasY = (tile / KOS_ATLAS_HEIGHT_TILES) * TILE_SIZE;

            int32 uoff0 = uf * TILE_SIZE;
            int32 uoff1 = (!uf) * TILE_SIZE;
            int32 voff0 = vf * TILE_SIZE;
            int32 voff1 = (!vf) * TILE_SIZE;

            // pull in the edges of each tile by one texel
            // avoid gaps in grid
            if (uoff0 < uoff1) {
                uoff0++;
                uoff1--;
            } else {
                uoff0--;
                uoff1++;
            }

            if (voff0 < voff1) {
                voff0++;
                voff1--;
            } else {
                voff0--;
                voff1++;
            }

            int32 u0 = atlasX + uoff0;
            int32 u1 = atlasX + uoff1;
            int32 v0 = atlasY + voff0;
            int32 v1 = atlasY + voff1;

            // tile world position in source pixels, centered at map center
            float wx = ((float)tx - halfW) * (float)TILE_SIZE;
            float wz = ((float)ty - halfH) * (float)TILE_SIZE;

            VERTS(wx, wx + (float)TILE_SIZE, 0.0f, wz, wz + (float)TILE_SIZE);

            XFORM_WTEST({});
            PERSPDIV();

            CULL(x, <, 0);
            CULL(x, >, 320);
            CULL(y, <, clipY1);
            CULL(y, >, clipY2);

            RenderDevice::DrawFloorTexturedPolyPTEx(
                tileVerts[TILE_UL], tileVerts[TILE_UR],
                tileVerts[TILE_LL], tileVerts[TILE_LR],
                u0, u1, v0, v1,
                surface, 0xFFFFFFFF, 0x00000000
            );
        }
    }
}

static void DrawRotozoom3DRoof(TileLayer *layer)
{
    if (!roofTextureReady) {
        // TODO: fall back to per-tile renderer for non-UFO4 stages
        return;
    }

    ScanlineInfo *scanline = &scanlines[0];

    int32 roofHeightRaw = scanlines[2].position.y;
    RotozoomCamera rc   = UnpackRotozoomCamera(scanline);
    float roofTileY     = (float)(-roofHeightRaw) * recip64k + rc.cam.y;

    LoadRotozoomMatrix(rc, rc.foa, rc.f, nullptr);

    const float baseZ = RenderDevice::GetDepth();
    const float PATTERN_SIZE = 128.0f;

    // Determine current animation frame from tile 712's UV on sheet 6
    const AniTileState *as = AniTileTracker::GetAniTile(712);
    if (!as || as->sheetID == 0)
        return;

    int32 frameIdx = (as->v / ROOF_TEX_SIZE) * 4 + (as->u / ROOF_TEX_SIZE);
    if (frameIdx < 0 || frameIdx >= ROOF_FRAME_COUNT)
        frameIdx = 0;

    const GFXSurface *roofSurf = &roofFrames[frameIdx];

    float py = 120.0f - (float)((currentScreen->clipBound_Y1 + currentScreen->clipBound_Y2) / 2);
    Vector4f dir = { rc.forward.x + rc.up.x * (py * recip200),
                     rc.forward.y + rc.up.y * (py * recip200),
                     rc.forward.z + rc.up.z * (py * recip200) };
    if (dir.y == 0.0f)
        dir.y = 0.001f;
    float t = (roofTileY - rc.cam.y) / dir.y;
    float originX = rc.cam.x + dir.x * t;
    float originZ = rc.cam.z + dir.z * t;

    const int32 originMX = (int32)floorf(originX / PATTERN_SIZE);
    const int32 originMZ = (int32)floorf(originZ / PATTERN_SIZE);
    const int32 metaRadius   = 5;
    const float fadeStart    = 0;//1.0f;
    const float recipFadeRange    = shz_invf((float)(metaRadius) - fadeStart);
    // Camera forward/right projected onto XZ plane for view-aligned fade
    float fwdX = rc.forward.x;
    float fwdZ = rc.forward.z;
    float fwdLen = sqrtf(fwdX * fwdX + fwdZ * fwdZ);
    if (fwdLen > 0.001f) {
        fwdX /= fwdLen;
        fwdZ /= fwdLen;
    }
    float rightX = fwdZ;
    float rightZ = -fwdX;

    RenderDevice::PrepareTexturedPolyTREX(currentScreen->clipBound_Y1, INK_ALPHA, roofSurf);

    for (int32 mz = originMZ - metaRadius; mz <= originMZ + metaRadius; mz++) {
        float z0 = (float)mz * PATTERN_SIZE;
        float z1 = z0 + PATTERN_SIZE;

        for (int32 mx = originMX - metaRadius; mx <= originMX + metaRadius; mx++) {
            float dx = ((float)mx + 0.5f) - originX / PATTERN_SIZE;
            float dz = ((float)mz + 0.5f) - originZ / PATTERN_SIZE;
            float viewFwd   = fabsf(dx * fwdX + dz * fwdZ);
            float viewRight = fabsf(dx * rightX + dz * rightZ);
            float dist = viewFwd > viewRight ? viewFwd : viewRight;

            float alpha = 1.0f;
            if (dist > fadeStart)
                alpha = (1.0f - (dist - fadeStart) * recipFadeRange) * 0.8f;
            if (alpha <= 0.0f)
                continue;
            uint32 a = (uint32)(alpha * 255.0f);
            uint32 color = (a << 24) | 0x00FFFFFF;

            float x0 = (float)mx * PATTERN_SIZE;
            float x1 = x0 + PATTERN_SIZE;

            VERTS(x0, x1, roofTileY, z0, z1);
            XFORM_WTEST({});
            for (int32 i = 0; i < 4; i++) {
                float invW = shz_invf(tileVerts[i].w);
                tileVerts[i].x *= invW;
                tileVerts[i].y *= invW;
                tileVerts[i].z = tileVerts[i].z * invW * 0.01f + baseZ;
                tileVerts[i].w = invW;
            }

            RenderDevice::DrawFloorTexturedPolyTREx(
                tileVerts[TILE_UL], tileVerts[TILE_UR],
                tileVerts[TILE_LL], tileVerts[TILE_LR],
                x0, x1, z0, z1,
                roofSurf, color, 0x00000000
            );
        }
    }
}

// simplified version of DrawRotozoomPlayfield
// only draws with a texture of the infinite plane tilemap instead of atlas
static void DrawRotozoom3DFloor(TileLayer *layer)
{
    ScanlineInfo *scanline = &scanlines[0];

    if (!floorTextureReady)
        return;

    RotozoomCamera rc = UnpackRotozoomCamera(scanline);
    LoadRotozoomMatrix(rc, rc.foa, rc.f, nullptr);

    const float baseZ = RenderDevice::GetDepth();
    const float PATTERN_W = (float)(floorTilesW * TILE_SIZE);
    const float PATTERN_H = (float)(floorTilesH * TILE_SIZE);
    const float texW = (float)floorSurf.width;
    const float texH = (float)floorSurf.height;

    // project screen center onto the y=0 ground plane to find the origin
    float py = 120.0f - (float)((currentScreen->clipBound_Y1 + currentScreen->clipBound_Y2) / 2);
    Vector4f dir = { rc.forward.x + rc.up.x * (py * recip200),
                     rc.forward.y + rc.up.y * (py * recip200),
                     rc.forward.z + rc.up.z * (py * recip200) };
    if (dir.y >= -0.001f)
        dir.y = -0.001f;
    float t = -rc.cam.y / dir.y;
    float originX = rc.cam.x + dir.x * t;
    float originZ = rc.cam.z + dir.z * t;

    const int32 originMX = (int32)floorf(originX / PATTERN_W);
    const int32 originMZ = (int32)floorf(originZ / PATTERN_H);
    const int32 metaRadius = 8;
    const float fadeStart  = 2.0f;
    const float fadeRange  = (float)(metaRadius) - fadeStart;

    // camera forward/right projected onto XZ plane for view-aligned fade
    float fwdX = rc.forward.x;
    float fwdZ = rc.forward.z;
    float fwdLen = sqrtf(fwdX * fwdX + fwdZ * fwdZ);
    if (fwdLen > 0.001f) {
        fwdX /= fwdLen;
        fwdZ /= fwdLen;
    }
    float rightX = fwdZ;
    float rightZ = -fwdX;

    const int32 gridSize = metaRadius * 2 + 1;
    const int32 gridVSize = gridSize + 1;
    Vector4f gridVerts[gridVSize][gridVSize];
    uint8 gridValid[gridVSize][gridVSize];

    int32 minMX = originMX - metaRadius;
    int32 minMZ = originMZ - metaRadius;

    for (int32 gz = 0; gz < gridVSize; gz++) {
        float wz = (float)(minMZ + gz) * PATTERN_H;
        for (int32 gx = 0; gx < gridVSize; gx++) {
            float wx = (float)(minMX + gx) * PATTERN_W;

            gridVerts[gz][gx] = { wx, 0.0f, wz, 1.0f };
            mat_trans_nodiv(gridVerts[gz][gx].x, gridVerts[gz][gx].y,
                            gridVerts[gz][gx].z, gridVerts[gz][gx].w);

            if (gridVerts[gz][gx].w <= EPS) {
                gridValid[gz][gx] = 0;
            } else {
                float invW = shz_invf(gridVerts[gz][gx].w);
                gridVerts[gz][gx].x *= invW;
                gridVerts[gz][gx].y *= invW;
                gridVerts[gz][gx].z = gridVerts[gz][gx].z * invW * 0.01f + baseZ;
                gridVerts[gz][gx].w = invW;
                gridValid[gz][gx] = 1;
            }
        }
    }

    RenderDevice::PrepareTexturedPolyTREX(currentScreen->clipBound_Y1, INK_ALPHA, &floorSurf);

    for (int32 gz = 0; gz < gridSize; gz++) {
        int32 mz = minMZ + gz;
        for (int32 gx = 0; gx < gridSize; gx++) {
            int32 mx = minMX + gx;

            if (!gridValid[gz][gx] || !gridValid[gz][gx + 1] ||
                !gridValid[gz + 1][gx] || !gridValid[gz + 1][gx + 1])
                continue;

            float dx = ((float)mx + 0.5f) - originX / PATTERN_W;
            float dz = ((float)mz + 0.5f) - originZ / PATTERN_H;
            float viewFwd   = fabsf(dx * fwdX + dz * fwdZ);
            float viewRight = fabsf(dx * rightX + dz * rightZ);
            float dist = viewFwd > viewRight ? viewFwd : viewRight;

            float alpha = 1.0f;
            if (dist > fadeStart)
                alpha = 1.0f - (dist - fadeStart) / fadeRange;
            if (alpha <= 0.0f)
                continue;
            uint32 a = (uint32)(alpha * 255.0f);
            uint32 color = (a << 24) | 0x00FFFFFF;

            RenderDevice::DrawFloorTexturedPolyTREx(
                gridVerts[gz][gx], gridVerts[gz][gx + 1],
                gridVerts[gz + 1][gx], gridVerts[gz + 1][gx + 1],
                0.0f, texW, 0.0f, texH,
                &floorSurf, color, 0x00000000
            );
        }
    }
}

// this is technically a 3d layer that uses a camera
// but the rotation of the layer is fixed
// the scrolling is based on time alone
static void DrawRotozoomPinballBG(TileLayer *layer)
{
    const GFXSurface *surface = &gfxSurface[0];
    uint16 *layout            = layer->layout;
    ScanlineInfo *scanline    = &scanlines[0];

    // parameters tweaked by eye, trying to get something that resembled sw version
    const float camHeight = 35.0f;
    const float roofY = camHeight + 18.0f;

    // see: SonicMania/Objects/Pinball/PBL_Setup.c
    // PBL_Setup_Scanline_PinballBG
    //    int32 sin     = RSDK.Sin256(32);
    //    int32 cos     = RSDK.Cos256(32);
    // fixed 45 degree angle
    const float sinA = 0.7071f;
    const float cosA = 0.7071f;

    // scroll position from timer: moves diagonally at 45 degrees
    int32 timer = scanline->position.x;
    float scrollU = (float)timer * recip64k;
    float scrollV = (float)timer * recip64k;

    Vector4f cam;
    cam.x = scrollU;
    cam.y = camHeight;
    cam.z = scrollV;

    // camera basis: yaw = 45 degrees, no pitch
    Vector4f forward = { sinA, 0.0f, cosA };
    Vector4f right  = { cosA, 0.0f, -sinA };
    Vector4f up     = { 0.0f, 1.0f, 0.0f };

    // wider horizontal FOV for horizontal stretch, tighter vertical for depth density
    const float fovy   = 47.5f * RSDK_PI / 180.0f;
    const float fVert  = 1.0f / tanf(fovy * 0.5f);
    const float fHoriz = fVert * 0.35f;

    RotozoomCamera rc;
    rc.cam = cam; rc.forward = forward; rc.right = right; rc.up = up;
    rc.drc = fipr(right.x,   right.y,   right.z,   0, cam.x, cam.y, cam.z, 0);
    rc.duc = fipr(up.x,      up.y,      up.z,      0, cam.x, cam.y, cam.z, 0);
    rc.dfc = fipr(forward.x, forward.y, forward.z, 0, cam.x, cam.y, cam.z, 0);

    float __attribute__((aligned(32))) finalmat[4][4];
    LoadRotozoomMatrix(rc, fHoriz, fVert, finalmat);

    const float baseZ = RenderDevice::GetDepth();
    const int32 tilesW = 1 << layer->widthShift;
    const int32 tilesH = 1 << layer->heightShift;

    // origin: ray through center of visible ceiling area
    float originX, originZ;
    float py = 120.0f - 56.0f;
    Vector4f dir = { forward.x + up.x * (py * recip200),
                     forward.y + up.y * (py * recip200),
                     forward.z + up.z * (py * recip200) };

    if (dir.y == 0.0f) {
        dir.y = 0.001f;
    }

    float t = (roofY - cam.y) / dir.y;
    originX = cam.x + dir.x * t;
    originZ = cam.z + dir.z * t;

    const int32 originTX = (int32)floorf(originX * recip16);
    const int32 originTZ = (int32)floorf(originZ * recip16);

    const int32 tileRadius = 15;
    const float tileRadSq  = (float)(tileRadius * tileRadius);
    const int32 wrapMaskX  = tilesW - 1;
    const int32 wrapMaskZ  = tilesH - 1;
    const float clipBot    = (float)currentScreen->clipBound_Y2;

    RenderDevice::PrepareTexturedPolyPTEX(currentScreen->clipBound_Y1, INK_NONE, surface);

    for (int32 tz = originTZ - tileRadius; tz <= originTZ + tileRadius; tz++) {
        float z0 = (float)(tz * TILE_SIZE);
        float z1 = z0 + (float)TILE_SIZE;
        float zdiff = (float)(originTZ - tz);
        bool cantReuse = true;

        for (int32 tx = originTX - tileRadius; tx <= originTX + tileRadius; tx++) {
            float xdiff = (float)(originTX - tx);
            // euclidean distance test: too far, move on to next tile
            if ((xdiff * xdiff + zdiff * zdiff) > tileRadSq) {
                cantReuse = true;
                continue;
            }

            int32 wrappedTx = tx & wrapMaskX;
            int32 wrappedTz = tz & wrapMaskZ;
            __builtin_prefetch(&layout[(wrappedTx + (wrappedTz << layer->widthShift)) + 16]);
            uint16 tileVal = layout[wrappedTx + (wrappedTz << layer->widthShift)];
            if (tileVal == 0xFFFF) {
                cantReuse = true; continue;
            }

            int32 uf = (tileVal >> 10) & 1;
            int32 vf = (tileVal >> 11) & 1;
            uint16 tile = tileVal & 0x3FF;

            float x0 = (float)(tx * TILE_SIZE);
            float x1 = x0 + (float)TILE_SIZE;

            // last iteration didn't prevent vertex reuse, attempt it
            if (!cantReuse) {
                // test the most lower right corner to see if it is behind camera
                // if it is, move on to next tile
                float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                   x1, roofY, z1, 1.0f);

                if (wTest <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // reuse verts to save transforms
                VERTS_LCOPY(x1, roofY, z0, z1);

                // now transform and test upper right
                // behind camera, move on to next tile
                mat_trans_nodiv(tileVerts[TILE_UR].x, tileVerts[TILE_UR].y, tileVerts[TILE_UR].z, tileVerts[TILE_UR].w);

                if (tileVerts[TILE_UR].w <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // fully transform lower right vert now
                mat_trans_nodiv(tileVerts[TILE_LR].x, tileVerts[TILE_LR].y, tileVerts[TILE_LR].z, tileVerts[TILE_LR].w);

                // perspective divide the new right-side verts

                float invWUR = shz_invf(tileVerts[TILE_UR].w);
                tileVerts[TILE_UR].x *= invWUR;
                tileVerts[TILE_UR].y *= invWUR;
                tileVerts[TILE_UR].z *= invWUR;
                tileVerts[TILE_UR].z += baseZ;
                tileVerts[TILE_UR].w = invWUR;

                float invWLR = shz_invf(tileVerts[TILE_LR].w);
                tileVerts[TILE_LR].x *= invWLR;
                tileVerts[TILE_LR].y *= invWLR;
                tileVerts[TILE_LR].z *= invWLR;
                tileVerts[TILE_LR].z += baseZ;
                tileVerts[TILE_LR].w = invWLR;
            } else {
                // need to make all new verts for this tile
                // start by testing upper right vert w
                // if it is behind camera, move on to next tile
                float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                   x1, roofY, z0, 1.0f);
                if (wTest <= EPS) {
                    cantReuse = true;
                    continue;
                }

                VERTS(x0, x1, roofY, z0, z1);

                XFORM_WTEST(cantReuse = true);
                PERSPDIV();
            }

            // so far, verts can be reused for next tile
            cantReuse = false;

            CULL(x, <, 0);
            CULL(x, >, 320);
            CULL(y, <, 0);
            CULL(y, >, clipBot);

            int32 u0, u1, v0, v1;
            TileAtlasUV(tile, uf, vf, u0, u1, v0, v1, surface);

            // distance fade: farther tiles (smaller 1/W) darken toward purple
            float invW = tileVerts[TILE_UR].w;

            // fade only near the horizon: full brightness above fadeStart, full fog below fadeEnd
            const float fadeStart = 0.008f;
            const float fadeEnd   = 0.0034f;
            float fadeMix = 0.0f;
            if (invW < fadeStart) {
                fadeMix = 1.0f;//1.0f - (invW - fadeEnd) / (fadeStart - fadeEnd);
                //if (fadeMix < 0.0f) fadeMix = 0.0f;
                //if (fadeMix > 1.0f) fadeMix = 1.0f;
            }

            // vertex color fades to black, offset color fades to purple
            uint8 vC = (uint8)((1.0f - fadeMix) * 0xFF);//EE);
            uint32 pblBgColor = 0xFF000000 | (vC << 16) | (vC << 8) | vC;
            uint8 oR = (uint8)(fadeMix * 45.0f);
            uint8 oG = (uint8)(fadeMix * 25.0f);
            uint8 oB = (uint8)(fadeMix * 75.0f);
            uint32 pblBgOColor = 0xFF000000 | (oR << 16) | (oG << 8) | oB;

            // DC_DESATURATE
            uint8 pblDesat = RenderDevice::GetPaletteDesaturation();
            if (pblDesat > 0) {
                pblBgColor = 0xFF000000 | DesaturateColor32(pblBgColor & 0x00FFFFFF, pblDesat);
                pblBgOColor = 0xFF000000 | DesaturateColor32(pblBgOColor & 0x00FFFFFF, pblDesat);
            }

            RenderDevice::DrawFloorTexturedPolyPTEx(
                tileVerts[TILE_UL], tileVerts[TILE_UR],
                tileVerts[TILE_LL], tileVerts[TILE_LR],
                u0, u1, v0, v1,
                surface, pblBgColor, pblBgOColor
            );
        }
    }
}

// Tiles with magenta pixels in the tileset should NOT have plasma drawn under them.
// this is a set of bitfields covering all 1024 tile numbers
// a bit set to 1 means magenta pixels exist in that tile
static const uint32 ufo7PlasmaSkip[32] = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0xFF700000, 0x87EAA930, 0x5C2AB499, 0xFC000011, 0xFFFFFFFF, 0x1FFFFFFF, 0x8080C1DC, 0xC0603803,
    0xE000BFE0, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
};

static void DrawRotozoomPlayfield(TileLayer *layer, bool pinball)
{
    const GFXSurface *surface = &gfxSurface[0];
    uint16 *layout            = layer->layout;
    int32 width               = (TILE_SIZE << layer->widthShift) - 1;
    int32 height              = (TILE_SIZE << layer->heightShift) - 1;
    ScanlineInfo *scanline    = &scanlines[0];

    RotozoomCamera rc = UnpackRotozoomCamera(scanline);

    float __attribute__((aligned(32))) finalmat[4][4];
    LoadRotozoomMatrix(rc, rc.foa, rc.f, finalmat);

    // aliases for readability in the large body below
    const Vector4f &cam     = rc.cam;
    const Vector4f &forward = rc.forward;
    const Vector4f &right   = rc.right;
    const Vector4f &up      = rc.up;
    const float sp          = rc.sp;

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
    if (sp == 0.0f && lodTextureReady && RenderDevice::GetPaletteDesaturation() == 0)  { // DC_DESATURATE: skip LOD when paused
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
                    gridVerts[gz][gx].z += baseZ - 0.0001f;
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
                if (((cx0 + cz0) < (tileRadius + LOD_RAD_OFFSET)) &&
                    ((cx1 + cz0) < (tileRadius + LOD_RAD_OFFSET)) &&
                    ((cx0 + cz1) < (tileRadius + LOD_RAD_OFFSET)) &&
                    ((cx1 + cz1) < (tileRadius + LOD_RAD_OFFSET))
                ) {
                    if (((cx0*cx0 + cz0*cz0) < tileRadSq + LOD_RAD_OFF_SQ) &&
                        ((cx1*cx1 + cz0*cz0) < tileRadSq + LOD_RAD_OFF_SQ) &&
                        ((cx0*cx0 + cz1*cz1) < tileRadSq + LOD_RAD_OFF_SQ) &&
                        ((cx1*cx1 + cz1*cz1) < tileRadSq + LOD_RAD_OFF_SQ))
                        continue;
                }

                Vector4f &ul = gridVerts[gz][gx];
                Vector4f &ur = gridVerts[gz][gx + 1];
                Vector4f &ll = gridVerts[gz + 1][gx];
                Vector4f &lr = gridVerts[gz + 1][gx + 1];

                // screen edge culling
                if (ul.x < 0 && ur.x < 0 && ll.x < 0 && lr.x < 0) continue;
                if (ul.x > 320 && ur.x > 320 && ll.x > 320 && lr.x > 320) continue;
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
                uint32 ocolor;
                uint32 color;
                if (ufoNum == 2 || ufoNum == 4 || ufoNum == 7) {
                    uint8 bright = 0xEE - comp;
                    color  = 0xFF000000 | (bright << 16) | (bright << 8) | bright;
                    ocolor = 0x00000000;
                } else {
                    color  = 0xFFEEEEEE;
                    ocolor = 0xFF000000 | (comp << 16) | (comp << 8) | comp;
                }
                RenderDevice::DrawFloorTexturedPolyPTEx(
                    ul, ur, ll, lr,
                    iu0, iu1, iv0, iv1,
                    &mapSurf, color, ocolor
                );
            }
        }
    }

    // "INK_MASKED" plasma pass on stage 7 / stage 1+
    if (ufoNum == 7 && !pinball) {
        uint16 plasmaSheetID = (uint16)scanlines[0].position.x;
        int32 plasmaTimer    = scanlines[0].position.y;

        if (plasmaSheetID > 0 && plasmaSheetID < SURFACE_COUNT) {
            const GFXSurface *plasmaSurface = &gfxSurface[plasmaSheetID];
            if (plasmaSurface->texture) {
                RenderDevice::PrepareTexturedPolyPTEX(currentScreen->clipBound_Y1, INK_NONE, plasmaSurface);

                float scrollU = 0.0f;
                float scrollV = (float)plasmaTimer * 1.0f;

                const float waveFreq   = 12.0f;
                const float waveAmp    = 48.0f;
                const float phaseScale = 2.0f * RSDK_PI / 256.0f;
                const float plasmaBaseZ = baseZ - 0.0001f;
                const int plasmaRadius = 33;
                const float plasmaRadSq = (float)(plasmaRadius * plasmaRadius);

                int pfirstZ = firstNonEmptyRow > minTileZ ? firstNonEmptyRow : minTileZ;
                int plastZ  = lastNonEmptyRow < maxTileZ ? lastNonEmptyRow : maxTileZ;

                float wavePhaseTop = (float)(((int)(pfirstZ * waveFreq) + plasmaTimer * 2) & 0xFF) * phaseScale;
                float waveOffTop   = sinf(wavePhaseTop) * waveAmp;

                for (int tz = pfirstZ; tz <= plastZ; tz++) {
                    float wavePhaseBot = (float)(((int)((tz + 1) * waveFreq) + plasmaTimer * 2) & 0xFF) * phaseScale;
                    float waveOffBot   = sinf(wavePhaseBot) * waveAmp;

                    float z0    = (float)((uint32)(tz * TILE_SIZE));
                    float z1    = z0 + TILE_SIZE;
                    float zdiff = (float)(originTZ - tz);

                    int firstX = (minTileX < firstNonemptyTileInRow[tz]) ? firstNonemptyTileInRow[tz] : minTileX;
                    int lastX  = (maxTileX > lastNonemptyTileInRow[tz]) ? lastNonemptyTileInRow[tz] : maxTileX;

                    bool cantReuse = true;
                    size_t layout_offset = firstX + (tz << layer->widthShift) - 1;

                    for (int tx = firstX; tx <= lastX; tx++) {
                        bool reuseRights = false;
                        layout_offset++;

                        float xdiff  = (float)(originTX - tx);
                        float xzdist = (xdiff * xdiff) + (zdiff * zdiff);
                        if (xzdist > plasmaRadSq) {
                            cantReuse = true;
                            continue;
                        }

                        uint16 tile = layout[layout_offset] & 0xFFF;
                        if (tile == 0xFFF) {
                            cantReuse = true;
                            continue;
                        }
                        if (tile < 1024 && (ufo7PlasmaSkip[tile >> 5] & (1u << (tile & 31)))) {
                            cantReuse = true;
                            continue;
                        }

                        if (!cantReuse)
                            reuseRights = true;
                        cantReuse = false;

                        float x0 = (float)((uint32)(tx * TILE_SIZE));
                        float x1 = x0 + TILE_SIZE;

                        __builtin_prefetch(&layout[layout_offset + 16]);

                        if (reuseRights) {
                            float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                                x1, 0.0f, z1, 1.0f);
                            if (wTest <= EPS) {
                                cantReuse = true;
                                continue;
                            }

                            VERTS_LCOPY(x1, 0.0f, z0, z1);

                            mat_trans_nodiv(tileVerts[TILE_UR].x, tileVerts[TILE_UR].y, tileVerts[TILE_UR].z, tileVerts[TILE_UR].w);
                            if (tileVerts[TILE_UR].w <= EPS) {
                                cantReuse = true;
                                continue;
                            }

                            mat_trans_nodiv(tileVerts[TILE_LR].x, tileVerts[TILE_LR].y, tileVerts[TILE_LR].z, tileVerts[TILE_LR].w);

                            float wUR = tileVerts[TILE_UR].w;
                            float wLR = tileVerts[TILE_LR].w;
                            tileVerts[TILE_UR].w = shz_invf(wUR);
                            tileVerts[TILE_LR].w = shz_invf(wLR);

                            tileVerts[TILE_UR].x *= tileVerts[TILE_UR].w;
                            tileVerts[TILE_UR].y *= tileVerts[TILE_UR].w;
                            tileVerts[TILE_UR].z *= tileVerts[TILE_UR].w;
                            tileVerts[TILE_UR].z += plasmaBaseZ;

                            tileVerts[TILE_LR].x *= tileVerts[TILE_LR].w;
                            tileVerts[TILE_LR].y *= tileVerts[TILE_LR].w;
                            tileVerts[TILE_LR].z *= tileVerts[TILE_LR].w;
                            tileVerts[TILE_LR].z += plasmaBaseZ;
                        } else {
                            float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                                x1, 0.0f, z0, 1.0f);
                            if (wTest < EPS) {
                                cantReuse = true;
                                continue;
                            }

                            VERTS(x0, x1, 0.0f, z0, z1);
                            XFORM_WTEST(cantReuse = true);

                            for (int32 i = 0; i < 4; i++) {
                                float invW    = shz_invf(tileVerts[i].w);
                                tileVerts[i].x *= invW;
                                tileVerts[i].y *= invW;
                                tileVerts[i].z *= invW;
                                tileVerts[i].z += plasmaBaseZ;
                                tileVerts[i].w = invW;
                            }
                        }

                        CULL(x, <, 0);
                        CULL(x, >, 320);
                        CULL(y, <, 0);
                        CULL(y, >, 240);

                        uint8 comp   = (uint8)((1.0f - ((tileVerts[TILE_UR].w * 250.0f) < 1.0f ? (tileVerts[TILE_UR].w * 250.0f) : 1.0f)) * 84.0f);
                        uint8 bright = 0xEE - comp;
                        uint32 color  = 0xFF000000 | (bright << 16) | (bright << 8) | bright;
                        uint32 ocolor = 0x00000000;

                        float baseU0 = (float)(tx * TILE_SIZE) + scrollU;
                        float baseU1 = baseU0 + TILE_SIZE;
                        float baseV0 = (float)(tz * TILE_SIZE) + scrollV;
                        float baseV1 = baseV0 + TILE_SIZE;

                        RenderDevice::DrawFloorTexturedPolyPTExUV(
                            tileVerts[TILE_UL], tileVerts[TILE_UR],
                            tileVerts[TILE_LL], tileVerts[TILE_LR],
                            baseU0 + waveOffTop, baseV0, baseU1 + waveOffTop, baseV0,
                            baseU0 + waveOffBot, baseV1, baseU1 + waveOffBot, baseV1,
                            plasmaSurface, color, ocolor
                        );
                    }

                    waveOffTop = waveOffBot;
                }
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

            // prefetch a new cache-line sized run of tiles to speed up later iterations of inner loop
            __builtin_prefetch(&layout[layout_offset + 16]);

            if (reuseRights) {
                // early out test
                // one FIPR and one branch can skip all of the work for this tile
                float wTest = fipr(finalmat[0][3], finalmat[1][3], finalmat[2][3], finalmat[3][3],
                                    x1, 0.0f, z1, 1.0f);
                // new lower-right vert would be very close to, or behind camera
                // skip the entire tile
                if (wTest <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // reuse previous iteration upper/lower right tiles
                // now create new upper/lower right tiles
                VERTS_LCOPY(x1, 0.0f, z0, z1);

                // transform new upper right
                mat_trans_nodiv(tileVerts[TILE_UR].x, tileVerts[TILE_UR].y, tileVerts[TILE_UR].z, tileVerts[TILE_UR].w);
                // new upper-right vert would be very close to, or behind camera
                // skip the entire tile
                if (tileVerts[TILE_UR].w <= EPS) {
                    cantReuse = true;
                    continue;
                }

                // transform new lower right
                mat_trans_nodiv(tileVerts[TILE_LR].x, tileVerts[TILE_LR].y, tileVerts[TILE_LR].z, tileVerts[TILE_LR].w);

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
                                    x1, 0.0f, z0, 1.0f);

                // upper-right vert would be very close to, or behind camera
                // skip the entire tile
                if (wTest < EPS) {
                    cantReuse = true;
                    continue;
                }

                // create all four tile verts
                VERTS(x0, x1, 0.0f, z0, z1);

                XFORM_WTEST(cantReuse = true);

                PERSPDIV();
            }

            CULL(x, <, 0);
            CULL(x, >, 320);
            CULL(y, <, 0);
            CULL(y, >, 240);

            // simulated floor distance shading, experimentally derived
            uint8 comp = (uint8)((1.0f - ((tileVerts[TILE_UR].w*250.0f) < 1.0f ? (tileVerts[TILE_UR].w*250.0f) : 1.0f)) * 84.0f);
            uint32 ocolor;
            uint32 color = 0xFFEEEEEE;
            if (!pinball) {
                if (ufoNum == 2 || ufoNum == 4 || ufoNum == 7) {
                    uint8 bright = 0xEE - comp;
                    color  = 0xFF000000 | (bright << 16) | (bright << 8) | bright;
                    ocolor = 0x00000000;
                } else {
                    ocolor = 0xFF000000 | (comp << 16) | (comp << 8) | comp;
                }
            } else {
                // no additive fade for pinball stage
                // instead, try to recreate the darkness fade at the upper end of table
                // applied to vertex color
                ocolor = 0x00000000;
                float invW = shz_invf(tileVerts[TILE_UR].w);
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
}

#endif

void RSDK::DrawLayerRotozoom(TileLayer *layer)
{
#if defined(KOS_HARDWARE_RENDERER)
    const GFXSurface *surface = &gfxSurface[0];
    ScanlineInfo *scanline    = &scanlines[0];

    // this covers Title clouds + ERZ clouds, FBZ1 background, MMZ1 far plane
    if ((uint32)scanline->deform.x != (uint32)SCANLINE_MAJOR_MAGIC_3DTILES) {
        // MMZ1 far plane
        if (isMMZ1) {
            DrawRotozoomMMZ1(layer);
        }
        // the rest
        DrawLayerRotozoom2D(layer);
        return;
    }

    uint32 minor = (uint32)scanline->deform.y;

    if (minor == (uint32)SCANLINE_MINOR_MAGIC_ISLAND) {
        DrawRotozoomIsland(layer);
        return;
    }

    if (minor == (uint32)SCANLINE_MINOR_MAGIC_ROOF) {
        DrawRotozoom3DRoof(layer);
        return;
    }

    if (minor == (uint32)SCANLINE_MINOR_MAGIC_UFO_FLOOR) {
        DrawRotozoom3DFloor(layer);
        return;
    }

    if (minor == (uint32)SCANLINE_MINOR_MAGIC_PBL_BG) {
        DrawRotozoomPinballBG(layer);
        return;
    }

    bool pinball = (minor == (uint32)SCANLINE_MINOR_MAGIC_PINBALL);
    if (!pinball && minor != (uint32)SCANLINE_MINOR_MAGIC_UFO) return;

    DrawRotozoomPlayfield(layer, pinball);
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
