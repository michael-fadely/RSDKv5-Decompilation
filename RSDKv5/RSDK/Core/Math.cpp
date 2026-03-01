#include "RSDK/Core/RetroEngine.hpp"
#include <math.h>

using namespace RSDK;

#if RETRO_PLATFORM == RETRO_KALLISTIOS && !RETRO_USE_ORIGINAL_CODE
//! Calculates 1.0f/sqrtf( \p x ), using a fast approximation.
__always_inline float shz_inverse_sqrtf(float x) {
    asm("fsrra %0" : "+f" (x));
    return x;
}

__always_inline float shz_invf(float x) {
    float rx = shz_inverse_sqrtf(x * x);
    return x < 0 ? -rx : rx;
}
#endif

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
int32 RSDK::sin1024LookupTable[0x400];
int32 RSDK::cos1024LookupTable[0x400];
int32 RSDK::tan1024LookupTable[0x400];
#endif
int32 RSDK::asin1024LookupTable[0x400];
int32 RSDK::acos1024LookupTable[0x400];

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
int32 RSDK::sin512LookupTable[0x200];
int32 RSDK::cos512LookupTable[0x200];
int32 RSDK::tan512LookupTable[0x200];
#endif
int32 RSDK::asin512LookupTable[0x200];
int32 RSDK::acos512LookupTable[0x200];

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
int32 RSDK::sin256LookupTable[0x100];
int32 RSDK::cos256LookupTable[0x100];
int32 RSDK::tan256LookupTable[0x100];
#endif
int32 RSDK::asin256LookupTable[0x100];
int32 RSDK::acos256LookupTable[0x100];

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
uint8 RSDK::arcTan256LookupTable[0x100 * 0x100];
#endif

uint32 RSDK::randSeed = 0;

void RSDK::ClearTrigLookupTables()
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    memset(sin256LookupTable, 0, sizeof(sin256LookupTable));
    memset(cos256LookupTable, 0, sizeof(cos256LookupTable));
    memset(tan256LookupTable, 0, sizeof(tan256LookupTable));
#endif
    memset(asin256LookupTable, 0, sizeof(asin256LookupTable));
    memset(acos256LookupTable, 0, sizeof(acos256LookupTable));
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    memset(sin512LookupTable, 0, sizeof(sin512LookupTable));
    memset(cos512LookupTable, 0, sizeof(cos512LookupTable));
    memset(tan512LookupTable, 0, sizeof(tan512LookupTable));
#endif
    memset(asin512LookupTable, 0, sizeof(asin512LookupTable));
    memset(acos512LookupTable, 0, sizeof(acos512LookupTable));
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    memset(sin1024LookupTable, 0, sizeof(sin1024LookupTable));
    memset(cos1024LookupTable, 0, sizeof(cos1024LookupTable));
    memset(tan1024LookupTable, 0, sizeof(tan1024LookupTable));
#endif
    memset(asin1024LookupTable, 0, sizeof(asin1024LookupTable));
    memset(acos1024LookupTable, 0, sizeof(acos1024LookupTable));
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    memset(arcTan256LookupTable, 0, sizeof(arcTan256LookupTable));
#endif
    randSeed = 0;
}

