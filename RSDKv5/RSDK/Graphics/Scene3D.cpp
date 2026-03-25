#include "RSDK/Core/RetroEngine.hpp"

using namespace RSDK;

#if RETRO_REV0U
#include "Legacy/Scene3DLegacy.cpp"
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS
#include <algorithm>
#define recip256 0.00390625f

// used in conjunction with specular LUT table later
// 0.0003345f; // ~= 31 / (65536*sqrt(2))
// divided by 256 bc of the fixed point matrix we dot with
#define NORMAL_VAL_SCALE 0.00000131f
// ((1.0 / 0.0003345) / 1024) / 256
#define DIFFUSE_SCALE 0.01140418f

/*! 2D Vector type
 *
 *  Structure for holding coordinates of a 2-dimensional vector.
 *
 * \sa shz_vec3_t, shz_vec4_t
 */
typedef struct shz_vec2 {
    union {
        float e[2]; //!< <X, Y> coordinates as an array
        struct {
            float x; //!< X coordinate
            float y; //!< Y coordinate
        };
    };
} shz_vec2_t;

/*! 3D Vector type
 *
 *  Structure for holding coordinates of a 3-dimensional vector.
 *
 * \sa shz_vec2_t, shz_vec4_t
 */
typedef struct shz_vec3 {
    union {
        float e[3]; //!< <X, Y, Z> coordinates as an array
        struct {
            union {
                struct {
                    float x; //!< X coordinate
                    float y; //!< Y coordinate
                };
                shz_vec2_t xy; //!< Inner 2D vector containing <X, Y> coords
            };
            float z; //!< Z coordinate
        };
    };
} shz_vec3_t;

/*! 4D Vector type
 *
 *  Structure for holding coordinates of a 4-dimensional vector.
 *
 *  \sa shz_vec2_t, shz_vec3_t
 */
typedef struct shz_vec4 {
    union {
        float e[4]; //!< <X, Y, Z, W> coordinates as an array.
        struct {
            union {
                struct {
                    float x; //!< X coordinate
                    float y; //!< Y coordinate
                    float z; //!< Z coordinate
                };
                shz_vec3_t xyz; //!< <X, Y, Z> coordinates as a 3D vector
            };
            float w; //!< W coordinate
        };
        struct {
            shz_vec2_t xy; //!< <X, Y> coordinates as a 2D vector
            shz_vec2_t zw; //!< <Z, W> coordinates as a 2D vector
        };
    };
} shz_vec4_t;
typedef __attribute__((aligned(8))) union shz_matrix_4x4 {
    float elem[16];
    float elem2D[4][4];
    shz_vec4_t col[4];
    struct {
        shz_vec4_t left;
        shz_vec4_t up;
        shz_vec4_t forward;
        shz_vec4_t pos;
    };
} shz_matrix_4x4_t;

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

__always_inline void shz_xmtrx_load_4x4(const shz_matrix_4x4_t* matrix) {
    asm volatile(R"(
        fschg
        fmov.d	@%[mtx], xd0
        add     #32, %[mtx]
        pref	@%[mtx]
        add     #-(32-8), %[mtx]
        fmov.d	@%[mtx]+, xd2
        fmov.d	@%[mtx]+, xd4
        fmov.d	@%[mtx]+, xd6
        fmov.d	@%[mtx]+, xd8
        fmov.d	@%[mtx]+, xd10
        fmov.d	@%[mtx]+, xd12
        fmov.d	@%[mtx]+, xd14
        fschg
    )"
                 : [mtx] "+r"(matrix)
                 : "m"(*matrix));
}

__always_inline void shz_xmtrx_load_apply_store_4x4(shz_matrix_4x4_t* out, const shz_matrix_4x4_t* matrix1,
                                               const shz_matrix_4x4_t* matrix2) {
    unsigned int prefetch_scratch;

    asm volatile(R"(
        mov     %[m1], %[prefscr]
        add     #32, %[prefscr]
        fschg
        pref    @%[prefscr]

        fmov.d  @%[m1]+, xd0
        fmov.d  @%[m1]+, xd2
        fmov.d  @%[m1]+, xd4
        fmov.d  @%[m1]+, xd6
        pref    @%[m1]
        fmov.d  @%[m1]+, xd8
        fmov.d  @%[m1]+, xd10
        fmov.d  @%[m1]+, xd12
        mov     %[m2], %[prefscr]
        add     #32, %[prefscr]
        fmov.d  @%[m1], xd14
        pref    @%[prefscr]

        fmov.d  @%[m2]+, dr0
        fmov.d  @%[m2]+, dr2
        fmov.d  @%[m2]+, dr4
        ftrv    xmtrx, fv0

        fmov.d  @%[m2]+, dr6
        fmov.d  @%[m2]+, dr8
        ftrv    xmtrx, fv4

        fmov.d  @%[m2]+, dr10
        fmov.d  @%[m2]+, dr12
        ftrv    xmtrx, fv8

        add     #16, %[out]
        fmov.d  dr2, @-%[out]
        fmov.d  dr0,  @-%[out]

        fmov.d  @%[m2], dr14
        ftrv    xmtrx, fv12

        add     #32, %[out]
        fmov.d  dr6, @-%[out]
        fmov.d  dr4, @-%[out]

        add     #32, %[out]
        fmov.d  dr10, @-%[out]
        fmov.d  dr8, @-%[out]

        add     #32, %[out]
        fmov.d  dr14, @-%[out]
        fmov.d  dr12, @-%[out]

        fschg
    )"
                 : [m1] "+&r"(matrix1), [m2] "+r"(matrix2), [out] "+&r"(out),
                   "=m"(*out), [prefscr] "=&r"(prefetch_scratch)
                 : "m"(*matrix1), "m"(*matrix2)
                 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
                   "fr14", "fr15");
}
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS
Model __attribute__((aligned(32))) RSDK::modelList[MODEL_COUNT];
Scene3D __attribute__((aligned(32))) RSDK::scene3DList[SCENE3D_COUNT];
ScanEdge __attribute__((aligned(32))) RSDK::scanEdgeBuffer[SCREEN_YSIZE * 2];
#else
Model RSDK::modelList[MODEL_COUNT];
Scene3D RSDK::scene3DList[SCENE3D_COUNT];
ScanEdge RSDK::scanEdgeBuffer[SCREEN_YSIZE * 2];
#endif

enum ModelFlags {
    MODEL_NOFLAGS     = 0,
    MODEL_USENORMALS  = 1 << 0,
    MODEL_USETEXTURES = 1 << 1,
    MODEL_USECOLOURS  = 1 << 2,
#if RETRO_PLATFORM == RETRO_KALLISTIOS
    MODEL_ISSTRIPPED  = 1 << 3,
    MODEL_ISBAKED     = 1 << 4,
#endif
};

#if RETRO_PLATFORM == RETRO_KALLISTIOS
static StripTransformedVert __attribute__((aligned(32))) stripXformVerts[SCENE3D_VERT_COUNT];

struct StripItem {
    Model *model;
    Scene3D *scene;
    uint8 drawMode;
    uint16 vertCount;
    StripTransformedVert *verts; // points INTO stripXformVerts
};

static StripItem stripItems[16];
static int stripItemCount = 0;
static int stripVertOffset = 0; // running offset into stripXformVerts
#endif

void RSDK::ProcessScanEdge(int32 x1, int32 y1, int32 x2, int32 y2)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    int32 ix1 = FROM_FIXED(x1);
    int32 iy1 = FROM_FIXED(y1);
    int32 ix2 = FROM_FIXED(x2);
    int32 iy2 = FROM_FIXED(y2);

    int32 top = FROM_FIXED(y1);
    if (iy1 != iy2) {
        if (iy1 > iy2) {
            top = FROM_FIXED(y2);
            ix1 = FROM_FIXED(x2);
            ix2 = FROM_FIXED(x1);
            iy1 = FROM_FIXED(y2);
            iy2 = FROM_FIXED(y1);
        }

        int32 bottom = iy2 + 1;
        if (top < currentScreen->clipBound_Y2 && bottom >= currentScreen->clipBound_Y1) {
            if (bottom > currentScreen->clipBound_Y2)
                bottom = currentScreen->clipBound_Y2;
            int32 scanPos = TO_FIXED(ix1);
            int32 delta   = TO_FIXED(ix2 - ix1) / (iy2 - iy1);
            if (top < 0) {
                scanPos -= top * delta;
                top = 0;
            }

            ScanEdge *edge = &scanEdgeBuffer[top];
            for (int32 i = top; i < bottom; ++i) {
                int32 scanX = scanPos >> 16;
                if (scanX < edge->start)
                    edge->start = scanX;
                if (scanX > edge->end)
                    edge->end = scanX;
                scanPos += delta;
                ++edge;
            }
        }
    }
#endif
}

void RSDK::ProcessScanEdgeClr(uint32 c1, uint32 c2, int32 x1, int32 y1, int32 x2, int32 y2)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    int32 iy1 = FROM_FIXED(y1);
    int32 iy2 = FROM_FIXED(y2);
    int32 ix1 = FROM_FIXED(x1);
    int32 ix2 = FROM_FIXED(x2);

    int32 top     = FROM_FIXED(y1);
    uint32 color1 = c1;
    uint32 color2 = c2;
    if (iy1 != iy2) {
        if (iy1 > iy2) {
            top    = FROM_FIXED(y2);
            ix1    = FROM_FIXED(x2);
            ix2    = FROM_FIXED(x1);
            iy1    = FROM_FIXED(y2);
            iy2    = FROM_FIXED(y1);
            color1 = c2;
            color2 = c1;
        }

        int32 bottom = iy2 + 1;
        if (top < currentScreen->clipBound_Y2 && bottom >= currentScreen->clipBound_Y1) {
            if (bottom > currentScreen->clipBound_Y2)
                bottom = currentScreen->clipBound_Y2;

            int32 size   = iy2 - iy1;
            int32 scanX  = TO_FIXED(ix1);
            int32 deltaX = TO_FIXED(ix2 - ix1) / size;

            int32 c1R   = (color1 & 0xFF0000);
            int32 c2R   = (color2 & 0xFF0000);
            int32 scanR = c1R;

            int32 deltaR = 0;
            if (c1R != c2R)
                deltaR = (c2R - c1R) / size;

            int32 c1G   = (color1 & 0x00FF00) << 8;
            int32 c2G   = (color2 & 0x00FF00) << 8;
            int32 scanG = c1G;

            int32 deltaG = 0;
            if (c1G != c2G)
                deltaG = (c2G - c1G) / size;

            int32 c1B   = (color1 & 0x0000FF) << 16;
            int32 c2B   = (color2 & 0x0000FF) << 16;
            int32 scanB = c1B;

            int32 deltaB = 0;
            if (c1B != c2B)
                deltaB = (c2B - c1B) / size;

            if (top < 0) {
                scanX -= top * deltaX;

                scanR -= top * deltaR;
                scanG -= top * deltaG;
                scanB -= top * deltaB;

                top = 0;
            }

            ScanEdge *edge = &scanEdgeBuffer[top];
            for (int32 i = top; i < bottom; ++i) {
                if (FROM_FIXED(scanX) < edge->start) {
                    edge->start = FROM_FIXED(scanX);

                    edge->startR = scanR;
                    edge->startG = scanG;
                    edge->startB = scanB;
                }

                if (FROM_FIXED(scanX) > edge->end) {
                    edge->end = FROM_FIXED(scanX);

                    edge->endR = scanR;
                    edge->endG = scanG;
                    edge->endB = scanB;
                }

                scanX += deltaX;

                scanR += deltaR;
                scanG += deltaG;
                scanB += deltaB;

                ++edge;
            }
        }
    }
#endif
}

