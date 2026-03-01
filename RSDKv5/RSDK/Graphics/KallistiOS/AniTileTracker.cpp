#include "AniTileTracker.hpp"

using namespace RSDK;

AniTileState AniTileTracker::m_State[TILE_COUNT] {};

// static
void AniTileTracker::ResetAllTiles() {
    // could be more efficient
    for (auto& state : m_State) {
        state = {};
    }
}

// static
void AniTileTracker::UpdateAniTile(uint16 sheetID, uint16 tileIndex, uint16 u, uint16 v, uint16 width, uint16 height) {
    if (sheetID >= SURFACE_COUNT || tileIndex >= TILE_COUNT) {
        return;
    }

    if ((width % TILE_SIZE) || (height % TILE_SIZE)) {
        // RUH ROH!!!
        return;
    }

    for (uint32 iy = 0; iy < height; iy += TILE_SIZE) {
        uint16 iu = u;

        for (uint32 ix = 0; ix < width; ix += TILE_SIZE) {
            assert(tileIndex < TILE_COUNT);
            auto* state = &m_State[tileIndex++];

            state->sheetID = sheetID;
            state->u = iu;
            state->v = v;

            iu += TILE_SIZE;
        }

        v += TILE_SIZE;
    }
}
