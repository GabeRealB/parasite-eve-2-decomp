#include "gameplay/model_lighting.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>
#include <psyq/memory.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "hud_sprites.h"
#include "model_lighting.h"
#include "model_objects.h"
#include "pad_input.h"
#include "gameplay/player_actor.h"
#include "scene_runtime.h"

/// Writable packet word containing vertex 0's texture coordinates and CLUT address.
///
/// `primitive` must point to a live, writable Psy-Q `POLY_FT3`, `POLY_FT4`,
/// `POLY_GT3` or `POLY_GT4` aligned to four bytes. On the little-endian target,
/// the `u32` lvalue spans `u0` (bits 0..7), `v0` (bits 8..15) and `clut`
/// (bits 16..31): two unsigned texel coordinates and the GPU's encoded palette
/// address. The word view copies all four bytes from the model stream together
/// before palette offsets are added. Evaluates `primitive` once and captures
/// no caller variables.
#define MODEL_LIGHTING_UV0_CLUT_WORD(primitive) (*(u32*)&((primitive)->u0))

/// Writable packet word containing vertex 1's texture coordinates and texture-page settings.
///
/// `primitive` must point to a live, writable Psy-Q `POLY_FT3`, `POLY_FT4`,
/// `POLY_GT3` or `POLY_GT4` aligned to four bytes. On the little-endian target,
/// the `u32` lvalue spans `u1` (bits 0..7), `v1` (bits 8..15) and `tpage`
/// (bits 16..31): two unsigned texel coordinates and the GPU's encoded page
/// location, colour depth and semi-transparency mode. The word view copies all
/// four bytes from the model stream together before page offsets are added.
/// Evaluates `primitive` once and captures no caller variables.
#define MODEL_LIGHTING_UV1_TPAGE_WORD(primitive) (*(u32*)&((primitive)->u1))

#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"
#include <psyq/memory.h>
#include <psyq/rand.h>

/// 0x30-byte play-clock work `Gp_InitPlayClock` stores at `Task::work`.
/// `field_0` / `field_4` are `Mc_SaveData[0].state.playTime` split into minutes and
/// seconds. `field_8` snapshots `gDisplayState.gameTick`. `extra` is the
/// +0xC overlay passed to `Gp_ResetHudFx`.
typedef struct _GpIdMap30 {
    /* 0x00 */ s32      field_0;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ s32      field_8;
    /* 0x0C */ GpIdMapC extra;
} GpIdMap30;
STATIC_ASSERT_SIZEOF(GpIdMap30, 0x30);

extern CVECTOR D_80114BA4;

extern CVECTOR D_80114BA8;

/// Unreferenced nonzero word before the stored BSS.
extern u32 D_80114BAC;