void RSDK::SetIdentityMatrix(Matrix *matrix)
{
    matrix->values[0][0] = 0x100;
    matrix->values[1][0] = 0;
    matrix->values[2][0] = 0;
    matrix->values[3][0] = 0;
    matrix->values[0][1] = 0;
    matrix->values[1][1] = 0x100;
    matrix->values[2][1] = 0;
    matrix->values[3][1] = 0;
    matrix->values[0][2] = 0;
    matrix->values[1][2] = 0;
    matrix->values[2][2] = 0x100;
    matrix->values[3][2] = 0;
    matrix->values[0][3] = 0;
    matrix->values[1][3] = 0;
    matrix->values[2][3] = 0;
    matrix->values[3][3] = 0x100;
}

#if RETRO_PLATFORM != RETRO_KALLISTIOS
void RSDK::MatrixMultiply(Matrix *dest, Matrix *matrixA, Matrix *matrixB)
{
    int32 result[4][4];
    // Every element will be overwritten anyway, so memset is useless here.
    memset(result, 0, 4 * 4 * sizeof(int32));

    for (int32 i = 0; i < 0x10; ++i) {
        uint32 rowA        = i / 4;
        uint32 rowB        = i % 4;
        result[rowB][rowA] = (matrixA->values[3][rowA] * matrixB->values[rowB][3] >> 8) + (matrixA->values[2][rowA] * matrixB->values[rowB][2] >> 8)
                             + (matrixA->values[1][rowA] * matrixB->values[rowB][1] >> 8)
                             + (matrixA->values[0][rowA] * matrixB->values[rowB][0] >> 8);
    }

    for (int32 i = 0; i < 0x10; ++i) {
        uint32 rowA              = i / 4;
        uint32 rowB              = i % 4;
        dest->values[rowB][rowA] = result[rowB][rowA];
    }
}
#else
void RSDK::MatrixMultiply(Matrix *dest, Matrix *matrixA, Matrix *matrixB)
{
    float __attribute__((aligned(32))) xmtrxB[16];
    int32 *mtrxB = (int32 *)matrixB->values;
    for (int i=0;i<16;i++) {
        xmtrxB[i] = (float)(*mtrxB++);
    }

    float __attribute__((aligned(32))) xmtrxA[16];
    int32 *mtrxA = (int32 *)matrixA->values;
    for (int i=0;i<16;i++) {
        xmtrxA[i] = (float)(*mtrxA++);
    }

    float __attribute__((aligned(32))) xmtrxC[16];
    int32 *mtrxC = (int32 *)dest->values;
    shz_xmtrx_load_apply_store_4x4((shz_matrix_4x4_t*)xmtrxC, (const shz_matrix_4x4_t*)xmtrxA, (const shz_matrix_4x4_t*)xmtrxB);
    for (int i=0;i<16;i++) {
        *mtrxC++ = ((int32)xmtrxC[i]) >> 8;
    }
}
#endif
void RSDK::MatrixTranslateXYZ(Matrix *matrix, int32 x, int32 y, int32 z, bool32 setIdentity)
{
    if (setIdentity) {
        matrix->values[0][0] = 0x100;
        matrix->values[1][0] = 0;
        matrix->values[2][0] = 0;
        matrix->values[0][1] = 0;
        matrix->values[1][1] = 0x100;
        matrix->values[2][1] = 0;
        matrix->values[0][2] = 0;
        matrix->values[1][2] = 0;
        matrix->values[2][2] = 0x100;
        matrix->values[3][0] = 0;
        matrix->values[3][1] = 0;
        matrix->values[3][2] = 0;
        matrix->values[3][3] = 0x100;
    }

    matrix->values[0][3] = x >> 8;
    matrix->values[1][3] = y >> 8;
    matrix->values[2][3] = z >> 8;
}
void RSDK::MatrixScaleXYZ(Matrix *matrix, int32 scaleX, int32 scaleY, int32 scaleZ)
{
    matrix->values[0][0] = scaleX;
    matrix->values[1][0] = 0;
    matrix->values[2][0] = 0;
    matrix->values[3][0] = 0;
    matrix->values[0][1] = 0;
    matrix->values[1][1] = scaleY;
    matrix->values[2][1] = 0;
    matrix->values[3][1] = 0;
    matrix->values[0][2] = 0;
    matrix->values[1][2] = 0;
    matrix->values[2][2] = scaleZ;
    matrix->values[3][2] = 0;
    matrix->values[0][3] = 0;
    matrix->values[1][3] = 0;
    matrix->values[2][3] = 0;
    matrix->values[3][3] = 0x100;
}
void RSDK::MatrixRotateX(Matrix *matrix, int16 rotationX)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    int32 sine   = sin1024LookupTable[rotationX & 0x3FF] >> 2;
    int32 cosine = cos1024LookupTable[rotationX & 0x3FF] >> 2;
#else
    int32 sine   = Sin1024(rotationX) >> 2;
    int32 cosine = Cos1024(rotationX) >> 2;
#endif

    matrix->values[0][0] = 0x100;
    matrix->values[1][0] = 0;
    matrix->values[2][0] = 0;
    matrix->values[3][0] = 0;
    matrix->values[0][1] = 0;
    matrix->values[1][1] = cosine;
    matrix->values[2][1] = sine;
    matrix->values[3][1] = 0;
    matrix->values[0][2] = 0;
    matrix->values[1][2] = -sine;
    matrix->values[2][2] = cosine;
    matrix->values[3][2] = 0;
    matrix->values[0][3] = 0;
    matrix->values[1][3] = 0;
    matrix->values[2][3] = 0;
    matrix->values[3][3] = 0x100;
}
void RSDK::MatrixRotateY(Matrix *matrix, int16 rotationY)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    int32 sine           = sin1024LookupTable[rotationY & 0x3FF] >> 2;
    int32 cosine         = cos1024LookupTable[rotationY & 0x3FF] >> 2;
#else
    int32 sine           = Sin1024(rotationY) >> 2;
    int32 cosine         = Cos1024(rotationY) >> 2;
#endif
    matrix->values[0][0] = cosine;
    matrix->values[1][0] = 0;
    matrix->values[2][0] = sine;
    matrix->values[3][0] = 0;
    matrix->values[0][1] = 0;
    matrix->values[1][1] = 0x100;
    matrix->values[2][1] = 0;
    matrix->values[3][1] = 0;
    matrix->values[0][2] = -sine;
    matrix->values[1][2] = 0;
    matrix->values[2][2] = cosine;
    matrix->values[3][2] = 0;
    matrix->values[0][3] = 0;
    matrix->values[1][3] = 0;
    matrix->values[2][3] = 0;
    matrix->values[3][3] = 0x100;
}
void RSDK::MatrixRotateZ(Matrix *matrix, int16 rotationZ)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    int32 sine           = sin1024LookupTable[rotationZ & 0x3FF] >> 2;
    int32 cosine         = cos1024LookupTable[rotationZ & 0x3FF] >> 2;
#else
    int32 sine           = Sin1024(rotationZ) >> 2;
    int32 cosine         = Cos1024(rotationZ) >> 2;
#endif
    matrix->values[0][0] = cosine;
    matrix->values[1][0] = -sine;
    matrix->values[2][0] = 0;
    matrix->values[3][0] = 0;
    matrix->values[0][1] = sine;
    matrix->values[1][1] = cosine;
    matrix->values[2][1] = 0;
    matrix->values[3][1] = 0;
    matrix->values[0][2] = 0;
    matrix->values[1][2] = 0;
    matrix->values[2][2] = 0x100;
    matrix->values[3][2] = 0;
    matrix->values[0][3] = 0;
    matrix->values[1][3] = 0;
    matrix->values[2][3] = 0;
    matrix->values[3][3] = 0x100;
}
void RSDK::MatrixRotateXYZ(Matrix *matrix, int16 rotationX, int16 rotationY, int16 rotationZ)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS || RETRO_USE_ORIGINAL_CODE
    int32 sinX = sin1024LookupTable[rotationX & 0x3FF] >> 2;
    int32 cosX = cos1024LookupTable[rotationX & 0x3FF] >> 2;
    int32 sinY = sin1024LookupTable[rotationY & 0x3FF] >> 2;
    int32 cosY = cos1024LookupTable[rotationY & 0x3FF] >> 2;
    int32 sinZ = sin1024LookupTable[rotationZ & 0x3FF] >> 2;
    int32 cosZ = cos1024LookupTable[rotationZ & 0x3FF] >> 2;
#else
    int32 sinX = Sin1024(rotationX) >> 2;
    int32 cosX = Cos1024(rotationX) >> 2;
    int32 sinY = Sin1024(rotationY) >> 2;
    int32 cosY = Cos1024(rotationY) >> 2;
    int32 sinZ = Sin1024(rotationZ) >> 2;
    int32 cosZ = Cos1024(rotationZ) >> 2;
#endif

    matrix->values[0][0] = (cosZ * cosY >> 8) + (sinZ * (sinY * sinX >> 8) >> 8);
    matrix->values[0][1] = -(sinZ * cosX) >> 8;
    matrix->values[0][2] = (sinZ * (cosY * sinX >> 8) >> 8) - (cosZ * sinY >> 8);
    matrix->values[0][3] = 0;
    matrix->values[1][0] = (sinZ * cosY >> 8) - (cosZ * (sinY * sinX >> 8) >> 8);
    matrix->values[1][1] = cosZ * cosX >> 8;
    matrix->values[1][2] = (-(sinZ * sinY) >> 8) - (cosZ * (cosY * sinX >> 8) >> 8);
    matrix->values[1][3] = 0;
    matrix->values[2][0] = sinY * cosX >> 8;
    matrix->values[2][1] = sinX;
    matrix->values[2][2] = cosY * cosX >> 8;
    matrix->values[2][3] = 0;
    matrix->values[3][0] = 0;
    matrix->values[3][1] = 0;
    matrix->values[3][2] = 0;
    matrix->values[3][3] = 0x100;
}
void RSDK::MatrixInverse(Matrix *dest, Matrix *matrix)
{
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    double inv[16], det;
    double m[16];
    for (int32 y = 0; y < 4; ++y) {
        for (int32 x = 0; x < 4; ++x) {
            m[(y << 2) + x] = matrix->values[y][x] / 256.0;
        }
    }

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0)
        return;

    det = 1.0 / det;

    for (int32 i = 0; i < 0x10; ++i) inv[i] = (int32)((inv[i] * det) * 256);
    for (int32 i = 0; i < 0x10; ++i) dest->values[i / 4][i % 4] = (int32)inv[i];
#endif
}

