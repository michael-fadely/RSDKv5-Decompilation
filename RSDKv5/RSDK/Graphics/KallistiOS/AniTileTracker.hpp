#pragma once
#include <RSDK/Core/RetroEngine.hpp>

namespace RSDK {
struct AniTileState {
    uint16 sheetID = 0;
    uint16 u = 0;
    uint16 v = 0;

    [[nodiscard]] bool IsTileAligned() const {
        return ((u | v) & 0xF) == 0;
    }
};

class AniTileTracker {
public:
    static void ResetAllTiles();
    static void UpdateAniTile(uint16 sheetID, uint16 tileIndex, uint16 u, uint16 v, uint16 width, uint16 height);
    static const AniTileState* GetAniTile(uint16 tileIndex);

private:
    static AniTileState m_State[TILE_COUNT];
};

// static
inline const AniTileState* AniTileTracker::GetAniTile(uint16 tileIndex) {
    if (tileIndex > TILE_COUNT) {
        return nullptr;
    }

    const auto* result = &m_State[tileIndex];

    if (result->sheetID == 0) {
        return nullptr;
    }

    return result;
}
}  // namespace RSDK
