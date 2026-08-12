#include "common.h"

#include "main/gfx.h"
#include "main/mem.h"

typedef struct {
    u8      pad[0x10];
    SVECTOR dir;
} ScratchLightBlock;

static __inline__ void setLightToMatrices(s32 id, GsF_LIGHT* light, MATRIX* dirMtx, MATRIX* colorMtx)
{
    void**             scratch;
    ScratchLightBlock* block;
    void*              head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (ScratchLightBlock*)((u8*)head - 0x18);
    *scratch = block;
    Gfx_NormalizeLightDir((VECTOR*)light, (SVECTOR*)((u8*)head - 8));

    dirMtx->m[id][0] = -block->dir.vx;
    dirMtx->m[id][1] = -block->dir.vy;
    dirMtx->m[id][2] = -block->dir.vz;

    colorMtx->m[0][id] = light->r << 4;
    colorMtx->m[1][id] = light->g << 4;
    colorMtx->m[2][id] = light->b << 4;

    *scratch = (u8*)*scratch + 0x18;
}

void Gpu_InitDefaultLights(void)
{
    GsF_LIGHT light0;
    GsF_LIGHT light1;
    GsF_LIGHT light2;
    s32       c100;
    s32       c20;

    c100 = 0x64;
    c20  = 0x14;

    light0.vx = c100;
    light0.vy = c100;
    light0.vz = c100;
    light0.r  = 0xD0;
    light0.g  = 0xD0;
    light0.b  = 0xD0;
    setLightToMatrices(0, &light0, &GsLIGHTWSMATRIX, &D_80074080);

    light1.vy = -0x32;
    light1.vz = -0x64;
    light1.vx = c20;
    light1.r  = 0x80;
    light1.g  = 0x80;
    light1.b  = 0x80;
    setLightToMatrices(1, &light1, &GsLIGHTWSMATRIX, &D_80074080);

    light2.vx = -0x14;
    light2.vy = c20;
    light2.vz = c100;
    light2.r  = 0x60;
    light2.g  = 0x60;
    light2.b  = 0x60;
    setLightToMatrices(2, &light2, &GsLIGHTWSMATRIX, &D_80074080);

    D_80074080.t[0] = 0x40;
    D_80074080.t[1] = 0x40;
    D_80074080.t[2] = 0x40;
}

void Gfx_SetFlatLight(s32 id, GsF_LIGHT* light, MATRIX* dirMtx, MATRIX* colorMtx)
{
    void**             scratch;
    ScratchLightBlock* block;
    void*              head;

    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    block    = (ScratchLightBlock*)((u8*)head - 0x18);
    *scratch = block;
    Gfx_NormalizeLightDir((VECTOR*)light, (SVECTOR*)((u8*)head - 8));

    dirMtx->m[id][0] = -block->dir.vx;
    dirMtx->m[id][1] = -block->dir.vy;
    dirMtx->m[id][2] = -block->dir.vz;

    colorMtx->m[0][id] = light->r << 4;
    colorMtx->m[1][id] = light->g << 4;
    colorMtx->m[2][id] = light->b << 4;

    *scratch = (u8*)*scratch + 0x18;
}

void Gfx_SetDefaultFlatLight(s32 id, GsF_LIGHT* light)
{
    setLightToMatrices(id, light, &GsLIGHTWSMATRIX, &D_80074080);
}

void Gfx_SetLightAmbient(long arg0, long arg1, long arg2)
{
    D_80074080.t[0] = arg0;
    D_80074080.t[1] = arg1;
    D_80074080.t[2] = arg2;
}
