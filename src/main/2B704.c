#include "common.h"

#include "main/unknown_syms.h"

INCLUDE_ASM("main/nonmatchings/2B704", func_8003AF04);

// Layout matches PsyQ GsF_LIGHT (libgs.h is avoided: it redeclares GsClearOt).
typedef struct {
    /* 0x00 */ s32 vx;
    /* 0x04 */ s32 vy;
    /* 0x08 */ s32 vz;
    /* 0x0C */ u8  r;
    /* 0x0D */ u8  g;
    /* 0x0E */ u8  b;
} FlatLight;

typedef struct {
    u8      pad[0x10];
    SVECTOR dir;
} ScratchLightBlock;

extern void   func_8003CD78(VECTOR* light, SVECTOR* out);
extern MATRIX GsLIGHTWSMATRIX;

void func_8003B140(s32 id, FlatLight* light, MATRIX* dirMtx, MATRIX* colorMtx)
{
    void**             scratch;
    ScratchLightBlock* block;
    void*              head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (ScratchLightBlock*)((u8*)head - 0x18);
    *scratch = block;
    func_8003CD78((VECTOR*)light, (SVECTOR*)((u8*)head - 8));

    dirMtx->m[id][0] = -block->dir.vx;
    dirMtx->m[id][1] = -block->dir.vy;
    dirMtx->m[id][2] = -block->dir.vz;

    colorMtx->m[0][id] = light->r << 4;
    colorMtx->m[1][id] = light->g << 4;
    colorMtx->m[2][id] = light->b << 4;

    *scratch = (u8*)*scratch + 0x18;
}

static __inline__ void setLightToMatrices(s32 id, FlatLight* light, MATRIX* dirMtx, MATRIX* colorMtx)
{
    void**             scratch;
    ScratchLightBlock* block;
    void*              head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (ScratchLightBlock*)((u8*)head - 0x18);
    *scratch = block;
    func_8003CD78((VECTOR*)light, (SVECTOR*)((u8*)head - 8));

    dirMtx->m[id][0] = -block->dir.vx;
    dirMtx->m[id][1] = -block->dir.vy;
    dirMtx->m[id][2] = -block->dir.vz;

    colorMtx->m[0][id] = light->r << 4;
    colorMtx->m[1][id] = light->g << 4;
    colorMtx->m[2][id] = light->b << 4;

    *scratch = (u8*)*scratch + 0x18;
}

void func_8003B228(s32 id, FlatLight* light)
{
    setLightToMatrices(id, light, &GsLIGHTWSMATRIX, &D_80074080);
}

void func_8003B318(long arg0, long arg1, long arg2)
{
    D_80074080.t[0] = arg0;
    D_80074080.t[1] = arg1;
    D_80074080.t[2] = arg2;
}