uint16 RSDK::LoadMesh(const char *filename, uint8 scope)
{
    if (!scope || scope > SCOPE_STAGE)
        return -1;

    char fullFilePath[0x100];
    sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Meshes/%s", filename);

    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5(fullFilePath, hash);

    for (int32 i = 0; i < MODEL_COUNT; ++i) {
        if (HASH_MATCH_MD5(hash, modelList[i].hash)) {
            return i;
        }
    }

    uint16 id = -1;
    for (id = 0; id < MODEL_COUNT; ++id) {
        if (modelList[id].scope == SCOPE_NONE)
            break;
    }

    if (id >= MODEL_COUNT)
        return -1;

    Model *model = &modelList[id];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
    FileInfo info;
    InitFileInfo(&info);
    if (LoadFile(&info, fullFilePath, FMODE_RB)) {
#else
    FileInfo info {};
    InitFileInfo(&info);

    sprintf_s(fullFilePath, sizeof(fullFilePath), "%sData/Meshes/%s", RSDK::SKU::userFileDir, filename);

    info.externalFile = true;
    auto fileOpened = LoadFile(&info, fullFilePath, FMODE_RB);

    if (!fileOpened) {
        CloseFile(&info);
        info = {};
        InitFileInfo(&info);
        sprintf_s(fullFilePath, sizeof(fullFilePath), "Data/Meshes/%s", filename); // put it back lmao
        fileOpened = LoadFile(&info, fullFilePath, FMODE_RB);
    }
    else {
        printf("found replacement file: %s\n", fullFilePath);
    }

    if (fileOpened) {
#endif
        uint32 sig = ReadInt32(&info, false);

        if (sig != RSDK_SIGNATURE_MDL) {
            CloseFile(&info);
            return -1;
        }

        model->scope = scope;
        HASH_COPY_MD5(model->hash, hash);

        model->flags         = ReadInt8(&info);
        model->faceVertCount = ReadInt8(&info);

        model->vertCount  = ReadInt16(&info);
        model->frameCount = ReadInt16(&info);

        AllocateStorage((void **)&model->vertices, sizeof(ModelVertex) * model->vertCount * model->frameCount, DATASET_STG, true);
#if RETRO_PLATFORM != RETRO_KALLISTIOS
        if (model->flags & MODEL_USETEXTURES)
            AllocateStorage((void **)&model->texCoords, sizeof(TexCoord) * model->vertCount, DATASET_STG, true);
#endif
        if (model->flags & MODEL_USECOLOURS)
            AllocateStorage((void **)&model->colors, sizeof(Color) * model->vertCount, DATASET_STG, true);

        if (model->flags & MODEL_USETEXTURES) {
            for (int32 v = 0; v < model->vertCount; ++v) {
                model->texCoords[v].x = ReadSingle(&info);
                model->texCoords[v].y = ReadSingle(&info);
            }
        }

        if (model->flags & MODEL_USECOLOURS) {
            for (int32 v = 0; v < model->vertCount; ++v) {
                model->colors[v].color = ReadInt32(&info, false);
            }
        }

#if RETRO_PLATFORM == RETRO_KALLISTIOS
        if (model->flags & MODEL_ISSTRIPPED) {
            // strip topology counts
            model->stripCount    = ReadInt16(&info);
            model->looseTriCount = ReadInt16(&info);

            // per-strip lengths
            AllocateStorage((void **)&model->stripLengths,
                            sizeof(uint16) * model->stripCount, DATASET_STG, true);
            for (int32 s = 0; s < model->stripCount; s++)
                model->stripLengths[s] = ReadInt16(&info);

            // total strip indices
            uint32 totalStripIdx = 0;
            for (int32 s = 0; s < model->stripCount; s++)
                totalStripIdx += model->stripLengths[s];

            AllocateStorage((void **)&model->stripIndices,
                            sizeof(uint16) * totalStripIdx, DATASET_STG, true);
            for (uint32 i = 0; i < totalStripIdx; i++)
                model->stripIndices[i] = ReadInt16(&info);

            // loose triangle indices
            if (model->looseTriCount > 0) {
                AllocateStorage((void **)&model->looseIndices,
                                sizeof(uint16) * model->looseTriCount * 3, DATASET_STG, true);
                for (int32 i = 0; i < model->looseTriCount * 3; i++)
                    model->looseIndices[i] = ReadInt16(&info);
            } else {
                model->looseIndices = nullptr;
            }

            // Reconstruct flat face indices from strips + loose tris
            // so wireframe draw modes can fall through to the face path
            uint32 faceCount = 0;
            for (int32 s = 0; s < model->stripCount; s++)
                faceCount += model->stripLengths[s] - 2;
            faceCount += model->looseTriCount;

            model->indexCount    = faceCount * 3;
            model->faceVertCount = 3;
            AllocateStorage((void **)&model->indices,
                            sizeof(uint16) * model->indexCount, DATASET_STG, true);

            uint32 out  = 0;
            uint16 *si  = model->stripIndices;
            for (int32 s = 0; s < model->stripCount; s++) {
                uint16 len = model->stripLengths[s];
                for (int32 i = 0; i + 2 < len; i++) {
                    if (i & 1) {
                        model->indices[out++] = si[i + 1];
                        model->indices[out++] = si[i];
                        model->indices[out++] = si[i + 2];
                    } else {
                        model->indices[out++] = si[i];
                        model->indices[out++] = si[i + 1];
                        model->indices[out++] = si[i + 2];
                    }
                }
                si += len;
            }
            for (int32 i = 0; i < model->looseTriCount * 3; i++)
                model->indices[out++] = model->looseIndices[i];
        } else {
            // existing code path for flat index list
            model->indexCount = ReadInt16(&info);
            AllocateStorage((void **)&model->indices, sizeof(uint16) * model->indexCount, DATASET_STG, true);
            for (int32 i = 0; i < model->indexCount; ++i) model->indices[i] = ReadInt16(&info);

            // zero out strip fields
            model->stripCount    = 0;
            model->looseTriCount = 0;
            model->stripLengths  = nullptr;
            model->stripIndices  = nullptr;
            model->looseIndices  = nullptr;
        }
#else
        model->indexCount = ReadInt16(&info);
        AllocateStorage((void **)&model->indices, sizeof(uint16) * model->indexCount, DATASET_STG, true);
        for (int32 i = 0; i < model->indexCount; ++i) model->indices[i] = ReadInt16(&info);
#endif

        for (int32 f = 0; f < model->frameCount; ++f) {
            for (int32 v = 0; v < model->vertCount; ++v) {
                model->vertices[(f * model->vertCount) + v].x = (int32)(ReadSingle(&info) * 0x100);
                model->vertices[(f * model->vertCount) + v].y = (int32)(ReadSingle(&info) * 0x100);
                model->vertices[(f * model->vertCount) + v].z = (int32)(ReadSingle(&info) * 0x100);

                model->vertices[(f * model->vertCount) + v].nx = 0;
                model->vertices[(f * model->vertCount) + v].ny = 0;
                model->vertices[(f * model->vertCount) + v].nz = 0;
                if (model->flags & MODEL_USENORMALS) {
                    model->vertices[(f * model->vertCount) + v].nx = (int32)(ReadSingle(&info) * 0x10000);
                    model->vertices[(f * model->vertCount) + v].ny = (int32)(ReadSingle(&info) * 0x10000);
                    model->vertices[(f * model->vertCount) + v].nz = (int32)(ReadSingle(&info) * 0x10000);
                }
            }
        }

        CloseFile(&info);
        return id;
    }
    return -1;
}
uint16 RSDK::Create3DScene(const char *name, uint16 vertexLimit, uint8 scope)
{
    if (!scope || scope > SCOPE_STAGE)
        return -1;

    RETRO_HASH_MD5(hash);
    GEN_HASH_MD5(name, hash);

    for (int32 i = 0; i < SCENE3D_COUNT; ++i) {
        if (HASH_MATCH_MD5(hash, scene3DList[i].hash)) {
            return i;
        }
    }

    uint16 id = -1;
    for (id = 0; id < SCENE3D_COUNT; ++id) {
        if (scene3DList[id].scope == SCOPE_NONE)
            break;
    }

    if (id >= SCENE3D_COUNT)
        return -1;

    Scene3D *scene = &scene3DList[id];

    if (vertexLimit > SCENE3D_VERT_COUNT || !vertexLimit)
        vertexLimit = SCENE3D_VERT_COUNT;

    scene->scope = scope;
    HASH_COPY_MD5(scene->hash, hash);
    scene->vertLimit = vertexLimit;
    scene->faceCount = 6;

    scene->projectionX = 8;
    scene->projectionY = 8;
    AllocateStorage((void **)&scene->vertices, sizeof(Scene3DVertex) * vertexLimit, DATASET_STG, true);
    AllocateStorage((void **)&scene->normals, sizeof(Scene3DVertex) * vertexLimit, DATASET_STG, true);
    AllocateStorage((void **)&scene->faceVertCounts, sizeof(uint8) * vertexLimit, DATASET_STG, true);
    AllocateStorage((void **)&scene->faceBuffer, sizeof(Scene3DFace) * vertexLimit, DATASET_STG, true);

    return id;
}

void RSDK::AddModelToScene(uint16 modelFrames, uint16 sceneIndex, uint8 drawMode, Matrix *matWorld, Matrix *matNormals, color color)
{
    if (modelFrames < MODEL_COUNT && sceneIndex < SCENE3D_COUNT) {
        if (matWorld) {
            Model *mdl            = &modelList[modelFrames];
            Scene3D *scn          = &scene3DList[sceneIndex];
            uint16 *indices       = mdl->indices;
            int32 vertID          = scn->vertexCount;
            uint8 *faceVertCounts = &scn->faceVertCounts[scn->faceCount];
            int32 indCnt          = mdl->indexCount;
            if (scn->vertLimit - vertID >= indCnt) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
                float __attribute__((aligned(32))) xmtrx[4][4];
                for (int i=0;i<16;i++) {
                    uint32 rowA              = i / 4;
                    uint32 rowB              = i % 4;
                    xmtrx[rowA][rowB] = (float)matWorld->values[rowB][rowA] * recip256;
                }
                shz_xmtrx_load_4x4((const shz_matrix_4x4_t*)xmtrx);
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                bool isWireframe =
                    (drawMode == S3D_WIREFRAME || drawMode == S3D_WIREFRAME_SHADED ||
                     drawMode == S3D_WIREFRAME_SCREEN ||
                     drawMode == S3D_WIREFRAME_SHADED_SCREEN);
                if ((mdl->flags & MODEL_ISSTRIPPED) && !isWireframe) {
                    // Transform all unique verts once (not per-index, not duplicated)
                    for (int32 v = 0; v < mdl->vertCount; v++) {
                        ModelVertex *mv = &mdl->vertices[v];
                        StripTransformedVert *sv = &stripXformVerts[stripVertOffset + v];

                        float w = 256.0f;
                        sv->x = mv->x;
                        sv->y = mv->y;
                        sv->z = mv->z;
                        mat_trans_nodiv(sv->x, sv->y, sv->z, w);

                        sv->color = color;

                        // Transform normal Y for lighting
                        if (mdl->flags & MODEL_USENORMALS) {
                            sv->ny = (float)mv->ny * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                        } else {
                            sv->ny = 0;
                        }
                    }

                    // If MODEL_USECOLOURS, override per-vert color from model data
                    if (mdl->flags & MODEL_USECOLOURS) {
                        for (int32 v = 0; v < mdl->vertCount; v++) {
                            stripXformVerts[stripVertOffset + v].color = mdl->colors[v].color;
                        }
                    }

                    scn->drawMode = drawMode;

                    if (stripItemCount < 16) {
                        StripItem &item = stripItems[stripItemCount++];
                        item.model     = mdl;
                        item.scene     = scn;
                        item.drawMode  = drawMode;
                        item.vertCount = mdl->vertCount;
                        item.verts     = &stripXformVerts[stripVertOffset];
                        stripVertOffset += mdl->vertCount;
                    }

                    return; // skip the existing per-index loop entirely
                }
#endif
                scn->vertexCount += mdl->indexCount;
                scn->drawMode = drawMode;
                scn->faceCount += indCnt / mdl->faceVertCount;

                int32 i = 0;
                int32 f = 0;

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                switch (mdl->flags) {
#else
                int isBaked = mdl->flags & MODEL_ISBAKED;

                switch ((mdl->flags & ~MODEL_ISBAKED)) {
#endif
                    default:
                    case MODEL_NOFLAGS:
                    case MODEL_USECOLOURS:
                        for (; i < mdl->indexCount;) {
                            faceVertCounts[f++] = mdl->faceVertCount;

                            for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                ModelVertex *modelVert = &mdl->vertices[indices[i++]];
                                Scene3DVertex *vertex  = &scn->vertices[vertID++];

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                vertex->x = matWorld->values[0][3] + (modelVert->z * matWorld->values[0][2] >> 8)
                                            + (matWorld->values[0][0] * modelVert->x >> 8) + (matWorld->values[0][1] * modelVert->y >> 8);
                                vertex->y = matWorld->values[1][3] + (modelVert->y * matWorld->values[1][1] >> 8)
                                            + (modelVert->z * matWorld->values[1][2] >> 8) + (matWorld->values[1][0] * modelVert->x >> 8);
                                vertex->z = matWorld->values[2][3] + ((modelVert->x * matWorld->values[2][0]) >> 8)
                                            + ((matWorld->values[2][2] * modelVert->z >> 8) + (matWorld->values[2][1] * modelVert->y >> 8));
#else
                                float w = 256.0f;
                                vertex->x = modelVert->x;
                                vertex->y = modelVert->y;
                                vertex->z = modelVert->z;
                                mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                vertex->isBaked = isBaked;
#endif

                                vertex->color = color;
                            }
                        }
                        break;

                    case MODEL_USENORMALS:
                        if (matNormals) {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *modelVert = &mdl->vertices[indices[i++]];
                                    Scene3DVertex *vertex  = &scn->vertices[vertID++];

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (modelVert->z * matWorld->values[0][2] >> 8)
                                                + (modelVert->x * matWorld->values[0][0] >> 8) + (modelVert->y * matWorld->values[0][1] >> 8);
                                    vertex->y = matWorld->values[1][3] + (modelVert->y * matWorld->values[1][1] >> 8)
                                                + (matWorld->values[1][0] * modelVert->x >> 8) + (modelVert->z * matWorld->values[1][2] >> 8);
                                    vertex->z = matWorld->values[2][3] + (modelVert->x * matWorld->values[2][0] >> 8)
                                                + (matWorld->values[2][2] * modelVert->z >> 8) + (matWorld->values[2][1] * modelVert->y >> 8);

                                    vertex->nx = (modelVert->nz * matNormals->values[0][2] >> 8) + (modelVert->nx * matNormals->values[0][0] >> 8)
                                                 + (matNormals->values[0][1] * modelVert->ny >> 8);
                                    vertex->ny = (modelVert->ny * matNormals->values[1][1] >> 8) + (modelVert->nz * matNormals->values[1][2] >> 8)
                                                 + (modelVert->nx * matNormals->values[1][0] >> 8);
                                    vertex->nz =
                                        ((modelVert->ny * matNormals->values[2][1]) >> 8)
                                        + ((matNormals->values[2][0] * modelVert->nx >> 8) + (modelVert->nz * matNormals->values[2][2] >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = modelVert->x;
                                    vertex->y = modelVert->y;
                                    vertex->z = modelVert->z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);

                                    vertex->ny = (float)modelVert->ny * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                                    vertex->isBaked = isBaked;
#endif

                                    vertex->color = color;
                                }
                            }
                        }
                        else {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *modelVert = &mdl->vertices[indices[i++]];
                                    Scene3DVertex *vertex  = &scn->vertices[vertID++];

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (modelVert->z * matWorld->values[0][2] >> 8)
                                                + (matWorld->values[0][0] * modelVert->x >> 8) + (matWorld->values[0][1] * modelVert->y >> 8);
                                    vertex->y = matWorld->values[1][3] + (modelVert->y * matWorld->values[1][1] >> 8)
                                                + (modelVert->z * matWorld->values[1][2] >> 8) + (matWorld->values[1][0] * modelVert->x >> 8);
                                    vertex->z = matWorld->values[2][3] + ((matWorld->values[2][2] * modelVert->z) >> 8)
                                                + ((matWorld->values[2][0] * modelVert->x >> 8) + (matWorld->values[2][1] * modelVert->y >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = modelVert->x;
                                    vertex->y = modelVert->y;
                                    vertex->z = modelVert->z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->isBaked = isBaked;
#endif

                                    vertex->color = color;
                                }
                            }
                        }
                        break;

                    case MODEL_USENORMALS | MODEL_USECOLOURS:
                        if (matNormals) {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *modelVert = &mdl->vertices[indices[i]];
                                    Color *modelColor      = &mdl->colors[indices[i++]];
                                    Scene3DVertex *vertex  = &scn->vertices[vertID++];

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (matWorld->values[0][2] * modelVert->z >> 8)
                                                + (modelVert->y * matWorld->values[0][1] >> 8) + (matWorld->values[0][0] * modelVert->x >> 8);
                                    vertex->y = matWorld->values[1][3] + (matWorld->values[1][2] * modelVert->z >> 8)
                                                + (modelVert->y * matWorld->values[1][1] >> 8) + (matWorld->values[1][0] * modelVert->x >> 8);
                                    vertex->z = matWorld->values[2][3] + (modelVert->x * matWorld->values[2][0] >> 8)
                                                + (modelVert->y * matWorld->values[2][1] >> 8) + (matWorld->values[2][2] * modelVert->z >> 8);

                                    vertex->nx = (matNormals->values[0][0] * modelVert->nx >> 8) + (modelVert->ny * matNormals->values[0][1] >> 8)
                                                 + (matNormals->values[0][2] * modelVert->nz >> 8);
                                    vertex->ny = (matNormals->values[1][0] * modelVert->nx >> 8) + (modelVert->ny * matNormals->values[1][1] >> 8)
                                                 + (matNormals->values[1][2] * modelVert->nz >> 8);
                                    vertex->nz =
                                        ((matNormals->values[2][2] * modelVert->nz) >> 8)
                                        + ((modelVert->ny * matNormals->values[2][1] >> 8) + (matNormals->values[2][0] * modelVert->nx >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = modelVert->x;
                                    vertex->y = modelVert->y;
                                    vertex->z = modelVert->z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);

                                    vertex->ny = (float)modelVert->ny * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                                    vertex->isBaked = isBaked;
#endif

                                    vertex->color = modelColor->color;
                                }
                            }
                        }
                        else {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *modelVert = &mdl->vertices[indices[i]];
                                    Color *modelColor      = &mdl->colors[indices[i++]];
                                    Scene3DVertex *vertex  = &scn->vertices[vertID++];

#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (matWorld->values[0][0] * modelVert->x >> 8)
                                                + (modelVert->y * matWorld->values[0][1] >> 8) + (modelVert->z * matWorld->values[0][2] >> 8);
                                    vertex->y = matWorld->values[1][3] + (modelVert->z * matWorld->values[1][2] >> 8)
                                                + (matWorld->values[1][0] * modelVert->x >> 8) + (modelVert->y * matWorld->values[1][1] >> 8);
                                    vertex->z = matWorld->values[2][3] + (matWorld->values[2][2] * modelVert->z >> 8)
                                                + (modelVert->y * matWorld->values[2][1] >> 8) + (modelVert->x * matWorld->values[2][0] >> 8);
#else
                                    float w = 256.0f;
                                    vertex->x = modelVert->x;
                                    vertex->y = modelVert->y;
                                    vertex->z = modelVert->z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->isBaked = isBaked;
#endif

                                    vertex->color = modelColor->color;
                                }
                            }
                        }
                        break;
                }
            }
        }
    }
}
void RSDK::AddMeshFrameToScene(uint16 modelFrames, uint16 sceneIndex, Animator *animator, uint8 drawMode, Matrix *matWorld, Matrix *matNormals,
                               color color)
{
    if (modelFrames < MODEL_COUNT && sceneIndex < SCENE3D_COUNT) {
        if (matWorld && animator) {
            Model *mdl            = &modelList[modelFrames];
            Scene3D *scn          = &scene3DList[sceneIndex];
            uint16 *indices       = mdl->indices;
            int32 vertID          = scn->vertexCount;
            uint8 *faceVertCounts = &scn->faceVertCounts[scn->faceCount];
            int32 indCnt          = mdl->indexCount;
            if (scn->vertLimit - vertID >= indCnt) {
#if RETRO_PLATFORM == RETRO_KALLISTIOS
                float __attribute__((aligned(32))) xmtrx[4][4];
                for (int i=0;i<16;i++) {
                    uint32 rowA              = i / 4;
                    uint32 rowB              = i % 4;
                    xmtrx[rowA][rowB] = (float)matWorld->values[rowB][rowA] * recip256;
                }
//                mat_load(&xmtrx);
                shz_xmtrx_load_4x4((const shz_matrix_4x4_t*)xmtrx);
#endif

#if RETRO_PLATFORM == RETRO_KALLISTIOS
                bool isWireframe =
                    (drawMode == S3D_WIREFRAME || drawMode == S3D_WIREFRAME_SHADED ||
                     drawMode == S3D_WIREFRAME_SCREEN ||
                     drawMode == S3D_WIREFRAME_SHADED_SCREEN);
                if ((mdl->flags & MODEL_ISSTRIPPED) && !isWireframe) {
                    int32 nextFrame = animator->frameID + 1;
                    if (nextFrame >= animator->frameCount)
                        nextFrame = animator->loopIndex;
                    int32 frameOffset     = animator->frameID * mdl->vertCount;
                    int32 nextFrameOffset = nextFrame * mdl->vertCount;
                    float interpolate     = animator->timer * recip256;

                    for (int32 v = 0; v < mdl->vertCount; v++) {
                        ModelVertex *fv  = &mdl->vertices[frameOffset + v];
                        ModelVertex *nfv = &mdl->vertices[nextFrameOffset + v];
                        StripTransformedVert *sv = &stripXformVerts[stripVertOffset + v];

                        // Interpolate position between frames
                        float w = 256.0f;
                        sv->x = fv->x + (interpolate * (nfv->x - fv->x));
                        sv->y = fv->y + (interpolate * (nfv->y - fv->y));
                        sv->z = fv->z + (interpolate * (nfv->z - fv->z));
                        mat_trans_nodiv(sv->x, sv->y, sv->z, w);

                        sv->color = color;

                        if (mdl->flags & MODEL_USENORMALS) {
                            // Interpolate normal Y too
                            float nyA     = (float)fv->ny;
                            float nyB     = (float)nfv->ny;
                            float nyInterp = nyA + interpolate * (nyB - nyA);
                            sv->ny = nyInterp * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                        } else {
                            sv->ny = 0;
                        }
                    }

                    // If MODEL_USECOLOURS, override per-vert color from model data
                    if (mdl->flags & MODEL_USECOLOURS) {
                        for (int32 v = 0; v < mdl->vertCount; v++) {
                            stripXformVerts[stripVertOffset + v].color = mdl->colors[v].color;
                        }
                    }

                    scn->drawMode = drawMode;
                    if (stripItemCount < 16) {
                        StripItem &item = stripItems[stripItemCount++];
                        item.model     = mdl;
                        item.scene     = scn;
                        item.drawMode  = drawMode;
                        item.vertCount = mdl->vertCount;
                        item.verts     = &stripXformVerts[stripVertOffset];
                        stripVertOffset += mdl->vertCount;
                    }
                    return; // skip the existing per-index loop entirely
                }
#endif
                scn->vertexCount += mdl->indexCount;
                scn->drawMode = drawMode;
                scn->faceCount += indCnt / mdl->faceVertCount;

                int32 nextFrame = animator->frameID + 1;
                if (nextFrame >= animator->frameCount)
                    nextFrame = animator->loopIndex;
                int32 frameOffset     = animator->frameID * mdl->vertCount;
                int32 nextFrameOffset = nextFrame * mdl->vertCount;

                int32 i           = 0;
                int32 f           = 0;
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                int32 interpolate = animator->timer;
#else
                float interpolate = animator->timer * recip256;
#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                switch (mdl->flags) {
#else
                int isBaked = mdl->flags & MODEL_ISBAKED;

                switch ((mdl->flags & ~MODEL_ISBAKED)) {
#endif
                    default:
                    case MODEL_NOFLAGS:
                    case MODEL_USECOLOURS:
                        for (; i < mdl->indexCount;) {
                            faceVertCounts[f++] = mdl->faceVertCount;

                            for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                ModelVertex *frameVert     = &mdl->vertices[frameOffset + indices[i]];
                                ModelVertex *nextFrameVert = &mdl->vertices[nextFrameOffset + indices[i]];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)) >> 8);
                                int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)) >> 8);
                                int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)) >> 8);
