#ifndef GFX_H
#define GFX_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

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
void Gfx_TransposeRot(MATRIX* arg0, volatile MATRIX* arg1);
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
/// Identity-matrix storage for GsCOORDINATE2.coord (parent at symbol - 4).
extern MATRIX D_80070E44;
/// GsCOORDINATE2 whose `.coord` is `D_80070E94`.
extern GsCOORDINATE2 D_80070E90;
extern MATRIX        D_80070E94;
extern MATRIX        D_80070F14;
extern MATRIX        D_80070F34;

#endif // GFX_H