static u32* func_8009FCDC(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

static u32* func_8009FD28(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2);

void func_807150F8(s32 arg0);

void func_80715198(void);

CVECTOR D_80114BA4 = { 0, 0, 0, 0 };
CVECTOR D_80114BA8 = { 0, 0, 0, 0 };
/// Unreferenced nonzero word before the stored BSS.
u32 D_80114BAC = 0x10FF2220;

// "Item obtained!"
// "Bonus item!!"

/* r1 = long vector in, r2 = long vector out: r2 = RT * r1 + TR at full
 * 32-bit precision, the input split into three 10/11-bit slices. */
#define gte_RotTransLV(r1, r2) __asm__ volatile( \
    "lw	$14, 0( %0 );"                           \
    "lw	$15, 4( %0 );"                           \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "andi	$12, $12, 0xffff;"                     \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $0;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "addiu	$16, $0, -0x400;"                     \
    "sra	$12, $14, 21;"                          \
    "and	$12, $16, $12;"                         \
    "andi	$13, $14, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "sra	$13, $15, 21;"                          \
    "and	$13, $16, $13;"                         \
    "andi	$16, $15, 0x3ff;"                      \
    "or	$13, $16, $13;"                          \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $13, $16;"                        \
    "sll	$13, $13, 16;"                          \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $12, $16;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $2;"                              \
    "sra	$14, $14, 10;"                          \
    "sra	$15, $15, 10;"                          \
    "andi	$12, $14, 0xffff;"                     \
    "srl	$16, $14, 31;"                          \
    "addu	$12, $16, $12;"                        \
    "andi	$12, $12, 0xffff;"                     \
    "andi	$13, $15, 0xffff;"                     \
    "srl	$16, $15, 31;"                          \
    "addu	$13, $16, $13;"                        \
    "sll	$13, $13, 16;"                          \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $4;"                              \
    "lw	$16, 8( %0 );"                           \
    "addiu	$14, $0, -0x400;"                     \
    "srl	$15, $16, 31;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "mtc2	$12, $1;"                              \
    "sra	$16, $16, 10;"                          \
    "sra	$12, $16, 21;"                          \
    "and	$12, $14, $12;"                         \
    "andi	$13, $16, 0x3ff;"                      \
    "or	$12, $13, $12;"                          \
    "addu	$12, $12, $15;"                        \
    "mtc2	$12, $3;"                              \
    "sra	$16, $16, 10;"                          \
    "addu	$12, $16, $15;"                        \
    "mtc2	$12, $5;"                              \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A480012;"                          \
    "mfc2	$14, $25;"                             \
    "mfc2	$15, $26;"                             \
    "mfc2	$16, $27;"                             \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A40E012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sra	$12, $12, 2;"                           \
    "addu	$16, $12, $16;"                        \
    "nop;"                                       \
    "nop;"                                       \
    ".word 0x4A416012;"                          \
    "mfc2	$12, $25;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$14, $12, $14;"                        \
    "mfc2	$12, $26;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$15, $12, $15;"                        \
    "mfc2	$12, $27;"                             \
    "nop;"                                       \
    "sll	$12, $12, 8;"                           \
    "addu	$16, $12, $16;"                        \
    "sw	$14, 0( %1 );"                           \
    "sw	$15, 4( %1 );"                           \
    "sw	$16, 8( %1 )"                            \
    :                                            \
    : "r"(r1), "r"(r2)                           \
    : "$12", "$13", "$14", "$15", "$16", "memory")

u32* func_8009AF90(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    s32      prev;
    s32      count;
    u32      idx;
    u16*     rec;
    CVECTOR  col;
    u8*      dest;
    u8*      rgb;
    s16*     xy;
    SVECTOR* sv;
    s32      dp;
    s32      flag;
    s32      x;

    col   = gGpColorGrey;
    count = ws->elemCount;
    if (count == 0) {
        return arg2;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)arg2;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                gte_stflg(&ws->gteFlag);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)arg2 >> 3] = ws->gteResult;
            }
            prev = rec[0];
            gte_stsxy(ws->preXformWrite + rec[2] + 4);
            gte_stsxy(ws->preXformWrite + rec[3] + 4);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_ldrgb(&D_80114BA4);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[2]);
            gte_ldrgb(&D_80114BA8);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[3]);
            gte_rtv0();
            gte_stsv(&ws->elemNormal);
            arg2 += ws->elemStride;
            // Blend the primitive colour towards the grey reference by the
            // object's blend value when it is below one.
            dp = ws->obj->shading.colorBlend;
            if (dp < TMD_OBJECT_COLOR_BLEND_ONE) {
                gte_lddp(dp);
                rgb = ws->preXformWrite + rec[2];
                gte_ldcv(rgb);
                gte_gpf12();
                gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - dp);
                gte_ldcv(&col);
                gte_gpl12();
                gte_stcv(rgb);
            }
            xy   = &ws->texCoord.vx;
            flag = 0;
            dest = ws->preXformWrite + rec[2] + 8;
            sv   = &ws->elemNormal;
            gte_lddp(ws->obj->shading.colorBlend >> 9);
            gte_ldsv(sv);
            gte_gpf12();
            gte_stsv(sv);
            // dest[8]/dest[9] are the U/V pair; dest[3] (dest[-6] once dest has
            // been advanced onto the V byte) is the primitive's code byte, set
            // when U had to be pulled back onto the second texture page. xy
            // steps from the screen X to Y alongside dest.
            x  = *xy + 0xA0;
            x -= sv->vx;
            if (x < 0) {
                x = 0;
            } else if (x >= 0x100) {
                x   -= 0x80;
                flag = 1;
                if (x >= 0xC0) {
                    x = 0xBF;
                }
            }
            *dest = x;
            xy++;
            dest++;
            x  = *xy + 0x78;
            x -= sv->vy;
            if (x < 0) {
                x = 0;
            } else if (x >= 0xF0) {
                x = 0xEF;
            }
            *dest    = x;
            dest[-6] = flag;
        } while (ws->elemCount-- > 0);
    }
    return arg2;
}

u32* gpXformStreamVertsOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    s32     prev;
    s32     count;
    u32     idx;
    u16*    rec;
    CVECTOR col;
    CVECTOR col2;
    u8*     dest;
    s32     val;
    s32     inv;

    col    = gGpColorWhite;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = 0x80 - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    count  = ws->elemCount;
    if (count == 0) {
        return stream;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)stream;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                gte_stflg(&ws->gteFlag);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)stream >> 3] = ws->gteResult;
            }
            prev = rec[0];
            dest = ws->preXformWrite + rec[2] + 4;
            gte_stsxy(dest);
            dest = ws->preXformWrite + rec[3] + 4;
            gte_stsxy(dest);
            gte_stsxy(&ws->texCoord);
            gte_ldv0((u8*)ws->normals + (rec[1] & 0xFFF8));
            gte_ldrgb(&col);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[2]);
            gte_ldrgb(&col2);
            gte_nccs();
            gte_strgb(ws->preXformWrite + rec[3]);
            gte_rtv0();
            gte_stsv(&ws->elemNormal);
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    return stream;
}