#else
                                int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)));
                                int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)));
                                int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)));
#endif
                                i++;
                                Scene3DVertex *vertex = &scn->vertices[vertID++];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                vertex->x = matWorld->values[0][3] + (z * matWorld->values[0][2] >> 8) + (matWorld->values[0][0] * x >> 8)
                                            + (matWorld->values[0][1] * y >> 8);
                                vertex->y = matWorld->values[1][3] + (y * matWorld->values[1][1] >> 8) + (z * matWorld->values[1][2] >> 8)
                                            + (matWorld->values[1][0] * x >> 8);
                                vertex->z = matWorld->values[2][3] + ((x * matWorld->values[2][0]) >> 8)
                                            + ((matWorld->values[2][2] * z >> 8) + (matWorld->values[2][1] * y >> 8));
#else
                                float w = 256.0f;
                                vertex->x = x;
                                vertex->y = y;
                                vertex->z = z;
                                mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                vertex->isBaked = isBaked;
#endif
                                vertex->color = color;
                            }
                        }
                        break;

                    case MODEL_USENORMALS:
                        if (matNormals) {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *frameVert     = &mdl->vertices[frameOffset + indices[i]];
                                    ModelVertex *nextFrameVert = &mdl->vertices[nextFrameOffset + indices[i]];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)) >> 8);
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)) >> 8);
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)) >> 8);
#else
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)));
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)));
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)));
#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 nx                   = frameVert->nx + ((interpolate * (nextFrameVert->nx - frameVert->nx)) >> 8);
                                    int32 ny                   = frameVert->ny + ((interpolate * (nextFrameVert->ny - frameVert->ny)) >> 8);
                                    int32 nz                   = frameVert->nz + ((interpolate * (nextFrameVert->nz - frameVert->nz)) >> 8);
