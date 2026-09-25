#ifndef GFX_H
#define GFX_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/coord.h"

// =============================================================================
// Types — VRAM/heap image slots (src/main/boot.c)
// =============================================================================

/// 8-byte VRAM/heap slot: pointer + size. Tables selected via Gfx_ImageSlotTables.
typedef struct _GfxImageSlot {
    /* 0x0 */ u_long* pixels;
    /* 0x4 */ s32     size;
} GfxImageSlot;
STATIC_ASSERT_SIZEOF(GfxImageSlot, 0x8);

// =============================================================================
// Functions — boot image slots
// =============================================================================

void Gfx_StoreImageSlot(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void Gfx_LoadImageSlot(s32 arg0, s32 arg1, s32 arg2);

// =============================================================================
// Functions — graph / coordinate trees (src/main/gamemain.c)
// =============================================================================

void Gfx_InitGraph(void);
void Gfx_InitCoordinateTrees(void);

// =============================================================================
// Functions — flat lights (src/main/gfxlight.c)
// =============================================================================

void Gpu_InitDefaultLights(void);
void Gfx_SetFlatLight(s32 id, GsF_LIGHT* light, MATRIX* dirMtx, MATRIX* colorMtx);
void Gfx_SetDefaultFlatLight(s32 id, GsF_LIGHT* light);
void Gfx_SetLightAmbient(long arg0, long arg1, long arg2);

// =============================================================================
// Functions — rotation matrices (src/main/gfxmtx.c)
// =============================================================================

void Gfx_RotMatrixXYZ(MATRIX* out, SVECTOR* angles, s32 flag);
void Gfx_RotMatrixYXZ(MATRIX* out, SVECTOR* angles, s32 flag);
void Gfx_RotMatrixZYX(MATRIX* out, SVECTOR* angles, s32 flag);
void Gfx_RotMatrixX(MATRIX* arg0, s32 angle, s32 flag);
void Gfx_RotMatrixY(MATRIX* arg0, s32 angle, s32 flag);
void Gfx_RotMatrixZ(MATRIX* arg0, s32 angle, s32 flag);
void Gfx_MatrixToEuler(MATRIX* arg0, SVECTOR* arg1);
void Gfx_TransposeRot(MATRIX* arg0, MATRIX* arg1);
void Gfx_MatrixCol0(MATRIX* arg0, volatile SVECTOR* arg1);
void Gfx_MatrixCol1(MATRIX* arg0, volatile SVECTOR* arg1);
void Gfx_MatrixCol2(MATRIX* arg0, volatile SVECTOR* arg1);
void Gfx_NormalizeLightDir(VECTOR* light, SVECTOR* out);
void Gfx_OrthonormalBasis(MATRIX* out, SVECTOR* arg1, SVECTOR* arg2);
s32  Gfx_ApplyMatrixNoSf(SVECTOR* arg0, SVECTOR* arg1);

// =============================================================================
// Globals
// =============================================================================

extern GfxImageSlot* Gfx_ImageSlotTables[];
/// Color/light matrix written by Gfx_SetDefaultFlatLight / Gfx_SetLightAmbient.
extern MATRIX D_80074080;
/// The view rotation: the coordinate `gGfxViewCoord` hangs off, whose `coord`
/// holds the view's rotation, parented in turn to `Gfx_ViewOffsetCoord`.
extern GpCoord gGfxViewRotCoord;
/// The root of the view chain, whose `coord` offsets the view along z.
extern GpCoord Gfx_ViewOffsetCoord;
/// The view coordinate: every world-space object is parented to it, so a
/// coordinate composed against it comes out in view space.
///
/// Its `coord` carries the view translation and its `workm` the view matrix the
/// world is drawn and projected through. The view rotation and the view offset
/// are the two coordinates above it in the chain, which is why its own matrix
/// holds a translation alone.
extern GpCoord gGfxViewCoord;

/// Two neighbouring elements of a matrix's rotation, `m[r][c]` and the one after
/// it, written or read as one word; code sets and copies rotations this way.
/// Build a value with `MATRIX_PAIR_VALUE`.
#define MATRIX_PAIR(mat, r, c) (*(s32*)&(mat)->m[r][c])
/// A `MATRIX_PAIR` word from its two elements, the first in the low half.
#define MATRIX_PAIR_VALUE(first, second) ((u16)(first) | ((s32)(second) << 16))
/// The leading rotation entries of a `MATRIX`, paired into words, as code
/// resets and copies a rotation.
typedef struct _GpMtxWords {
    s32 m00_m01;
    s32 m02_m10;
    s32 m11_m12;
    s32 m20_m21;
    s16 m22;
} GpMtxWords;

/// Sets a matrix's rotation to identity in five word stores; the translation
/// is left alone.
static __inline__ void gfxSetRotIdentity(MATRIX* m)
{
    GpMtxWords* w = (GpMtxWords*)m;

    w->m00_m01 = ONE;
    w->m02_m10 = 0;
    w->m11_m12 = ONE;
    w->m20_m21 = 0;
    w->m22     = ONE;
}

/// A matrix's translation as a vector.
#define MATRIX_TRANS(mat) ((VECTOR3*)(mat)->t)

#endif // GFX_H