u32* func_8009B500(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    POLY_GT3* poly;
    u16*      rec;
    u8*       verts;
    u8*       norms;
    CVECTOR   col;
    SVECTOR*  sv;
    s16*      xy;
    u8*       dest;
    u8*       rgb;
    u8*       pCode;
    u8*       pU;
    s32       combined;
    s32       flag;
    s32       x;
    s32       i;
    s32       len;

    poly = (POLY_GT3*)ws->primWrite;
    col  = gGpColorGrey;
    len  = 9;
    if (ws->elemCount-- > 0) {
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(&ws->gteResult);
                if (ws->gteResult > 0) {
                    gte_stsxy3_gt3(&poly[0]);
                    gte_stsxy3_gt3(&poly[1]);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[1]);
                    if (ws->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);
                    }

                    /* Environment-map UVs: each vertex's rotated normal, scaled by the
                     * blend value, offsets its screen position into the reflection
                     * texture. xy steps from X to Y alongside the U/V destination.
                     * A U past the first page wraps onto the second one and
                     * is flagged in the pad byte after that vertex's colour. */
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    combined = 0;
                    xy       = &poly[0].x0;
                    dest     = &poly[0].u0;
                    flag     = 0;
                    sv       = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    gte_rtv1();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x1;
                    dest = &poly[0].u1;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    gte_rtv2();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x2;
                    dest = &poly[0].u2;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    combined |= flag;
                    *dest     = x;
                    dest[-6]  = flag;

                    if (combined == 0) {
                        poly[0].tpage = 0x137;
                    } else {
                        /* The primitive samples the second page: a vertex that
                         * did not wrap is shifted back 0x80, or clamped to 0. */
                        pCode = &poly[0].code;
                        i     = 0;
                        pU    = &poly[0].u0;
                        do {
                            if (*pCode == 0) {
                                if ((s8)*pU < 0) {
                                    *pU = *pU + 0x80;
                                } else {
                                    *pU = 0;
                                }
                            }
                            pU += 0xC;
                            i++;
                            pCode += 0xC;
                        } while (i < 3);
                        poly[0].tpage = 0x139;
                    }
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x36);
                    setlen(&poly[1], len);
                    setcode(&poly[1], 0x34);
                    gte_stotz(&ws->gteResult);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[0]);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[1]);
                }
            }
            poly += 2;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimGt3OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           maskHi;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    CVECTOR       col;
    CVECTOR       col2;
    s32           len;
    s32           code;
    s32           val;
    s32           inv;

    poly   = (POLY_GT3*)ws->primWrite;
    col    = gGpColorGrey;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = 0x80 - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    if (ws->elemCount-- > 0) {
        opz    = &ws->gteResult;
        len    = 9;
        code   = 0x34;
        ds     = &gDisplayState;
        mask   = 0xFFFFFF;
        maskHi = 0xFF000000;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_gt3(&poly[0]);
                    gte_stsxy3_gt3(&poly[1]);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ldrgb(&col);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[0]);
                    gte_ldrgb(&col2);
                    gte_ncct();
                    gte_strgb3_gt3(&poly[1]);
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x36);
                    setlen(&poly[1], len);
                    setcode(&poly[1], code);
                    poly[0].tpage |= 0x20;
                    gte_stotz(opz);
                    poly[0].tag = (poly[0].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                    *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                        (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[0] & mask);
                    poly[1].tag = (poly[1].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                    *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                        (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[1] & mask);
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           mask;
    u32           maskHi;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;
    u8*           norms;
    CVECTOR       col;
    CVECTOR       col2;
    s32           len;
    s32           code;
    s32           val;
    s32           inv;

    poly   = (POLY_GT4*)ws->primWrite;
    col    = gGpColorGrey;
    col2   = gGpColorGrey;
    val    = ws->obj->shading.colorBlend >> 5;
    inv    = 0x80 - val;
    col.b  = val;
    col.g  = val;
    col.r  = val;
    col2.b = inv;
    col2.g = inv;
    col2.r = inv;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(&poly[0]);
                gte_stsxy3_gt4(&poly[1]);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly[0].x3);
                        gte_stsxy2(&poly[1].x3);
                        gte_avsz4();
                        norms = (u8*)ws->normals;
                        gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                        gte_ldrgb(&col);
                        gte_ncct();
                        gte_strgb3_gt4(&poly[0]);
                        gte_ldrgb(&col2);
                        gte_ncct();
                        gte_strgb3_gt4(&poly[1]);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_ldrgb(&col);
                        gte_nccs();
                        gte_strgb(&poly[0].r3);
                        gte_ldrgb(&col2);
                        gte_nccs();
                        gte_strgb(&poly[1].r3);
                        len    = 0xC;
                        code   = 0x3C;
                        ds     = &gDisplayState;
                        mask   = 0xFFFFFF;
                        maskHi = 0xFF000000;
                        setlen(&poly[0], len);
                        setcode(&poly[0], 0x3E);
                        setlen(&poly[1], len);
                        setcode(&poly[1], code);
                        gte_stotz(opz);
                        poly[0].tag = (poly[0].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                        *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                            (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[0] & mask);
                        poly[1].tag = (poly[1].tag & maskHi) | (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & mask);
                        *(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) =
                            (*(&ws->ot[(((((u32)ws->gteResult << ds->otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]) & maskHi) | ((u32)&poly[1] & mask);
                    }
                }
            }
            poly   += 2;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* func_8009C414(TmdStreamWorkspace* ws, s32 arg1, u32* arg2)
{
    POLY_GT4* poly;
    s32*      opz;
    s32*      flg;
    u16*      rec;
    u8*       verts;
    u8*       norms;
    CVECTOR   col;
    SVECTOR*  sv;
    s16*      xy;
    s16*      xy3;
    u8*       rgb3;
    u8*       dest;
    u8*       uv;
    u8*       rgb;
    u8*       codep;
    s32       flag;
    s32       anyflag;
    s32       x;
    s32       i;
    s32       len;

    poly = (POLY_GT4*)ws->primWrite;
    col  = gGpColorGrey;
    if (ws->elemCount-- > 0) {
        flg = &ws->gteFlag;
        opz = &ws->gteResult;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(&poly[0]);
                gte_stsxy3_gt4(&poly[1]);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & TMD_GTE_ERROR_FLAG) == 0) {
                    gte_nclip();
                    gte_stsxy2(&poly[0].x3);
                    gte_stsxy2(&poly[1].x3);
                    if (ws->gteResult <= 0) {
                        *(u_long*)&poly[0].x0 = *(u_long*)&poly[0].x1;
                        *(u_long*)&poly[1].x0 = *(u_long*)&poly[1].x1;
                        gte_stopz(opz);
                        if (ws->gteResult >= 0) {
                            goto skip;
                        }
                    } else {
                        gte_stopz(opz);
                        if (ws->gteResult >= 0) {
                            *(u_long*)&poly[0].x3 = *(u_long*)&poly[0].x2;
                            *(u_long*)&poly[1].x3 = *(u_long*)&poly[1].x2;
                        }
                    }
                    gte_avsz4();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_ncct();
                    gte_strgb3_gt4(&poly[0]);
                    gte_ldrgb(&D_80114BA8);
                    gte_ncct();
                    gte_strgb3_gt4(&poly[1]);

                    /* Environment-map UVs: each vertex's rotated normal, scaled by the
                     * blend value, offsets its screen position into the reflection
                     * texture. xy steps from X to Y while dest steps from U to V, then
                     * back to the pad byte after that vertex's colour, which records
                     * whether U wrapped onto the second texture page. */

                    /* vertex 0 */
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    anyflag = 0;
                    xy      = &poly[0].x0;
                    dest    = &poly[0].u0;
                    flag    = 0;
                    sv      = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 1 */
                    gte_rtv1();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x1;
                    dest = &poly[0].u1;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 2 */
                    gte_rtv2();
                    gte_stsv(&ws->elemNormal);
                    xy   = &poly[0].x2;
                    dest = &poly[0].u2;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest    = x;
                    dest    -= 6;
                    *dest    = flag;
                    anyflag |= flag;

                    /* vertex 3 */
                    xy3 = &poly[0].x3;
                    gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                    gte_ldrgb(&D_80114BA4);
                    gte_nccs();
                    gte_strgb(&poly[0].r3);
                    gte_ldrgb(&D_80114BA8);
                    gte_nccs();
                    gte_strgb(&poly[1].r3);
                    gte_rtv0();
                    gte_stsv(&ws->elemNormal);
                    xy   = xy3;
                    dest = &poly[0].u3;
                    flag = 0;
                    sv   = &ws->elemNormal;
                    gte_lddp(ws->obj->shading.colorBlend >> 9);
                    gte_ldsv(sv);
                    gte_gpf12();
                    gte_stsv(sv);
                    x  = *xy + 0xA0;
                    x -= sv->vx;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0x100) {
                        x   -= 0x80;
                        flag = 1;
                        if (x >= 0xC0) {
                            x = 0xBF;
                        }
                    }
                    *dest = x;
                    xy++;
                    dest++;
                    x  = *xy + 0x78;
                    x -= sv->vy;
                    if (x < 0) {
                        x = 0;
                    } else if (x >= 0xF0) {
                        x = 0xEF;
                    }
                    *dest = x;
                    dest -= 6;
                    *dest = flag;

                    anyflag |= flag;
                    if (ws->obj->shading.colorBlend < TMD_OBJECT_COLOR_BLEND_ONE) {
                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r0;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r1;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb = &poly[0].r2;
                        gte_ldcv(rgb);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb);

                        gte_lddp(ws->obj->shading.colorBlend);
                        rgb3 = &poly[0].r3;
                        gte_ldcv(rgb3);
                        gte_gpf12();
                        gte_lddp(TMD_OBJECT_COLOR_BLEND_ONE - ws->obj->shading.colorBlend);
                        gte_ldcv(&col);
                        gte_gpl12();
                        gte_stcv(rgb3);
                    }

                    codep = &poly[0].code;
                    if (anyflag == 0) {
                        poly[0].tpage = 0x137;
                    } else {
                        i  = 0;
                        uv = &poly[0].u0;
                        do {
                            if (*codep == 0) {
                                if ((s8)*uv < 0) {
                                    *uv = *uv + 0x80;
                                } else {
                                    *uv = 0;
                                }
                            }
                            uv    += 0xC;
                            i     += 1;
                            codep += 0xC;
                        } while (i < 4);
                        poly[0].tpage = 0x139;
                    }

                    len = 0xC;
                    setlen(&poly[0], len);
                    setcode(&poly[0], 0x3E);
                    setlen(&poly[1], len);
                    setcode(&poly[1], 0x3C);
                    gte_stotz(opz);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[0]);
                    addPrim((&ws->ot[(((((u32)ws->gteResult << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)) / sizeof(*ws->ot)]), &poly[1]);
                }
            }
        skip:
            poly += 2;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimGt3ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3*     poly;
    s32*          opz;
    DisplayState* ds;
    u16*          rec;
    u8*           verts;
    u8*           norms;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_ldrgb(stream + 3);
                    gte_stsxy3_gt3(poly);
                    gte_avsz3();
                    norms = (u8*)ws->normals;
                    gte_ldv3(norms + (rec[3] & 0xFFF8), norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8));
                    gte_ncct();
                    gte_strgb3_gt3(poly);
                    setlen(poly, 9);
                    setcode(poly, 0x34);
                    if (ws->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                        setcode(poly, 0x36);
                    }
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpDrawStreamPrimGt4ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4*     poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;
    u8*           norms;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_ldrgb(stream + 4);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        norms = (u8*)ws->normals;
                        gte_ldv3(norms + (rec[4] & 0xFFF8), norms + (rec[5] & 0xFFF8), norms + (rec[6] & 0xFFF8));
                        gte_ncct();
                        gte_strgb3_gt4(poly);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r3);
                        setlen(poly, 0xC);
                        setcode(poly, 0x3C);
                        if (ws->obj->flags & TMD_OBJECT_SEMI_TRANS) {
                            setcode(poly, 0x3E);
                        }
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* func_8009D388(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT3*           poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3();
                    setlen(poly, 7);
                    setcode(poly, 0x25);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D518(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 9);
                        setcode(poly, 0x2D);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D718(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_GT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_gt4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009D900(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_F4*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_f4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 5);
                        setcode(poly, 0x28);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DB00(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_F3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_f3(poly);
                    gte_stflg(flg);
                    if ((ws->gteFlag & clipMask) == 0) {
                        gte_avsz3();
                        setlen(poly, 4);
                        setcode(poly, 0x20);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DCB8(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT3*           poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_ft3(poly);
                    gte_avsz3();
                    setlen(poly, 7);
                    setcode(poly, 0x27);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009DE48(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_FT4*           poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_ft4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        setlen(poly, 9);
                        setcode(poly, 0x2F);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E048(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->normals + (rec[3] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x30);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E274(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G3*            poly;
    s32*                opz;
    DisplayState*       ds;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        opz = &ws->gteResult;
        ds  = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(&ws->gteFlag);
            if (ws->gteFlag >= 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g3(poly);
                    gte_avsz3();
                    gte_ldrgb(arg2 + 3);
                    gte_ldv0((u8*)ws->normals + (rec[3] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r0);
                    gte_ldrgb(arg2 + 4);
                    gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r1);
                    gte_ldrgb(arg2 + 5);
                    gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                    gte_nccs();
                    gte_strgb(&poly->r2);
                    setlen(poly, 6);
                    setcode(poly, 0x32);
                    gte_stotz(opz);
                    addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* func_8009E4A0(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    TmdStreamWorkspace* ws;
    POLY_G4*            poly;
    s32*                opz;
    DisplayState*       ds;
    u32                 clipMask;
    s32*                flg;
    u16*                rec;
    u8*                 verts;

    ws   = arg0;
    poly = (POLY_G4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)arg2;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                gte_stsxy3_g4(poly);
                gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                gte_rtps();
                gte_stflg(flg);
                if ((ws->gteFlag & clipMask) == 0) {
                    if (ws->gteResult > 0) {
                        goto draw;
                    }
                    gte_nclip();
                    gte_stopz(opz);
                    if (ws->gteResult < 0) {
                    draw:
                        gte_stsxy2(&poly->x3);
                        gte_avsz4();
                        gte_ldrgb(arg2 + 4);
                        gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r0);
                        gte_ldrgb(arg2 + 5);
                        gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r1);
                        gte_ldrgb(arg2 + 6);
                        gte_ldv0((u8*)ws->normals + (rec[6] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r2);
                        gte_ldrgb(arg2 + 7);
                        gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                        gte_nccs();
                        gte_strgb(&poly->r3);
                        setlen(poly, 8);
                        setcode(poly, 0x38);
                        gte_stotz(opz);
                        addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                    }
                }
            }
            poly++;
            arg2 += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return arg2;
}

u32* gpDrawStreamPrimG4CornerColorsSemiTrans(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_G4*      poly;
    s32*          opz;
    DisplayState* ds;
    u32           clipMask;
    s32*          flg;
    u16*          rec;
    u8*           verts;

    poly = (POLY_G4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        flg      = &ws->gteFlag;
        clipMask = TMD_GTE_ERROR_FLAG;
        opz      = &ws->gteResult;
        ds       = &gDisplayState;
        do {
            rec   = (u16*)stream;
            verts = (u8*)ws->verts;
            gte_ldv3(verts + (rec[0] & 0xFFF8), verts + (rec[1] & 0xFFF8), verts + (rec[2] & 0xFFF8));
            gte_rtpt();
            gte_stflg(flg);
            if ((ws->gteFlag & clipMask) == 0) {
                gte_nclip();
                gte_stopz(opz);
                if (ws->gteResult > 0) {
                    gte_stsxy3_g4(poly);
                    gte_ldv0((u8*)ws->verts + (rec[3] & 0xFFF8));
                    gte_rtps();
                    gte_stflg(flg);
                    if ((ws->gteFlag & clipMask) == 0) {
                        if (ws->gteResult > 0) {
                            goto draw;
                        }
                        gte_nclip();
                        gte_stopz(opz);
                        if (ws->gteResult < 0) {
                        draw:
                            gte_stsxy2(&poly->x3);
                            gte_avsz4();
                            gte_ldrgb(stream + 4);
                            gte_ldv0((u8*)ws->normals + (rec[4] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r0);
                            gte_ldrgb(stream + 5);
                            gte_ldv0((u8*)ws->normals + (rec[5] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r1);
                            gte_ldrgb(stream + 6);
                            gte_ldv0((u8*)ws->normals + (rec[6] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r2);
                            gte_ldrgb(stream + 7);
                            gte_ldv0((u8*)ws->normals + (rec[7] & 0xFFF8));
                            gte_nccs();
                            gte_strgb(&poly->r3);
                            setlen(poly, 8);
                            setcode(poly, 0x3A);
                            gte_stotz(opz);
                            addPrim(&ws->ot[((u32)ws->gteResult << ds->otDepthShift) >> 4 & 0x3FF], poly);
                        }
                    }
                }
            }
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

void func_8009EA50(s32 arg0)
{
    s32 temp;

    if (arg0 <= 0) {
        arg0 = 0;
        temp = 0x80;
    } else {
        if (arg0 >= 0x100) {
            arg0 = 0xFF;
        }
        temp = (0xFF - arg0) >> 1;
    }

    D_80114BA4.r = D_80114BA4.g = D_80114BA4.b = arg0;
    D_80114BA8.r = D_80114BA8.g = D_80114BA8.b = temp;
}

u32* gpXformStreamVertsUnlit(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    s32  prev;
    s32  count;
    u32  idx;
    u16* rec;

    count = ws->elemCount;
    if (count == 0) {
        return stream;
    }
    prev          = -1;
    ws->elemCount = count + prev;
    if (count > 0) {
        do {
            rec = (u16*)stream;
            idx = rec[0];
            if (idx != prev) {
                gte_ldv0((u8*)ws->verts + (idx & 0xFFF8));
                gte_rtps();
                gte_stsz(&ws->gteResult);
                if (ws->gteFlag & TMD_GTE_ERROR_FLAG) {
                    ws->gteResult |= TMD_VERTEX_DEPTH_INVALID;
                }
                ws->szTable[*(u16*)stream >> 3] = ws->gteResult;
            }
            prev = rec[0];
            gte_stsxy(ws->preXformWrite + rec[1]);
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    return stream;
}

u32* gpStreamPrimGt3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF4PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF3PreXform(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[4];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2                    = (u16)stream[6];
            *(u16*)&poly->u3                    = ((u16*)&stream[6])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[4];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2                    = (u16)stream[6];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[6];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[7];
            *(u16*)&poly->u2                    = (u16)stream[8];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4ElemColor(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[5];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[6];
            *(u16*)&poly->u2                    = (u16)stream[7];
            *(u16*)&poly->u3                    = ((u16*)&stream[7])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4CornerColors(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[8];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[9];
            *(u16*)&poly->u2                    = (u16)stream[10];
            *(u16*)&poly->u3                    = ((u16*)&stream[10])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4OneNormal(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            *(u16*)&poly->u3                    = ((u16*)&stream[5])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4Unlit(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       color;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = stream[2];
            GPU_PRIMITIVE_COLOR_WORD(poly, 1) = stream[3];
            GPU_PRIMITIVE_COLOR_WORD(poly, 2) = stream[4];
            color                             = stream[5];
            setlen(poly, 12);
            setcode(poly, 0x3E);
            GPU_PRIMITIVE_COLOR_WORD(poly, 3)   = color;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[6];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[7];
            *(u16*)&poly->u2                    = (u16)stream[8];
            *(u16*)&poly->u3                    = ((u16*)&stream[8])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimFt3(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_FT3* poly;

    poly = (POLY_FT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimFt4(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_FT4* poly;

    poly = (POLY_FT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF4(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F4* poly;
    s32      color;

    poly = (POLY_F4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 5);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x28);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimF3(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_F3* poly;
    s32      color;

    poly = (POLY_F3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            color = stream[2];
            setlen(poly, 4);
            GPU_PRIMITIVE_COLOR_WORD(poly, 0) = color;
            setcode(poly, 0x20);
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            poly->tpage                        += ws->obj->layerTexturePageOffset;
            // Carry the encoded byte until adding its signed row displacement.
            tmp         = (u8)ws->obj->layerClutRowOffset;
            tpage       = poly->tpage;
            tpage      |= 0x20;
            poly->tpage = tpage;
            poly->clut += (s8)tmp << 6;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3Base(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[3];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[4];
            *(u16*)&poly->u2                    = (u16)stream[5];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4OffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[4];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2                    = (u16)stream[6];
            *(u16*)&poly->u3                    = ((u16*)&stream[6])[1];
            poly->tpage                        += ws->obj->layerTexturePageOffset;
            tmp                                 = (u8)ws->obj->layerClutRowOffset;
            tpage                               = poly->tpage;
            tpage                              |= 0x20;
            poly->tpage                         = tpage;
            poly->clut                         += (s8)tmp << 6;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[4];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2                    = (u16)stream[6];
            *(u16*)&poly->u3                    = ((u16*)&stream[6])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4Base(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->primWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[4];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[5];
            *(u16*)&poly->u2                    = (u16)stream[6];
            *(u16*)&poly->u3                    = ((u16*)&stream[6])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3PreXformFixedLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXformLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            poly->tpage = 0x3F;
            poly->clut  = 0x3C10;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt3PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT3* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT3*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->obj->layerTexturePageOffset;
            tmp                                 = (u8)ws->obj->layerClutRowOffset;
            tpage                               = poly->tpage;
            tpage                              |= 0x20;
            poly->tpage                         = tpage;
            poly->clut                         += (s8)tmp << 6;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimGt4PreXformOffsetLayer(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    POLY_GT4* poly;
    s32       tpage;
    s32       tmp;

    poly = (POLY_GT4*)ws->preXformWrite;
    if (ws->elemCount-- > 0) {
        do {
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->obj->layerTexturePageOffset;
            tmp                                 = (u8)ws->obj->layerClutRowOffset;
            tpage                               = poly->tpage;
            tpage                              |= 0x20;
            poly->tpage                         = tpage;
            poly->clut                         += (s8)tmp << 6;
            poly++;
            MODEL_LIGHTING_UV0_CLUT_WORD(poly)  = stream[2];
            MODEL_LIGHTING_UV1_TPAGE_WORD(poly) = stream[3];
            *(u16*)&poly->u2                    = (u16)stream[4];
            *(u16*)&poly->u3                    = ((u16*)&stream[4])[1];
            poly->tpage                        += ws->texturePageOffset;
            poly->clut                         += ws->encodedClutOffset;
            poly++;
            stream += ws->elemStride;
        } while (ws->elemCount-- > 0);
    }
    ws->preXformWrite = (u8*)poly;
    return stream;
}

u32* gpStreamPrimG4(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    u8* prims;
    s32 stride;

    prims = ws->primWrite;
    if (ws->elemCount-- > 0) {
        stride = ws->elemStride;
        do {
            stream += stride;
            prims  += 0x24;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = prims;
    return stream;
}

u32* gpStreamPrimG3(TmdStreamWorkspace* ws, s32 flags, u32* stream)
{
    u8* prims;
    s32 stride;

    prims = ws->primWrite;
    if (ws->elemCount-- > 0) {
        stride = ws->elemStride;
        do {
            stream += stride;
            prims  += 0x1C;
        } while (ws->elemCount-- > 0);
    }
    ws->primWrite = prims;
    return stream;
}

static u32* func_8009FCDC(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->primWrite;
    if (arg0->elemCount-- > 0) {
        stride = arg0->elemStride;
        do {
            arg2  += stride;
            prims += 0x14;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = prims;
    return arg2;
}

static u32* func_8009FD28(TmdStreamWorkspace* arg0, s32 arg1, u32* arg2)
{
    u8* prims;
    s32 stride;

    prims = arg0->primWrite;
    if (arg0->elemCount-- > 0) {
        stride = arg0->elemStride;
        do {
            arg2  += stride;
            prims += 0x18;
        } while (arg0->elemCount-- > 0);
    }
    arg0->primWrite = prims;
    return arg2;
}

void Gp_ApplyPadReplay(s32 arg0, PadScratch* arg1)
{
    u16 temp_v0;
    u16 temp_v1;
    s32 offset;

    if (arg0 == 1) {
        func_807150F8(1);
        return;
    }

    offset = (u8*)Gp_ReplayCursor - (u8*)Fs_ActorLoadBase2;
    if (gDisplayState.demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
        offset = (u8*)Gp_ReplayCursor - (u8*)0x80600100;
    }
    if (offset <= 0x17FDF) {
        if (GameMain_HaltFlags != 0) {
            arg1->buttons = Gp_ReplayButtons;
            return;
        }
        temp_v1 = Gp_ReplayCursor[0];
        if (temp_v1 != Gp_ReplayButtons) {
            Gp_ReplayButtons    = temp_v1;
            Gp_ReplayFramesLeft = Gp_ReplayCursor[1];
        }
        if (arg1->buttons & 0x800) {
            arg1->buttons        = Gp_ReplayButtons | 0x800;
            Wip_SysFlags.field_4 = 1;
        } else {
            arg1->buttons = Gp_ReplayButtons;
        }
        temp_v0             = Gp_ReplayFramesLeft - 1;
        Gp_ReplayFramesLeft = temp_v0;
        if (!(temp_v0 & 0xFFFF)) {
            u16* next = Gp_ReplayCursor + 2;

            Gp_ReplayButtons = 0xFFFF;
            Gp_ReplayCursor  = next;
            if (*next == 0xFFFF) {
                Wip_SysFlags.field_4    = 0;
                Pad_RemapState->field_8 = 0;
            }
        }
    } else {
        Pad_RemapState->field_8 = 0;
    }
}

void Gp_InitPlayClock(Task* task)
{
    GpIdMap30*    rec;
    DisplayState* ds;

    Gp_UpdatePadInput();
    gGameSession->field_5E = 1;
    rec                    = memCalloc(0x30, 0);
    if (rec == NULL) {
        taskKill(task);
        return;
    }
    Gp_ResetHudFx(&rec->extra);
    GameMain_SetFrameTiming(DISPLAY_TIMING_TWO_VBLANKS);
    task->work   = rec;
    rec->field_0 = Mc_SaveData[0].state.playTime / 60;
    rec->field_4 = Mc_SaveData[0].state.playTime % 60;
    ds           = &gDisplayState;
    rec->field_8 = ds->gameTick;
    func_800B25B0();
    if (ds->demoScene != DISPLAY_DEMO_NONE) {
        srand(1);
        ds->animFrame            = 0;
        gDisplayState.frameCount = 0;
        Gp_LcgState              = 0;
        ds->gameTick             = 0;
        ds->loopCount            = 0;
        ds->vsyncCount           = 0;
        ds->loopTicks            = 0;
        if (ds->demoScene == DISPLAY_DEMO_FIXED_REPLAY) {
            Gp_ReplayCursor = (u16*)0x80600E4C;
        } else {
            Gp_ReplayCursor = (u16*)((u8*)Fs_ActorLoadBase2 + 0xD4C);
        }
        Gp_ReplayButtons        = 0xFFFF;
        Gp_ReplayFramesLeft     = 1;
        Pad_RemapState->field_8 = -1;
    } else if (Pad_RemapState->field_9 == 1) {
        func_80715198();
    }
    task->state++;
}

void Gp_TickPlayClock(Task* task)
{
    TextDrawReq   req;
    u8            buf[0x20];
    GpIdMap30*    rec;
    McSaveData*   save;
    PlayerStatus* cfg;
    GameSession*  session;
    s32           one;
    s32           temp;
    s32           companion;

    rec = (GpIdMap30*)task->work;
    cfg = &Player_Status;
    Gp_UpdatePadInput();

    temp         = gDisplayState.gameTick;
    D_8005ED68  += temp - rec->field_8;
    rec->field_8 = temp;
    if (D_8005ED68 >= 0xE10) {
        McSaveData* p;
        D_8005ED68 -= 0xE10;
        p           = &Mc_SaveData[0];
        if (p->state.playTime <= 0xEA5E) {
            p->state.playTime++;
            rec->field_4++;
            if (rec->field_4 >= 0x3C) {
                rec->field_4 -= 0x3C;
                rec->field_0++;
            }
        } else {
            p->state.playTime = 0xEA5F;
            rec->field_0      = 0x3E7;
            rec->field_4      = 0x3B;
        }
    }

    save = &Mc_SaveData[0];
    one  = 1;
    if (save->state.demoScene == one) {
        req.x          = -0x96;
        req.y          = 0x64;
        req.otIndex    = 4;
        req.colorRgb   = 0x502008;
        req.glyphTable = TEXT_GLYPH_TABLE_LARGE_ALTERNATE;
        req.alignment  = TEXT_ALIGNMENT_LEFT;
        req.drawMode   = one;
        Text_DrawString(&req, Text_ItoaUnsigned(buf, rec->field_0));
        Text_DrawString(&req, ":");
        Text_DrawString(&req, Text_ItoaPadded(buf, rec->field_4, 2));
        Text_DrawString(&req, "'");
        Text_DrawString(&req, Text_ItoaPadded(buf, D_8005ED68 / 60, 2));
        Pad_CheckButtons(one, one, 0x100);
    }

    if (gGameSession->suppressDeathChecks == 0) {
        if (cfg->hp > 0) {
            companion = save->state.companionType;
            if (companion == one) {
                if (save->state.companionHp <= 0) {
                    goto block_hp;
                }
            }
            if (companion != 3) {
                goto block_normal;
            }
            if (save->state.companionHp > 0) {
                goto block_normal;
            }
        block_hp:
            if (cfg->hp > 0) {
                goto block_companion;
            }
        }

        if (gGameSession->eventState != 0) {
            cfg->hp = 1;
            return;
        }
        Gp_StateC08.field_3 = 0;
        func_800A7DE0();
        Gp_PulseState1C80();
        session = gGameSession;
        if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            Gp_LcgState           = Gp_LcgState * 5 + 0x71357911;
            session->deathVariant = ((u32)Gp_LcgState >> 16 & 1) + 1;
            SndEvt_EnqueueType7(0x20000000, 8);
            SndBank_SetEnableFlags(0, 0x20000000);
            CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x1D) & 0xFF, 3);
        }

    block_companion: {
        McSaveData* p;
        p = &Mc_SaveData[0];
        if (p->state.companionHp <= 0) {
            if (gGameSession->eventState != 0) {
                p->state.companionHp = 1;
                return;
            }
            Gp_StateC08.field_3 = 0;
            func_800A7DE0();
            Gp_PulseState1C80();
            companion = p->state.companionType;
            if (companion == 1) {
                gGameSession->restartMode  = companion;
                Gp_LcgState                = Gp_LcgState * 5 + 0x71357911;
                gGameSession->deathVariant = ((u32)Gp_LcgState >> 16 & 1) + 1;
                SndEvt_EnqueueType7(0x20000000, 8);
                SndBank_SetEnableFlags(0, 0x20000000);
                CdCmd_EnqueueLoadFile(9, ((u8)gGameSession->deathVariant + 0x20) & 0xFF, 3);
                companion = p->state.companionType;
            }
            if (companion == 3) {
                gGameSession->restartMode = GAME_SESSION_RESTART_COMPANION_3_DOWN;
            }
        }
    }
        Display_AcquireRef();
        task->killCountdown  = gGameSession->deathRestartDelay;
        Wip_SysFlags.field_1 = 1;
        task->state++;
        return;
    }

block_normal:
    if (gGameSession->restartMode == GAME_SESSION_RESTART_ENDING) {
        Display_AcquireRef();
        gGameSession->deathVariant = 1;
        task->state++;
    } else {
        Gp_HudTask(&rec->extra);
    }
}

void Gp_RestartSessionTask(Task* arg0)
{
    RECT          rect;
    DisplayState* ds;
    GameSession*  session;
    CdCmdQueue*   queue;
    s32           flag;

    queue = &gCdCmdQueue;
    Gp_StartAreaBgm(&arg0->killCountdown);
    arg0->spawnArg1.value += 0xA;
    if (arg0->spawnArg1.value < 0x100) {
        return;
    }
    if (gGameSession->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
        SetDispMask(0);
    }
    SndEvt_EnqueueType2(0, 8);
    SndEvt_EnqueueType7(0x80000000, 0x78);
    SndEvt_EnqueueType7(0x60010001, 0x78);
    flag                  = 0xFF;
    arg0->spawnArg1.value = flag;
    Pad_SetCooldown(0);
    Game_ClearPtrSlots();
    ds               = &gDisplayState;
    ds->stopTaskWalk = 1;
    Task_ResetDefaultList();
    Gpu_ClearOTag(0);
    Gpu_ClearOTag(1);
    Mem_Init();
    CdCmd_ActivatePhase1();
    session                          = gGameSession;
    queue->suppressMoviePresentation = 1;
    if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
        rect.w = 0x140;
        rect.y = 0;
        rect.x = 0;
        rect.h = 0x200;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        ds->control.flags.imageSource = DISPLAY_IMAGE_NONE;
    }
    memset(&gGameSession->location, 0, sizeof(gGameSession->location));
    Mem_ConfigureAuxHeap(0, 0);
    if (gGameSession->restartMode == flag) {
        Gpu_PrimHeapSize   = 0xB000;
        GActiveAuxHeapSize = 0x30000;
        Gpu_PrimHeapBase   = (u8*)Fs_ImgBuffers - 0x35800;
        gMemActiveAuxHeap  = (u8*)Fs_ImgBuffers - 0xA800;
    }
    Mem_Init();
    Mem_InitAux();
    if (gGameSession->restartMode != flag) {
        CdCmd_SetupMdecBuffers();
    }
    Task_SpawnFromTable(&D_8010D1FC, 0, 0, 0);
}