#else
                                    int32 ny                   = frameVert->ny;
#endif
                                    i++;

                                    Scene3DVertex *vertex = &scn->vertices[vertID++];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (z * matWorld->values[0][2] >> 8) + (x * matWorld->values[0][0] >> 8)
                                                + (y * matWorld->values[0][1] >> 8);
                                    vertex->y = matWorld->values[1][3] + (y * matWorld->values[1][1] >> 8) + (matWorld->values[1][0] * x >> 8)
                                                + (z * matWorld->values[1][2] >> 8);
                                    vertex->z = matWorld->values[2][3] + (x * matWorld->values[2][0] >> 8) + (matWorld->values[2][2] * z >> 8)
                                                + (matWorld->values[2][1] * y >> 8);

                                    vertex->nx = (nz * matNormals->values[0][2] >> 8) + (nx * matNormals->values[0][0] >> 8)
                                                 + (matNormals->values[0][1] * ny >> 8);
                                    vertex->ny = (ny * matNormals->values[1][1] >> 8) + (nz * matNormals->values[1][2] >> 8)
                                                 + (nx * matNormals->values[1][0] >> 8);
                                    vertex->nz = ((ny * matNormals->values[2][1]) >> 8)
                                                 + ((matNormals->values[2][0] * nx >> 8) + (nz * matNormals->values[2][2] >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = x;
                                    vertex->y = y;
                                    vertex->z = z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->ny = (float)ny * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                                    vertex->isBaked = isBaked;
#endif
                                    vertex->color = color;
                                }
                            }
                        }
                        else {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *frameVert     = &mdl->vertices[frameOffset + indices[i]];
                                    ModelVertex *nextFrameVert = &mdl->vertices[nextFrameOffset + indices[i]];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)) >> 8);
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)) >> 8);
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)) >> 8);
#else
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)));
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)));
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)));
#endif
                                    i++;
                                    Scene3DVertex *vertex = &scn->vertices[vertID++];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (z * matWorld->values[0][2] >> 8) + (matWorld->values[0][0] * x >> 8)
                                                + (matWorld->values[0][1] * y >> 8);
                                    vertex->y = matWorld->values[1][3] + (y * matWorld->values[1][1] >> 8) + (z * matWorld->values[1][2] >> 8)
                                                + (matWorld->values[1][0] * x >> 8);
                                    vertex->z = matWorld->values[2][3] + ((matWorld->values[2][2] * z) >> 8)
                                                + ((matWorld->values[2][0] * x >> 8) + (matWorld->values[2][1] * y >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = x;
                                    vertex->y = y;
                                    vertex->z = z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->isBaked = isBaked;
#endif
                                    vertex->color = color;
                                }
                            }
                        }
                        break;

                    case MODEL_USENORMALS | MODEL_USECOLOURS:
                        if (matNormals) {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *frameVert     = &mdl->vertices[frameOffset + indices[i]];
                                    ModelVertex *nextFrameVert = &mdl->vertices[nextFrameOffset + indices[i]];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)) >> 8);
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)) >> 8);
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)) >> 8);
#else
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)));
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)));
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)));
#endif
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 nx                   = frameVert->nx + ((interpolate * (nextFrameVert->nx - frameVert->nx)) >> 8);
                                    int32 ny                   = frameVert->ny + ((interpolate * (nextFrameVert->ny - frameVert->ny)) >> 8);
                                    int32 nz                   = frameVert->nz + ((interpolate * (nextFrameVert->nz - frameVert->nz)) >> 8);
#else
                                    int32 ny                   = frameVert->ny;
#endif
                                    Color *modelColor     = &mdl->colors[indices[i++]];
                                    Scene3DVertex *vertex = &scn->vertices[vertID++];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (matWorld->values[0][2] * z >> 8) + (y * matWorld->values[0][1] >> 8)
                                                + (matWorld->values[0][0] * x >> 8);
                                    vertex->y = matWorld->values[1][3] + (matWorld->values[1][2] * z >> 8) + (y * matWorld->values[1][1] >> 8)
                                                + (matWorld->values[1][0] * x >> 8);
                                    vertex->z = matWorld->values[2][3] + (x * matWorld->values[2][0] >> 8) + (y * matWorld->values[2][1] >> 8)
                                                + (matWorld->values[2][2] * z >> 8);
                                    vertex->nx = (matNormals->values[0][0] * nx >> 8) + (ny * matNormals->values[0][1] >> 8)
                                                 + (matNormals->values[0][2] * nz >> 8);
                                    vertex->ny = (matNormals->values[1][0] * nx >> 8) + (ny * matNormals->values[1][1] >> 8)
                                                 + (matNormals->values[1][2] * nz >> 8);
                                    vertex->nz = ((matNormals->values[2][2] * nz) >> 8)
                                                 + ((ny * matNormals->values[2][1] >> 8) + (matNormals->values[2][0] * nx >> 8));
#else
                                    float w = 256.0f;
                                    vertex->x = x;
                                    vertex->y = y;
                                    vertex->z = z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->ny = (float)ny * (float)matNormals->values[1][1] * NORMAL_VAL_SCALE;
                                    vertex->isBaked = isBaked;
#endif
                                    vertex->color = modelColor->color;
                                }
                            }
                        }
                        else {
                            for (; i < mdl->indexCount;) {
                                faceVertCounts[f++] = mdl->faceVertCount;

                                for (int32 c = 0; c < mdl->faceVertCount; ++c) {
                                    ModelVertex *frameVert     = &mdl->vertices[frameOffset + indices[i]];
                                    ModelVertex *nextFrameVert = &mdl->vertices[nextFrameOffset + indices[i]];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)) >> 8);
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)) >> 8);
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)) >> 8);
#else
                                    int32 x                    = frameVert->x + ((interpolate * (nextFrameVert->x - frameVert->x)));
                                    int32 y                    = frameVert->y + ((interpolate * (nextFrameVert->y - frameVert->y)));
                                    int32 z                    = frameVert->z + ((interpolate * (nextFrameVert->z - frameVert->z)));
#endif
                                    Color *modelColor          = &mdl->colors[indices[i++]];
                                    Scene3DVertex *vertex      = &scn->vertices[vertID++];
#if RETRO_PLATFORM != RETRO_KALLISTIOS
                                    vertex->x = matWorld->values[0][3] + (matWorld->values[0][0] * x >> 8) + (y * matWorld->values[0][1] >> 8)
                                                + (z * matWorld->values[0][2] >> 8);
                                    vertex->y = matWorld->values[1][3] + (z * matWorld->values[1][2] >> 8) + (matWorld->values[1][0] * x >> 8)
                                                + (y * matWorld->values[1][1] >> 8);
                                    vertex->z = matWorld->values[2][3] + (matWorld->values[2][2] * z >> 8) + (y * matWorld->values[2][1] >> 8)
                                                + (x * matWorld->values[2][0] >> 8);
#else
                                    float w = 256.0f;
                                    vertex->x = x;
                                    vertex->y = y;
                                    vertex->z = z;
                                    mat_trans_nodiv(vertex->x, vertex->y, vertex->z, w);
                                    vertex->isBaked = isBaked;
#endif
                                    vertex->color = modelColor->color;
                                }
                            }
                        }
                        break;
                }
            }
        }
    }
}