void RSDK::CalculateTrigAngles()
{
    srand((uint32)time(NULL));
    randSeed = rand();

    for (int32 i = 0; i < 0x400; ++i) {
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
        sin1024LookupTable[i]  = (int32)(sinf((i / 512.f) * RSDK_PI) * 1024.f);
        cos1024LookupTable[i]  = (int32)(cosf((i / 512.f) * RSDK_PI) * 1024.f);
        tan1024LookupTable[i]  = (int32)(tanf((i / 512.f) * RSDK_PI) * 1024.f);
#endif
        asin1024LookupTable[i] = (int32)((asinf(i / 1023.f) * 512.f) / RSDK_PI);
        acos1024LookupTable[i] = (int32)((acosf(i / 1023.f) * 512.f) / RSDK_PI);
    }

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    cos1024LookupTable[0x000] = 0x400;
    cos1024LookupTable[0x100] = 0;
    cos1024LookupTable[0x200] = -0x400;
    cos1024LookupTable[0x300] = 0;

    sin1024LookupTable[0x000] = 0;
    sin1024LookupTable[0x100] = 0x400;
    sin1024LookupTable[0x200] = 0;
    sin1024LookupTable[0x300] = -0x400;
#endif

    for (int32 i = 0; i < 0x200; ++i) {
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
        sin512LookupTable[i]  = (int32)(sinf((i / 256.f) * RSDK_PI) * 512.f);
        cos512LookupTable[i]  = (int32)(cosf((i / 256.f) * RSDK_PI) * 512.f);
        tan512LookupTable[i]  = (int32)(tanf((i / 256.f) * RSDK_PI) * 512.f);
#endif
        asin512LookupTable[i] = (int32)((asinf(i / 511.f) * 256.f) / RSDK_PI);
        acos512LookupTable[i] = (int32)((acosf(i / 511.f) * 256.f) / RSDK_PI);
    }

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    cos512LookupTable[0x00]  = 0x200;
    cos512LookupTable[0x80]  = 0;
    cos512LookupTable[0x100] = -0x200;
    cos512LookupTable[0x180] = 0;

    sin512LookupTable[0x00]  = 0;
    sin512LookupTable[0x80]  = 0x200;
    sin512LookupTable[0x100] = 0;
    sin512LookupTable[0x180] = -0x200;
#endif

    for (int32 i = 0; i < 0x100; i++) {
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
        sin256LookupTable[i]  = (int32)((sin512LookupTable[i * 2] >> 1));
        cos256LookupTable[i]  = (int32)((cos512LookupTable[i * 2] >> 1));
        tan256LookupTable[i]  = (int32)((tan512LookupTable[i * 2] >> 1));
#endif
        asin256LookupTable[i] = (int32)((asinf(i / 255.f) * 128.f) / RSDK_PI);
        acos256LookupTable[i] = (int32)((acosf(i / 255.f) * 128.f) / RSDK_PI);
    }

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    for (int32 y = 0; y < 0x100; ++y) {
        uint8 *arcTan = (uint8 *)&arcTan256LookupTable[y];

        for (int32 x = 0; x < 0x100; ++x) {
            // 40.743664 = 0x100 / (2 * M_PI) (roughly)
            *arcTan = (int32)(float)((float)atan2((float)y, x) * 40.743664f);
            arcTan += 0x100;
        }
    }
#endif
}

#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
uint8 RSDK::ArcTanLookup(int32 X, int32 Y)
{
    int32 x = abs(X);
    int32 y = abs(Y);

    if (x <= y) {
        while (y > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }
    else {
        while (x > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }
    if (X <= 0) {
        if (Y <= 0)
            return arcTan256LookupTable[(x << 8) + y] + 0x80;
        else
            return 0x80 - arcTan256LookupTable[(x << 8) + y];
    }
    else if (Y <= 0)
        return -arcTan256LookupTable[(x << 8) + y];
    else
        return arcTan256LookupTable[(x << 8) + y];
}
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS && !RETRO_USE_ORIGINAL_CODE
// atan2 approx that scales output by (0x100 / (2 * M_PI))
// before returning
// battle-tested in multiple DC ports
static inline float scaled_atan2f(const float y, const float x)
{
    if ((x == 0.0f) && (y == 0.0f))
        return 0.0f;

    float abs_y = fabsf(y);
	float absy_plus_absx = abs_y + fabsf(x);
	float inv_absy_plus_absx = shz_invf(absy_plus_absx);
	float angle = 1.57079633f - copysignf(0.78539816f, x);
	float r = (x - copysignf(abs_y, x)) * inv_absy_plus_absx;
	angle += (0.1963f * r * r - 0.9817f) * r;

    return copysignf(angle, y) * 40.743664f;
}

uint8 RSDK::ArcTanLookup(int32 X, int32 Y)
{
    int32 x = abs(X);
    int32 y = abs(Y);

    if (x <= y) {
        while (y > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }
    else {
        while (x > 0xFF) {
            x >>= 4;
            y >>= 4;
        }
    }

    if (X <= 0) {
        if (Y <= 0)
            return (uint8)(int32)(float)(scaled_atan2f((float)y, (float)x)) + 0x80;
        else
            return 0x80 - (uint8)(int32)(float)(scaled_atan2f((float)y, (float)x));
    }
    else if (Y <= 0)
        return -(uint8)(int32)(float)(scaled_atan2f((float)y, (float)x));
    else
        return (uint8)(int32)(float)(scaled_atan2f((float)y, (float)x));
}
#endif