#if RETRO_PLATFORM != RETRO_KALLISTIOS
void RSDK::Draw3DScene(uint16 sceneID)
{
    if (sceneID < SCENE3D_COUNT) {
        Entity *entity = sceneInfo.entity;
        Scene3D *scn   = &scene3DList[sceneID];

        // Setup face buffer.
        // Each face's depth is an average of the depth of its vertices.
        Scene3DVertex *vertices = scn->vertices;
        Scene3DFace *faceBuffer = scn->faceBuffer;
        uint8 *faceVertCounts = scn->faceVertCounts;

        int32 vertIndex = 0;
        for (int32 i = 0; i < scn->faceCount; ++i) {
            switch (*faceVertCounts) {
                default:
                case 1:
                    faceBuffer->depth = vertices[0].z;
                    vertices += *faceVertCounts;
                    break;

                case 2:
                    faceBuffer->depth = vertices[0].z >> 1;
                    faceBuffer->depth += vertices[1].z >> 1;
                    vertices += 2;
                    break;

                case 3:
                    faceBuffer->depth = vertices[0].z >> 1;
                    faceBuffer->depth = (faceBuffer->depth + (vertices[1].z >> 1)) >> 1;
                    faceBuffer->depth += vertices[2].z >> 1;
                    vertices += 3;
                    break;

                case 4:
                    faceBuffer->depth = vertices[0].z >> 2;
                    faceBuffer->depth += vertices[1].z >> 2;
                    faceBuffer->depth += vertices[2].z >> 2;
                    faceBuffer->depth += vertices[3].z >> 2;
                    vertices += 4;
                    break;
            }

            faceBuffer->index = vertIndex;
            vertIndex += *faceVertCounts;

            ++faceBuffer;
            ++faceVertCounts;
        }

        // Sort the face buffer. This is needed so that the faces don't overlap each other incorrectly when they're rendered.
        // This is an insertion sort, taken from here:
        // https://web.archive.org/web/20110108233032/http://rosettacode.org/wiki/Sorting_algorithms/Insertion_sort#C

        Scene3DFace *a = scn->faceBuffer;

        int i, j;
        Scene3DFace temp;

        for(i=1; i<scn->faceCount; i++)
        {
            temp = a[i];
            j = i-1;
            while(j>=0 && a[j].depth < temp.depth)
            {
                a[j+1] = a[j];
                j -= 1;
            }
            a[j+1] = temp;
        }
        // Finally, display the faces.

        uint8 *vertCnt = scn->faceVertCounts;
        Vector2 vertPos[4];
        uint32 vertClrs[4];

        switch (scn->drawMode) {
            default: break;

            case S3D_WIREFRAME:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    for (int32 v = 0; v < *vertCnt - 1; ++v) {
                        DrawLine(drawVert[v + 0].x << 8, drawVert[v + 0].y << 8, drawVert[v + 1].x << 8, drawVert[v + 1].y << 8, drawVert[0].color,
                                 entity->alpha, entity->inkEffect, false);
                    }
                    DrawLine(drawVert[0].x << 8, drawVert[0].y << 8, drawVert[*vertCnt - 1].x << 8, drawVert[*vertCnt - 1].y << 8, drawVert[0].color,
                             entity->alpha, entity->inkEffect, false);
                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    for (int32 v = 0; v < *vertCnt; ++v) {
                        vertPos[v].x = (drawVert[v].x << 8) - (currentScreen->position.x << 16);
                        vertPos[v].y = (drawVert[v].y << 8) - (currentScreen->position.y << 16);
                    }
                    DrawFace(vertPos, *vertCnt, (drawVert->color >> 16) & 0xFF, (drawVert->color >> 8) & 0xFF, (drawVert->color >> 0) & 0xFF,
                             entity->alpha, entity->inkEffect);
                    vertCnt++;
                }
                break;

            // Might have been reserved for textures?
            // not sure about this, just a guess based on tex coords existing in the model format spec
            case S3D_UNUSED_1: break;
            case S3D_UNUSED_2: break;

            case S3D_WIREFRAME_SHADED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 ny1 = 0;
                    for (int32 v = 0; v < vertCount; ++v) {
                        ny1 += drawVert[v].ny;
                    }

                    int32 normal    = ny1 / vertCount;
                    int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                    int32 specular = normalVal >> 6 >> scn->specularIntensityX;
                    specular       = CLAMP(specular, 0x00, 0xFF);
                    int32 r = specular + ((int32)((drawVert->color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                    specular = normalVal >> 6 >> scn->specularIntensityY;
                    specular = CLAMP(specular, 0x00, 0xFF);
                    int32 g  = specular + ((int32)((drawVert->color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                    specular = normalVal >> 6 >> scn->specularIntensityZ;
                    specular = CLAMP(specular, 0x00, 0xFF);
                    int32 b  = specular + ((int32)((drawVert->color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                    r = CLAMP(r, 0x00, 0xFF);
                    g = CLAMP(g, 0x00, 0xFF);
                    b = CLAMP(b, 0x00, 0xFF);

                    uint32 color = (r << 16) | (g << 8) | (b << 0);

                    for (int32 v = 0; v < vertCount - 1; ++v) {
                        DrawLine(drawVert[v + 0].x << 8, drawVert[v + 0].y << 8, drawVert[v + 1].x << 8, drawVert[v + 1].y << 8, color, entity->alpha,
                                 entity->inkEffect, false);
                    }
                    DrawLine(drawVert[vertCount - 1].x << 8, drawVert[vertCount - 1].y << 8, drawVert[0].x << 8, drawVert[0].y << 8, color,
                             entity->alpha, entity->inkEffect, false);

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 ny = 0;
                    for (int32 v = 0; v < vertCount; ++v) {
                        ny += drawVert[v].ny;
                        vertPos[v].x = (drawVert[v].x << 8) - (currentScreen->position.x << 16);
                        vertPos[v].y = (drawVert[v].y << 8) - (currentScreen->position.y << 16);
                    }

                    int32 normal    = ny / vertCount;
                    int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                    int32 specular = normalVal >> 6 >> scn->specularIntensityX;
                    specular       = CLAMP(specular, 0x00, 0xFF);
                    int32 r = specular + ((int32)((drawVert->color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                    specular = normalVal >> 6 >> scn->specularIntensityY;
                    specular = CLAMP(specular, 0x00, 0xFF);
                    int32 g  = specular + ((int32)((drawVert->color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                    specular = normalVal >> 6 >> scn->specularIntensityZ;
                    specular = CLAMP(specular, 0x00, 0xFF);
                    int32 b  = specular + ((int32)((drawVert->color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                    r = CLAMP(r, 0x00, 0xFF);
                    g = CLAMP(g, 0x00, 0xFF);
                    b = CLAMP(b, 0x00, 0xFF);

                    uint32 color = (r << 16) | (g << 8) | (b << 0);

                    drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    DrawFace(vertPos, *vertCnt, (color >> 16) & 0xFF, (color >> 8) & 0xFF, (color >> 0) & 0xFF, entity->alpha, entity->inkEffect);

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_BLENDED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    for (int32 v = 0; v < vertCount; ++v) {
                        vertPos[v].x = (drawVert[v].x << 8) - (currentScreen->position.x << 16);
                        vertPos[v].y = (drawVert[v].y << 8) - (currentScreen->position.y << 16);

                        int32 normal    = drawVert[v].ny;
                        int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                        int32 specular = (normalVal >> 6) >> scn->specularIntensityX;
                        specular       = CLAMP(specular, 0x00, 0xFF);
                        int32 r = specular + ((int32)((drawVert->color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                        specular = (normalVal >> 6) >> scn->specularIntensityY;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 g  = specular + ((int32)((drawVert->color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                        specular = (normalVal >> 6) >> scn->specularIntensityZ;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 b  = specular + ((int32)((drawVert->color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                        r = CLAMP(r, 0x00, 0xFF);
                        g = CLAMP(g, 0x00, 0xFF);
                        b = CLAMP(b, 0x00, 0xFF);

                        vertClrs[v] = (r << 16) | (g << 8) | (b << 0);
                    }

                    DrawBlendedFace(vertPos, vertClrs, *vertCnt, entity->alpha, entity->inkEffect);

                    vertCnt++;
                }
                break;

            case S3D_WIREFRAME_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];

                    int32 v = 0;
                    for (; v < *vertCnt && v < 0xFF; ++v) {
                        int32 vertZ = drawVert[v].z;
                        if (vertZ < 0x100) {
                            v = 0xFF;
                        }
                        else {
                            vertPos[v].x = currentScreen->center.x + (drawVert[v].x << scn->projectionX) / vertZ;
                            vertPos[v].y = currentScreen->center.y - (drawVert[v].y << scn->projectionY) / vertZ;
                        }
                    }

                    if (v < 0xFF) {
                        for (int32 v = 0; v < *vertCnt - 1; ++v) {
                            DrawLine(vertPos[v + 0].x, vertPos[v + 0].y, vertPos[v + 1].x, vertPos[v + 1].y, drawVert[0].color, entity->alpha,
                                     entity->inkEffect, true);
                        }
                        DrawLine(vertPos[0].x, vertPos[0].y, vertPos[*vertCnt - 1].x, vertPos[*vertCnt - 1].y, drawVert[0].color, entity->alpha,
                                 entity->inkEffect, true);
                    }

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 v = 0;
                    for (; v < vertCount && v < 0xFF; ++v) {
                        int32 vertZ = drawVert[v].z;
                        if (vertZ < 0x100) {
                            v = 0xFF;
                        }
                        else {
                            vertPos[v].x = (currentScreen->center.x << 16) + ((drawVert[v].x << scn->projectionX) / vertZ << 16);
                            vertPos[v].y = (currentScreen->center.y << 16) - ((drawVert[v].y << scn->projectionY) / vertZ << 16);
                        }
                    }

                    if (v < 0xFF) {
                        DrawFace(vertPos, *vertCnt, (drawVert[0].color >> 16) & 0xFF, (drawVert[0].color >> 8) & 0xFF,
                                 (drawVert[0].color >> 0) & 0xFF, entity->alpha, entity->inkEffect);
                    }
                    vertCnt++;
                }
                break;

            case S3D_WIREFRAME_SHADED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 v   = 0;
                    int32 ny1 = 0;
                    for (; v < *vertCnt && v < 0xFF; ++v) {
                        int32 vertZ = drawVert[v].z;
                        if (vertZ < 0x100) {
                            v = 0xFF;
                        }
                        else {
                            vertPos[v].x = currentScreen->center.x + (drawVert[v].x << scn->projectionX) / vertZ;
                            vertPos[v].y = currentScreen->center.y - (drawVert[v].y << scn->projectionY) / vertZ;
                            ny1 += drawVert[v].ny;
                        }
                    }

                    if (v < 0xFF) {
                        int32 normal    = ny1 / vertCount;
                        int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                        int32 specular = normalVal >> 6 >> scn->specularIntensityX;
                        specular       = CLAMP(specular, 0x00, 0xFF);
                        int32 r = specular + ((int32)((drawVert[0].color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                        specular = normalVal >> 6 >> scn->specularIntensityY;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 g  = specular + ((int32)((drawVert[0].color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                        specular = normalVal >> 6 >> scn->specularIntensityZ;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 b  = specular + ((int32)((drawVert[0].color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                        r = CLAMP(r, 0x00, 0xFF);
                        g = CLAMP(g, 0x00, 0xFF);
                        b = CLAMP(b, 0x00, 0xFF);

                        uint32 color = (r << 16) | (g << 8) | (b << 0);

                        for (int32 v = 0; v < *vertCnt - 1; ++v) {
                            DrawLine(vertPos[v + 0].x, vertPos[v + 0].y, vertPos[v + 1].x, vertPos[v + 1].y, color, entity->alpha, entity->inkEffect,
                                     true);
                        }
                        DrawLine(vertPos[*vertCnt - 1].x, vertPos[*vertCnt - 1].y, vertPos[0].x, vertPos[0].y, color, entity->alpha,
                                 entity->inkEffect, true);
                    }

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 v  = 0;
                    int32 ny = 0;
                    for (; v < vertCount && v < 0xFF; ++v) {
                        int32 vertZ = drawVert[v].z;
                        if (vertZ < 0x100) {
                            v = 0xFF;
                        }
                        else {
                            vertPos[v].x = (currentScreen->center.x << 16) + ((drawVert[v].x << scn->projectionX) / vertZ << 16);
                            vertPos[v].y = (currentScreen->center.y << 16) - ((drawVert[v].y << scn->projectionY) / vertZ << 16);
                            ny += drawVert[v].ny;
                        }
                    }

                    if (v < 0xFF) {
                        int32 normal    = ny / vertCount;
                        int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                        int32 specular = normalVal >> 6 >> scn->specularIntensityX;
                        specular       = CLAMP(specular, 0x00, 0xFF);
                        int32 r = specular + ((int32)((drawVert[0].color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                        specular = normalVal >> 6 >> scn->specularIntensityY;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 g  = specular + ((int32)((drawVert[0].color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                        specular = normalVal >> 6 >> scn->specularIntensityZ;
                        specular = CLAMP(specular, 0x00, 0xFF);
                        int32 b  = specular + ((int32)((drawVert[0].color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                        r = CLAMP(r, 0x00, 0xFF);
                        g = CLAMP(g, 0x00, 0xFF);
                        b = CLAMP(b, 0x00, 0xFF);

                        uint32 color = (r << 16) | (g << 8) | (b << 0);

                        drawVert = &scn->vertices[scn->faceBuffer[f].index];
                        DrawFace(vertPos, *vertCnt, (color >> 16) & 0xFF, (color >> 8) & 0xFF, (color >> 0) & 0xFF, entity->alpha, entity->inkEffect);
                    }

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_BLENDED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 v = 0;
                    for (; v < vertCount && v < 0xFF; ++v) {
                        int32 vertZ = drawVert[v].z;
                        if (vertZ < 0x100) {
                            v = 0xFF;
                        }
                        else {
                            vertPos[v].x = (currentScreen->center.x << 16) + ((drawVert[v].x << scn->projectionX) / vertZ << 16);
                            vertPos[v].y = (currentScreen->center.y << 16) - ((drawVert[v].y << scn->projectionY) / vertZ << 16);

                            int32 normal    = drawVert[v].ny;
                            int32 normalVal = (normal >> 2) * (abs(normal) >> 2);

                            int32 specular = normalVal >> 6 >> scn->specularIntensityX;
                            specular       = CLAMP(specular, 0x00, 0xFF);
                            int32 r =
                                specular + ((int32)((drawVert[v].color >> 16) & 0xFF) * ((normal >> 10) + scn->diffuseX) >> scn->diffuseIntensityX);

                            specular = normalVal >> 6 >> scn->specularIntensityY;
                            specular = CLAMP(specular, 0x00, 0xFF);
                            int32 g =
                                specular + ((int32)((drawVert[v].color >> 8) & 0xFF) * ((normal >> 10) + scn->diffuseY) >> scn->diffuseIntensityY);

                            specular = normalVal >> 6 >> scn->specularIntensityZ;
                            specular = CLAMP(specular, 0x00, 0xFF);
                            int32 b =
                                specular + ((int32)((drawVert[v].color >> 0) & 0xFF) * ((normal >> 10) + scn->diffuseZ) >> scn->diffuseIntensityZ);

                            r = CLAMP(r, 0x00, 0xFF);
                            g = CLAMP(g, 0x00, 0xFF);
                            b = CLAMP(b, 0x00, 0xFF);

                            vertClrs[v] = (r << 16) | (g << 8) | (b << 0);
                        }
                    }

                    if (v < 0xFF) {
                        drawVert = &scn->vertices[scn->faceBuffer[f].index];
                        DrawBlendedFace(vertPos, vertClrs, *vertCnt, entity->alpha, entity->inkEffect);
                    }

                    vertCnt++;
                }
                break;
        }
    }
}
#else
// some of the old code path
static Vector2 __attribute__((aligned(32))) vertPos[4];
// anything new
static Vector4f __attribute__((aligned(32))) vert3DPos[4];
// face vert colors
static uint32 __attribute__((aligned(32))) vertClrs[4];

// the original lighting code was slow
// it is still slow, but this table along with an approx of normalVal
// really helps make it better
// we also pre-bake the lighting into as many models as possible
// this is rarely used now
static const __attribute__((aligned(32))) uint8_t specLUT[32] = {
    0, 0, 1, 2, 4, 7, 10, 13,
    17, 22, 27, 32, 38, 45, 52, 60,
    68, 77, 86, 96, 107, 117, 129, 141,
    153, 166, 180, 194, 209, 224, 240, 255
};

void RSDK::Draw3DScene(uint16 sceneID)
{
    const float pixelScaleX = RenderDevice::viewSize.x / RenderDevice::pixelSize.x;
    const float pixelScaleY = RenderDevice::viewSize.y / RenderDevice::pixelSize.y;

    // always pull the specular table into cache
    // does not really cost us anything but can save a lot
    __builtin_prefetch(specLUT);

    if (sceneID < SCENE3D_COUNT) {
        Entity *entity = sceneInfo.entity;
        Scene3D *scn   = &scene3DList[sceneID];

        // Setup face buffer.
        // Each face's depth is an average of the depth of its vertices.
        Scene3DVertex *vertices = scn->vertices;
        Scene3DFace *faceBuffer = scn->faceBuffer;
        uint8 *faceVertCounts = scn->faceVertCounts;

        // process any queued strip items for this scene
        for (int itemIdx = 0; itemIdx < stripItemCount; itemIdx++) {
            StripItem &item = stripItems[itemIdx];
            if (item.scene != scn)
                continue;

            Model *mdl = item.model;
            StripTransformedVert *xfVerts = item.verts;

            bool isScreen = (item.drawMode >= S3D_WIREFRAME_SCREEN);
            bool isShaded = (item.drawMode == S3D_SOLIDCOLOR_SHADED ||
                             item.drawMode == S3D_SOLIDCOLOR_SHADED_BLENDED ||
                             item.drawMode == S3D_SOLIDCOLOR_SHADED_SCREEN ||
                             item.drawMode == S3D_SOLIDCOLOR_SHADED_BLENDED_SCREEN ||
                             item.drawMode == S3D_WIREFRAME_SHADED ||
                             item.drawMode == S3D_WIREFRAME_SHADED_SCREEN);

            bool needsTR =
                (entity->inkEffect == INK_ALPHA || entity->inkEffect == INK_ADD ||
                 entity->inkEffect == INK_BLEND || entity->inkEffect == INK_SUB);

            float screenPosXf = (float)(currentScreen->position.x << 16);
            float screenPosYf = (float)(currentScreen->position.y << 16);

            uint32 alphaPrefix;
            if (needsTR) {
                uint32 al = (uint32)entity->alpha;
                // keeps some elements from becoming invisible when they really shouldn't
                if (al < 64)
                    al = 64;
                alphaPrefix = al << 24;
            } else {
                alphaPrefix = 0xFF000000;
            }

            // apply shading + alpha to each vert's color
            // if something is stripped, it is also baked
            // so shading is in strip vertex color
            // just need to apply alpha
            for (int32 v = 0; v < mdl->vertCount; v++) {
                StripTransformedVert *sv = &xfVerts[v];
                uint32 col = sv->color;
                sv->color = alphaPrefix | col;
            }

            // if necessary, prepare and submit PVR poly header for the strip
            if (needsTR) {
                RenderDevice::Prepare3DStripTR(entity->inkEffect);
            } else {
                RenderDevice::Prepare3DStripPT(entity->inkEffect);
            }

            pvr_vertex_t __attribute__((aligned(32))) stripBuf[256];

            // render triangle strips
            uint16 *idx = mdl->stripIndices;
            for (int32 s = 0; s < mdl->stripCount; s++) {
                uint16 len = mdl->stripLengths[s];

                // kkip strips with any near-z verts in screen mode
                if (isScreen) {
                    bool hasNearZ = false;
                    for (int32 v = 0; v < len; v++) {
                        if (xfVerts[idx[v]].z < 0x100) {
                            hasNearZ = true;
                            break;
                        }
                    }
                    if (hasNearZ) {
                        idx += len;
                        continue;
                    }
                }

                for (int32 v = 0; v < len; v++) {
                    StripTransformedVert *sv = &xfVerts[idx[v]];

                    if (isScreen) {
                        // fold common multiply by 256 into z reciprocal
                        float rvz = shz_divf(256.0f, sv->z);
                        stripBuf[v].x = currentScreen->center.x +
                                        sv->x * rvz;
                        stripBuf[v].y = currentScreen->center.y -
                                        sv->y * rvz;
                    } else {
                        stripBuf[v].x = ((sv->x * 256.0f) - screenPosXf) / 65535.0f;
                        stripBuf[v].y = ((sv->y * 256.0f) - screenPosYf) / 65535.0f;
                    }
                    stripBuf[v].x *= pixelScaleX;
                    stripBuf[v].y *= pixelScaleY;
                    stripBuf[v].flags = (v == len - 1) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                    stripBuf[v].z     = shz_divf(65536.0f, sv->z);
                    stripBuf[v].argb  = sv->color;
                    stripBuf[v].oargb = 0;
                }

                if (needsTR)
                    RenderDevice::Draw3DStripTR(stripBuf, len);
                else
                    RenderDevice::Draw3DStripPT(stripBuf, len);
                idx += len;
            }

            // render loose triangles from stripped models
            idx = mdl->looseIndices;
            for (int32 t = 0; t < mdl->looseTriCount; t++) {
                if (isScreen) {
                    if (xfVerts[idx[0]].z < 0x100 || xfVerts[idx[1]].z < 0x100 ||
                        xfVerts[idx[2]].z < 0x100) {
                        idx += 3;
                        continue;
                    }
                }

                for (int32 v = 0; v < 3; v++) {
                    StripTransformedVert *sv = &xfVerts[idx[v]];

                    if (isScreen) {
                        // fold common multiply by 256 into z reciprocal
                        float rvz = shz_divf(256.0f, sv->z);
                        stripBuf[v].x = currentScreen->center.x +
                                        sv->x * rvz;
                        stripBuf[v].y = currentScreen->center.y -
                                        sv->y * rvz;
                    } else {
                        stripBuf[v].x = ((sv->x * 256.0f) - screenPosXf) / 65535.0f;
                        stripBuf[v].y = ((sv->y * 256.0f) - screenPosYf) / 65535.0f;
                    }
                    stripBuf[v].x *= pixelScaleX;
                    stripBuf[v].y *= pixelScaleY;
                    stripBuf[v].flags = (v == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                    stripBuf[v].z     = shz_divf(65536.0f, sv->z);
                    stripBuf[v].argb  = sv->color;
                    stripBuf[v].oargb = 0;
                }

                if (needsTR)
                    RenderDevice::Draw3DStripTR(stripBuf, 3);
                else
                    RenderDevice::Draw3DStripPT(stripBuf, 3);
                idx += 3;
            }
        }
        stripItemCount  = 0;
        stripVertOffset = 0;
        // end of triangle strip rendering

        // "original" face-buffer path (non-stripped models and meshes)
        int32 vertIndex = 0;
        for (int32 i = 0; i < scn->faceCount; ++i) {
            switch (*faceVertCounts) {
                default:
                case 1:
                    faceBuffer->depth = vertices[0].z;
                    vertices += *faceVertCounts;
                    break;

                case 2:
                    faceBuffer->depth = vertices[0].z;
                    faceBuffer->depth += vertices[1].z;
                    faceBuffer->depth >>= 1;
                    vertices += 2;
                    break;

                case 3:
                    faceBuffer->depth = vertices[0].z >> 1;
                    faceBuffer->depth += vertices[1].z;
                    faceBuffer->depth += vertices[2].z;
                    faceBuffer->depth >>= 1;
                    vertices += 3;
                    break;

                case 4:
                    faceBuffer->depth = vertices[0].z;
                    faceBuffer->depth += vertices[1].z;
                    faceBuffer->depth += vertices[2].z;
                    faceBuffer->depth += vertices[3].z;
                    faceBuffer->depth >>= 2;
                    vertices += 4;
                    break;
            }

            faceBuffer->index = vertIndex;
            vertIndex += *faceVertCounts;

            ++faceBuffer;
            ++faceVertCounts;
        }

        Scene3DFace *a = scn->faceBuffer;
        Scene3DVertex *testVert = &scn->vertices[scn->faceBuffer[0].index];
        int isBaked = testVert->isBaked;
        // only require face-sorting for non-baked stuff (renders with z == layerDepth, required for order)
        if (!isBaked) {
            std::sort(a, a + scn->faceCount, [](const Scene3DFace &a, const Scene3DFace &b) {
                return a.depth > b.depth;
            });
        }

        // Finally, display the faces.

        uint8 *vertCnt = scn->faceVertCounts;
        uint32 screenx = (currentScreen->position.x << 16);
        uint32 screeny = (currentScreen->position.y << 16);
        uint32 centerx = (currentScreen->center.x << 16);
        uint32 centery = (currentScreen->center.y << 16);

        switch (scn->drawMode) {
            default: break;

            case S3D_WIREFRAME:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    float z = 0.0f;
                    for (int32 v = 0; v < *vertCnt - 1; v++) {
                        if (isBaked) {
                            z = shz_divf(65536.0f*0.5f, (float)drawVert[v].z);
                        }
                        Draw3DLine(z, drawVert[v + 0].x << 8, drawVert[v + 0].y << 8, drawVert[v + 1].x << 8, drawVert[v + 1].y << 8, drawVert[0].color,
                            entity->alpha, entity->inkEffect, false);
                    }
                    if (isBaked) {
                        z = shz_divf(65536.0f*0.5f, (float)drawVert[0].z);
                    }
                    Draw3DLine(z, drawVert[0].x << 8, drawVert[0].y << 8, drawVert[*vertCnt - 1].x << 8, drawVert[*vertCnt - 1].y << 8, drawVert[0].color,
                        entity->alpha, entity->inkEffect, false);
                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    for (int32 v = 0; v < *vertCnt; ++v) {
                        vert3DPos[v].x = (drawVert[v].x << 8) - screenx;
                        vert3DPos[v].y = (drawVert[v].y << 8) - screeny;
                        if (!isBaked) {
                            vert3DPos[v].z = 0;
                        } else {
                            vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                        }
                    }
                    Draw3DFace(vert3DPos, *vertCnt, (drawVert->color >> 16) & 0xFF, (drawVert->color >> 8) & 0xFF, (drawVert->color >> 0) & 0xFF,
                        entity->alpha, entity->inkEffect);
                    vertCnt++;
                }
                break;

            case S3D_WIREFRAME_SHADED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    float ny1 = 0;
                    for (int32 v = 0; v < vertCount; ++v) {
                        ny1 += drawVert[v].ny;
                    }
                    uint32 color = drawVert->color;
                    if (!isBaked) {
                        // this is a significantly simplified version of the original light model
                        // many assumptions are used (the most common specularIntensity and diffuseIntensity)
                        // also note that these normals are previously scaled by a constant
                        // that lets them approximate what `normalVal` used to be here
                        int32 normal    = (int32)shz_divf(ny1 , (float)vertCount);
                        int32 specularC = specLUT[(int32)normal];
                        float diffuseC  = (normal * DIFFUSE_SCALE) + 0.625f;

                        int32 r = specularC + (((color >> 16) & 0xff) * diffuseC);
                        int32 g = specularC + (((color >>  8) & 0xff) * diffuseC);
                        int32 b = specularC + (((color >>  0) & 0xff) * diffuseC);

                        if (r > 255) r = 255;
                        if (g > 255) g = 255;
                        if (b > 255) b = 255;

                        color = (r << 16) | (g << 8) | b;
                    }
                    float z = 0.0f;
                    for (int32 v = 0; v < vertCount - 1; v++) {
                        if (isBaked)
                            z = shz_divf(65536.0f*0.5f, (float)drawVert[v].z);
                        Draw3DLine(z, drawVert[v + 0].x << 8, drawVert[v + 0].y << 8, drawVert[v + 1].x << 8, drawVert[v + 1].y << 8, color, entity->alpha,
                            entity->inkEffect, false);
                    }
                    if (isBaked)
                        z = shz_divf(65536.0f*0.5f, (float)drawVert[vertCount - 1].z);
                    Draw3DLine(z, drawVert[vertCount - 1].x << 8, drawVert[vertCount - 1].y << 8, drawVert[0].x << 8, drawVert[0].y << 8, color,
                        entity->alpha, entity->inkEffect, false);

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;
                    vertCnt++;

                    float ny = 0;
                    for (int32 v = 0; v < vertCount; ++v) {
                        ny += drawVert[v].ny;
                        vert3DPos[v].x = (drawVert[v].x << 8) - screenx;
                        vert3DPos[v].y = (drawVert[v].y << 8) - screeny;
                        if (!isBaked)
                            vert3DPos[v].z = 0;
                        else
                            vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                    }
                    uint32 color = drawVert->color;
                    int32 r = (color >> 16) & 0xff;
                    int32 g = (color >>  8) & 0xff;
                    int32 b = (color      ) & 0xff;
                    if (!isBaked) {
                        int32 normal    = (int32)shz_divf(ny , (float)vertCount);
                        int32 specularC = specLUT[(int32)normal];
                        float diffuseC  = (normal * DIFFUSE_SCALE) + 0.625f;

                        r = specularC + (r * diffuseC);
                        g = specularC + (g * diffuseC);
                        b = specularC + (b * diffuseC);

                        if (r > 255) r = 255;
                        if (g > 255) g = 255;
                        if (b > 255) b = 255;

                        Draw3DFace(vert3DPos, vertCount, r, g, b, entity->alpha, entity->inkEffect);
                    }
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_BLENDED:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;
                    vertCnt++;

                    for (int32 v = 0; v < vertCount; ++v) {
                        uint32 color = drawVert[v].color;
                        int32 r = (color >> 16) & 0xff;
                        int32 g = (color >>  8) & 0xff;
                        int32 b = (color      ) & 0xff;
                        vert3DPos[v].x = (drawVert[v].x << 8) - screenx;
                        vert3DPos[v].y = (drawVert[v].y << 8) - screeny;
                        if (!isBaked)
                            vert3DPos[v].z = 0;
                        else
                            vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                        if (!isBaked) {
                            int32 normal    = (int32)drawVert[v].ny;
                            int32 specularC = specLUT[(int32)normal];
                            float diffuseC  = (normal * DIFFUSE_SCALE) + 0.625f;

                            r = specularC + (r * diffuseC);
                            g = specularC + (g * diffuseC);
                            b = specularC + (b * diffuseC);

                            if (r > 255) r = 255;
                            if (g > 255) g = 255;
                            if (b > 255) b = 255;
                        }
                        vertClrs[v] = (r << 16) | (g << 8) | b;
                    }
                    Draw3DBlendedFace(vert3DPos, vertClrs, vertCount, entity->alpha, entity->inkEffect);
                }
                break;

            case S3D_WIREFRAME_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount = *vertCnt;

                    int32 v;
                    for (v = 0; v < vertCount && v < 0xFF; ++v) {
                        if (drawVert[v].z < 0x100) {
                            v = 0xFF;
                        }
                    }

                    if (v < 0xFF) {
                        float z = 0.0f;
                        for (v = 0; v < vertCount; ++v) {
                            float rvz = shz_invf((float)drawVert[v].z);
                            vertPos[v].x = currentScreen->center.x + (drawVert[v].x << 8) * rvz;
                            vertPos[v].y = currentScreen->center.y - (drawVert[v].y << 8) * rvz;
                        }
                        for (v = 0; v < vertCount - 1; v++) {
                            if (isBaked)
                                z = shz_divf(65536.0f*0.5f, (float)drawVert[v].z);
                            Draw3DLine(z, vertPos[v + 0].x, vertPos[v + 0].y, vertPos[v + 1].x, vertPos[v + 1].y, drawVert[0].color, entity->alpha,
                                entity->inkEffect, true);
                        }
                        if (isBaked)
                            z = shz_divf(65536.0f*0.5f, (float)drawVert[0].z);
                        Draw3DLine(z, vertPos[0].x, vertPos[0].y, vertPos[vertCount - 1].x, vertPos[vertCount - 1].y, drawVert[0].color, entity->alpha,
                            entity->inkEffect, true);
                    }

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;
                    vertCnt++;

                    int32 v;
                    for (v = 0 ; v < vertCount && v < 0xFF; ++v) {
                        if (drawVert[v].z < 0x100) {
                            v = 0xFF;
                        }
                    }

                    if (v < 0xFF) {
                        for (v = 0; v < vertCount; ++v) {
                            float rvz = shz_divf(65536.0f, (float)drawVert[v].z);
                            vert3DPos[v].x = centerx + ((drawVert[v].x << 8) * rvz);
                            vert3DPos[v].y = centery - ((drawVert[v].y << 8) * rvz);
                            if (!isBaked)
                                vert3DPos[v].z = 0;
                            else
                                vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                        }
                        Draw3DFace(vert3DPos, vertCount, (drawVert[0].color >> 16) & 0xFF, (drawVert[0].color >> 8) & 0xFF,
                            (drawVert[0].color >> 0) & 0xFF, entity->alpha, entity->inkEffect);
                    }
                }
                break;

            case S3D_WIREFRAME_SHADED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;

                    int32 v;
                    float ny1 = 0;
                    for (v = 0 ; v < vertCount && v < 0xFF; ++v) {
                        if (drawVert[v].z < 0x100) {
                            v = 0xFF;
                        }
                    }

                    if (v < 0xFF) {
                        for (v = 0 ; v < vertCount; ++v) {
                            float rvz = shz_invf((float)drawVert[v].z);
                            vertPos[v].x = currentScreen->center.x + (drawVert[v].x << 8) * rvz;
                            vertPos[v].y = currentScreen->center.y - (drawVert[v].y << 8) * rvz;
                            ny1 += drawVert[v].ny;
                        }

                        uint32 color = drawVert[v].color;
                        int32 r = (color >> 16) & 0xff;
                        int32 g = (color >>  8) & 0xff;
                        int32 b = (color      ) & 0xff;
                        if (!isBaked) {
                            int32 normal    = (int32)shz_divf(ny1, (float)vertCount);
                            int32 specularC = specLUT[(int32)normal];
                            float diffuseC  = (normal * DIFFUSE_SCALE) + 0.625f;

                            r = specularC + (r * diffuseC);
                            g = specularC + (g * diffuseC);
                            b = specularC + (b * diffuseC);

                            if (r > 255) r = 255;
                            if (g > 255) g = 255;
                            if (b > 255) b = 255;

                            color = (r << 16) | (g << 8) | (b << 0);
                        }
                        float z = 0 ;
                        for (v = 0; v < vertCount - 1; v++) {
                            if (isBaked)
                                z = shz_divf(65536.0f*0.5f, (float)drawVert[v].z);
                            Draw3DLine(z, vertPos[v + 0].x, vertPos[v + 0].y, vertPos[v + 1].x, vertPos[v + 1].y, color, entity->alpha,
                                entity->inkEffect, true);
                        }
                        if (isBaked)
                            z = shz_divf(65536.0f*0.5f, (float)drawVert[vertCount - 1].z);
                        Draw3DLine(z, vertPos[vertCount - 1].x, vertPos[vertCount - 1].y, vertPos[0].x, vertPos[0].y, color, entity->alpha,
                            entity->inkEffect, true);
                    }

                    vertCnt++;
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;
                    int32 v;
                    float ny = 0;
                    vertCnt++;

                    for (v = 0; v < vertCount && v < 0xFF; ++v) {
                        if (drawVert[v].z < 0x100) {
                            v = 0xFF;
                        }
                    }
                    if (v < 0xFF) {
                        for (v = 0; v < vertCount; ++v) {
                            float rvz = shz_divf(65536.0f, (float)drawVert[v].z);
                            ny += drawVert[v].ny;
                            vert3DPos[v].x = centerx + ((drawVert[v].x << 8) * rvz);
                            vert3DPos[v].y = centery - ((drawVert[v].y << 8) * rvz);
                            if (!isBaked)
                                vert3DPos[v].z = 0;
                            else
                                vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                        }
                        uint32 color = drawVert[0].color;
                        int32 r = (color >> 16) & 0xff;
                        int32 g = (color >>  8) & 0xff;
                        int32 b = (color      ) & 0xff;
                        if (!isBaked) {
                            int32 normal    = (int32)shz_divf(ny, (float)vertCount);
                            int32 specularC = specLUT[(int32)normal];
                            float diffuseC  = (normal * DIFFUSE_SCALE) + 0.625f;

                            r = specularC + (r * diffuseC);
                            g = specularC + (g * diffuseC);
                            b = specularC + (b * diffuseC);

                            if (r > 255) r = 255;
                            if (g > 255) g = 255;
                            if (b > 255) b = 255;
                        }
                        Draw3DFace(vert3DPos, vertCount, r, g, b, entity->alpha, entity->inkEffect);
                    }
                }
                break;

            case S3D_SOLIDCOLOR_SHADED_BLENDED_SCREEN:
                for (int32 f = 0; f < scn->faceCount; ++f) {
                    Scene3DVertex *drawVert = &scn->vertices[scn->faceBuffer[f].index];
                    int32 vertCount         = *vertCnt;
                    vertCnt++;

                    int32 v;
                    for (v = 0; v < vertCount && v < 0xFF; ++v) {
                        if (drawVert[v].z < 0x100) {
                            v = 0xFF;
                        }
                    }
                    if (v < 0xFF) {
                        for (v = 0; v < vertCount; ++v) {
                            float rvz = shz_divf(65536.0f, (float)drawVert[v].z);
                            vert3DPos[v].x = centerx + ((drawVert[v].x << 8) * rvz);
                            vert3DPos[v].y = centery - ((drawVert[v].y << 8) * rvz);
                            if (!isBaked) {
                                vert3DPos[v].z = 0.0f;
                            } else {
                                vert3DPos[v].z = shz_divf(65536.0f, (float)drawVert[v].z);
                            }
                            uint32 color = drawVert[v].color;
                            int32 r = (color >> 16) & 0xff;
                            int32 g = (color >>  8) & 0xff;
                            int32 b = (color      ) & 0xff;
                            if (!isBaked) {
                                int32 normal    = (int32)drawVert[v].ny + 2;
                                if (normal > 31) normal = 31;
                                int32 specularC = specLUT[(int32)normal];
                                float diffuseC  = (normal * DIFFUSE_SCALE) + 0.75f;

                                // fix the lack of color on the pinball stage target bumpers
                                if (scn->diffuseX != scn->diffuseY || scn->diffuseX != scn->diffuseZ) {
                                    r = (scn->diffuseX * r) >> 8;
                                    g = (scn->diffuseY * g) >> 8;
                                    b = (scn->diffuseZ * b) >> 8;
                                }

                                r = specularC + (r * diffuseC);
                                g = specularC + (g * diffuseC);
                                b = specularC + (b * diffuseC);

                                if (r > 255) r = 255;
                                if (g > 255) g = 255;
                                if (b > 255) b = 255;

                                color = (r << 16) | (g << 8) | b;
                            }
                            vertClrs[v] = color;
                        }
                        Draw3DBlendedFace(vert3DPos, vertClrs, vertCount, entity->alpha, entity->inkEffect);
                    }
                }
            break;
        }
    }
}
#endif