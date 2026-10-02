#include "linked_actors.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "ending.h"
#include "gameplay/enemy.h"
#include "hud.h"
#include "hud_sprites.h"
#include "gameplay/scene.h"
#include "world_collision.h"
#include "world_targets.h"

/// Draws one of the prompt's button labels on line `line`, `dx` pixels right of
/// the prompt's left edge.
#define DRAW_PROMPT_LABEL(req, dx, line, color, str)                            \
    {                                                                           \
        req.x          = obj.panel.contentOriginX.unsignedValue + (dx) + xBase; \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        req.colorRgb   = (color);                                               \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_LEFT;                                   \
        req.drawMode   = TEXT_DRAW_OUTLINED;                                    \
        Text_DrawString(&req, (str));                                           \
    }

/// Draws a quantity right-aligned on line `line`; an empty count sets `flag`.
#define DRAW_PROMPT_COUNT(req, line, count)                                     \
    {                                                                           \
        req.colorRgb   = 0x606060;                                              \
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;                                \
        req.alignment  = TEXT_ALIGNMENT_RIGHT;                                  \
        req.drawMode   = TEXT_DRAW_FILL_ONLY;                                   \
        req.x          = obj.panel.contentOriginX.unsignedValue + 0x94;         \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        Text_DrawString(&req, Text_ItoaSigned(buf, (count)));                   \
        if ((count) == 0) {                                                     \
            flag = 1;                                                           \
        }                                                                       \
    }

#include "gameplay/damage.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/gfxgte.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"

/// 0x60-byte scratch from the scratch stack used by `Gp_DrawAimCircle` to draw the
/// wireframe targeting sphere. `vec` is the point being rotated / projected,
/// `mat` the rotation loaded into the GTE, `rx` / `ry` the two radii taken from
/// the caller and `radius` the per-ring radius derived from them. `dp` / `flag`
/// / `otz` / `sxy` receive `gte_stdp` / `gte_stflg` / `gte_stszotz` /
/// `gte_stsxy` of each RTPS, and `sxyPrev` keeps the previous point so the two
/// form a `LINE_F2`. `trans` is the GTE translation vector (`gte_SetTransVector`).
typedef struct _GpCircleScratch {
    /* 0x00 */ SVECTOR vec;
    /* 0x08 */ MATRIX  mat;
    /* 0x28 */ s32     rx;
    /* 0x2C */ s32     ry;
    /* 0x30 */ s32     radius;
    /* 0x34 */ s32     dp;
    /* 0x38 */ s32     flag;
    /* 0x3C */ s32     otz;
    /* 0x40 */ DVECTOR sxyPrev;
    /* 0x44 */ DVECTOR sxy;
    /* 0x48 */ byte    pad_48[8];
    /* 0x50 */ VECTOR  trans;
} GpCircleScratch;
STATIC_ASSERT_SIZEOF(GpCircleScratch, 0x60);

// The image stores this head alone in the linked_actors BSS subsegment.
WorldTargetNode* gWorldTargetListHead;

static __inline__ void Gp_RingPointXZ(GpCircleScratch* sc, s32 ang);

static __inline__ void Gp_ProjectRingPt(GpCircleScratch* sc);

static __inline__ void Gp_LinkRingSeg(GpCircleScratch* sc);

/// Draws `val`, clamped at zero, as a right-aligned number at (`x`, `y`).
static inline void _gpDrawHudValue(s32 x, s32 y, s32 color, s32 val);

/// Draws the "HP" and "MP" captions relative to `obj`'s origin and draw order.
static inline void _gpDrawHudLabels(UiObject* obj, s32 x, s32 y, s32 color);

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void func_800A4904(s32 arg0)
{
    WorldTargetNode* node;
    Enemy*           enemy;
    Enemy*           claim;
    u16              val;
    s32              idx;

    for (node = gWorldTargetListHead; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
            enemy = GP_NODE_ENEMY(node);
            claim = enemy;
            if (arg0 == 0) {
                enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
            } else {
                val  = Gp_StateC08.attachId;
                idx  = (val / 100U - 1) * 9;
                idx += ((val % 100U) / 10U - 1) * 3;
                idx += val % 10U;
                idx += 0x28000;
                Gp_ClaimSlot18(claim, idx);
            }
        }
    }
}

static __inline__ void Gp_RingPointXZ(GpCircleScratch* sc, s32 ang)
{
    sc->vec.vx = (sc->radius * rcos(ang)) >> 12;
    sc->vec.vz = (sc->radius * rsin(ang)) >> 12;
}

static __inline__ void Gp_ProjectRingPt(GpCircleScratch* sc)
{
    gte_ldv0(&sc->vec);
    gte_rtps();
    gte_stsxy(&sc->sxy);
    gte_stdp(&sc->dp);
    gte_stflg(&sc->flag);
    gte_stszotz(&sc->otz);
}

static __inline__ void Gp_LinkRingSeg(GpCircleScratch* sc)
{
    LINE_F2* prim;

    prim                              = gGpuPrimCursor;
    gGpuPrimCursor                    = prim + 1;
    GPU_PRIMITIVE_COLOR_WORD(prim, 0) = GPU_PACK_COLOR_WORD(0, 0xc0, 0x40, 0);
    GPU_PRIMITIVE_XY_WORD(prim, 0)    = *(u32*)&sc->sxyPrev;
    GPU_PRIMITIVE_XY_WORD(prim, 1)    = *(u32*)&sc->sxy;
    setlen(prim, 3);
    setcode(prim, 0x40);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)sc->otz << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
}

void Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*            slot;
    GfxCoord*        coord;
    GfxCoord*        other;
    GpCircleScratch* sc;
    s32              base;
    s32              limit;
    s32              ang;
    s32              i;
    s32              t;
    s32              pass;

    slot   = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    sc     = SCRATCH_STACK_RESERVE_BLOCK(GpCircleScratch);
    coord  = slot->extra.tmd->coords;
    sc->rx = arg1;
    sc->ry = arg2;
    base   = gDisplayState.animFrame << 4;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    if (arg3 & 4) {
        other      = &slot->extra.tmd->coords[4];
        sc->vec.vx = 0;
        sc->vec.vy = 0x12C;
        sc->vec.vz = 0;
        _gfxLoadRotSv(&coord->workm, &sc->vec);
        gte_rtv0();
        gte_stsv(&sc->vec);
        sc->trans.vx = other->workm.t[0] + sc->vec.vx;
        sc->trans.vy = other->workm.t[1] + sc->vec.vy;
        sc->trans.vz = other->workm.t[2] + sc->vec.vz;
        gte_SetTransVector(&sc->trans);
    } else if ((arg3 & 2) == 0) {
        gte_SetTransMatrix(&coord->workm);
    } else {
        arg3      &= ~2;
        sc->vec.vx = 0;
        sc->vec.vy = 0;
        sc->vec.vz = arg1;
        _gfxLoadRotSv(&coord->workm, &sc->vec);
        gte_rtv0();
        gte_stsv(&sc->vec);
        sc->trans.vx = coord->workm.t[0] + sc->vec.vx;
        sc->trans.vy = coord->workm.t[1] + sc->vec.vy;
        sc->trans.vz = coord->workm.t[2] + sc->vec.vz;
        gte_SetTransVector(&sc->trans);
    }

    if (arg3 & 4) {
        sc->mat = coord->workm;
        gfxRotMatrixX(&sc->mat, -0x400, GRAPHICS_ROTATION_COMPOSE);
        arg3 &= ~4;
    } else {
        sc->mat = gGfxViewCoord.workm;
    }
    gte_SetRotMatrix(&sc->mat);

    limit = 0x400;
    if (arg3 == 0) {
        limit = 0x300;
    }

    for (i = 0; i < 12; i++) {
        ang = base + ((i << 12) / 12);
        for (t = 0; t <= limit; ang += 0x73, t += 0x80) {
            if (arg3 == 0) {
                sc->radius = (sc->rx * rcos(t)) >> 12;
                sc->vec.vy = -(sc->ry * rsin(t)) >> 12;
                Gp_RingPointXZ(sc, ang);
            } else {
                sc->vec.vy = -(sc->ry * t) >> 10;
                sc->vec.vx = (sc->rx * rcos(ang)) >> 12;
                sc->vec.vz = (sc->rx * rsin(ang)) >> 12;
            }
            Gp_ProjectRingPt(sc);
            if (t > 0) {
                Gp_LinkRingSeg(sc);
            }
            *(u32*)&sc->sxyPrev = *(u32*)&sc->sxy;
        }
    }

    base = -base;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            if (arg3 == 0) {
                sc->radius = (sc->rx * rcos(0x300)) >> 12;
                sc->vec.vy = -(sc->ry * rsin(0x300)) >> 12;
            } else {
                sc->radius = sc->rx;
                sc->vec.vy = -(u16)sc->ry;
            }
        } else {
            sc->vec.vy = 0;
            sc->radius = sc->rx;
        }
        for (i = 0; i < 25; i++) {
            ang = base + ((i << 12) / 24);
            Gp_RingPointXZ(sc, ang);
            Gp_ProjectRingPt(sc);
            if (i != 0) {
                Gp_LinkRingSeg(sc);
            }
            *(u32*)&sc->sxyPrev = *(u32*)&sc->sxy;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(0x60);
}

void Gp_InitSlot18(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR*         vec;
    WorldTargetNode* node;
    Enemy*           enemy;
    u16              val;
    s32              idx;
    s32              ry2;
    s32              rx2;

    if (arg0 == 0) {
        if (arg3 == 0) {
            Gp_DrawAimCircle(0, arg1, arg2, 0);
        } else {
            Gp_DrawAimCircle(0, arg1, arg2, 2);
        }
    }

    arg2 += 0x64;
    arg1 += 0x64;
    ry2   = (arg2 * arg2) >> 8;
    rx2   = (arg1 * arg1) >> 8;
    node  = gWorldTargetListHead;
    SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    vec = SCRATCH_STACK_CURSOR(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
                vec->vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                vec->vy = GP_NODE_ENEMY(node)->playerRelPos.vy;
                vec->vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (arg3 != 0) {
                    vec->vz -= arg1;
                }
                if (vec->vy >= -arg2 && vec->vy < 0x65 && vec->vx >= -arg1 && vec->vx <= arg1 && vec->vz >= -arg1 &&
                    vec->vz <= arg1) {
                    vec->vx >>= 4;
                    vec->vy >>= 4;
                    vec->vz >>= 4;
                    if ((u32)(ry2 * (vec->vx * vec->vx + vec->vz * vec->vz) + rx2 * (vec->vy * vec->vy)) <=
                        (u32)(ry2 * rx2)) {
                        enemy = GP_NODE_ENEMY(node);
                        if (arg0 == 0) {
                            enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
                        } else {
                            val  = Gp_StateC08.attachId;
                            idx  = (val / 100U - 1) * 9;
                            idx += ((val % 100U) / 10U - 1) * 3;
                            idx += val % 10U;
                            idx += 0x28000;
                            Gp_ClaimSlot18(enemy, idx);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

void func_800A5574(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    SVECTOR*         vec;
    WorldTargetNode* node;
    Enemy*           enemy;
    Enemy*           claim;
    u16              val;
    s32              idx;
    s32              t;
    void*            work;

    if (arg0 == 0) {
        if (arg3 == 0) {
            Gp_DrawAimCircle(0, arg1, arg2, 1);
        } else {
            Gp_DrawAimCircle(0, arg1, arg2, 3);
        }
    }

    arg1 += 0x64;
    arg2 += 0x64;
    node  = gWorldTargetListHead;
    SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    vec = SCRATCH_STACK_CURSOR(SVECTOR);

    if (node != NULL) {
        do {
            if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
                vec->vx = (u16)GP_NODE_ENEMY(node)->playerRelPos.vx;
                vec->vy = (u16)GP_NODE_ENEMY(node)->playerRelPos.vy;
                vec->vz = (u16)GP_NODE_ENEMY(node)->playerRelPos.vz;
                if (arg3 != 0) {
                    vec->vz -= arg1;
                }
                t = vec->vy;
                if (t < 0x65 && t >= -arg2) {
                    if ((u32)(vec->vx * vec->vx + vec->vz * vec->vz) <= (u32)(arg1 * arg1)) {
                        work  = GP_NODE_ENEMY(node);
                        enemy = work;
                        claim = work;
                        if (arg0 == 0) {
                            enemy->colorMode |= ENEMY_COLOR_HIT_FLASH;
                        } else {
                            val  = Gp_StateC08.attachId;
                            idx  = (val / 100U - 1) * 9;
                            idx += ((val % 100U) / 10U - 1) * 3;
                            idx += val % 10U;
                            idx += 0x28000;
                            Gp_ClaimSlot18(claim, idx);
                        }
                    }
                }
            }
            node = node->next;
        } while (node != NULL);
    }

    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}

/// Draws `val`, clamped at zero, as a right-aligned number at (`x`, `y`).
static inline void _gpDrawHudValue(s32 x, s32 y, s32 color, s32 val)
{
    u8          buf[0x10];
    TextDrawReq req;

    if (val < 0) {
        val = 0;
    }
    req.x          = x;
    req.y          = y;
    req.otIndex    = -2;
    req.colorRgb   = color;
    req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
    req.alignment  = TEXT_ALIGNMENT_RIGHT;
    req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
    Text_DrawString(&req, Text_ItoaUnsigned(buf, val));
}

/// Draws the "HP" and "MP" captions relative to `obj`'s origin and draw order.
static inline void _gpDrawHudLabels(UiObject* obj, s32 x, s32 y, s32 color)
{
    TextDrawReq hpReq;
    TextDrawReq mpReq;

    hpReq.x          = obj->panel.contentOriginX.unsignedValue + 4 + x;
    hpReq.y          = obj->panel.contentOriginY.unsignedValue + 8 + y;
    hpReq.otIndex    = obj->panel.otIndex.signedValue + 1;
    hpReq.colorRgb   = color;
    hpReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    hpReq.alignment  = TEXT_ALIGNMENT_LEFT;
    hpReq.drawMode   = TEXT_DRAW_OUTLINED;
    Text_DrawString(&hpReq, Gp_StrHP);

    mpReq.x          = obj->panel.contentOriginX.unsignedValue + 0x2E + x;
    mpReq.y          = obj->panel.contentOriginY.unsignedValue + 8 + y;
    mpReq.otIndex    = obj->panel.otIndex.signedValue + 1;
    mpReq.colorRgb   = color;
    mpReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    mpReq.alignment  = TEXT_ALIGNMENT_LEFT;
    mpReq.drawMode   = TEXT_DRAW_OUTLINED;
    Text_DrawString(&mpReq, Gp_StrMP);
}

void func_800A57B0(GpIdMapC* arg0)
{
    GameDebugState* debugState;
    s32             pendingMp;
    TILE*           tile;
    SPRT *          sp1, *sp3, *sp5;
    POLY_FT4 *      poly1, *poly2;
    DR_TPAGE*       tp;
    PlayerStatus*   cfg;
    s32             pendingHp;
    s32             y;
    s32             hp;
    s32             mp;
    s32             color;
    s32             rectMode;
    s32             iconX, iconY;
    u16*            flags;
    s32             x;
    s32             w1;
    s32             w2;
    s32             i;
    HudHpMp*        hudHpMp;

    cfg        = &gPlayerStatus;
    debugState = Pad_RemapState;
    pendingHp  = 0;
    pendingMp  = 0;
    if (debugState->hideHud != 0) {
        return;
    }

    if (cfg->hp < Gp_HpMpWork.hp) {
        Gp_HpMpWork.hp = Gp_HpMpWork.hp - 1;
    } else if (Gp_HpMpWork.hp < cfg->hp) {
        Gp_HpMpWork.hp = Gp_HpMpWork.hp + 1;
    }
    hudHpMp = &Gp_HpMpWork;
    if (cfg->mp < hudHpMp->mp) {
        hudHpMp->mp = hudHpMp->mp - 1;
    } else if (hudHpMp->mp < cfg->mp) {
        hudHpMp->mp = hudHpMp->mp + 1;
    }

    x  = -0x98;
    y  = -0x64;
    y -= gDisplayState.vramYOffset;
    if (gGameSession->hudShakeY > 0) {
        y -= gGameSession->hudShakeY * 3;
    }

    if (cfg->statusFlags & PLAYER_STATUS_BERSERKER) {
        pendingHp = arg0->field_10 << 1;
    } else {
        pendingMp = arg0->field_10;
    }

    color = 0x606060;
    hp    = Gp_HpMpWork.hp;
    mp    = Gp_HpMpWork.mp;

    _gpDrawHudValue(x + 0x2B, y + 0xA, color, cfg->hp);
    _gpDrawHudValue(x + 0x56, y + 0xA, color, cfg->mp);

    {
        UiObject obj;

        obj.panel.otIndex.signedValue          = -3;
        obj.panel.contentOriginX.unsignedValue = 0;
        obj.panel.contentOriginY.unsignedValue = 0;
        obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;
        _gpDrawHudLabels(&obj, x, y, color);
    }

    if (hp > 0) {
        if (cfg->hpMax > 0) {
            if (hp >= pendingHp) {
                w1 = (hp - pendingHp) * 0x25 / cfg->hpMax;
                if (w1 >= 0x26) {
                    w1 = 0x25;
                } else if (w1 < 0) {
                    w1 = 0;
                }
            } else {
                w1 = 0;
            }
            w2 = hp * 0x25 / cfg->hpMax;
            if (w2 >= 0x26) {
                w2 = 0x25;
            }
            if (w1 > 0) {
                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                tile->x0       = x + 5;
                tile->y0       = y + 0xE;
                tile->h        = 2;
                setlen(tile, 3);
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0);
                setcode(tile, 0x60);
                tile->w = w1;
                addPrim(gGpuCurrentOt - 2, tile);
            }
            if (w2 - w1 > 0) {
                tile           = gGpuPrimCursor;
                gGpuPrimCursor = tile + 1;
                {
                    s32 tileX = w1 + 5;
                    tile->x0  = x + tileX;
                }
                tile->y0                          = y + 0xE;
                tile->h                           = 2;
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0xff, 0xff, 0, 0);
                setlen(tile, 3);
                setcode(tile, 0x60);
                tile->w = w2 - w1;
                addPrim(gGpuCurrentOt - 2, tile);
            }
        }
    }

    if (cfg->mpMax <= 0) {
        w1 = 0;
        w2 = w1;
    } else {
        if (mp >= pendingMp) {
            w1 = (mp - pendingMp) * 0x25 / cfg->mpMax;
            if (w1 >= 0x26) {
                w1 = 0x25;
            } else if (w1 < 0) {
                w1 = 0;
            }
        } else {
            w1 = 0;
        }
        w2 = mp * 0x25 / cfg->mpMax;
        if (w2 >= 0x26) {
            w2 = 0x25;
        }
    }
    if (w1 > 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        tile->x0       = x + 0x30;
        tile->y0       = y + 0xE;
        tile->h        = 2;
        setlen(tile, 3);
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0);
        setcode(tile, 0x60);
        tile->w = w1;
        addPrim(gGpuCurrentOt - 2, tile);
    }
    if (w2 - w1 > 0) {
        tile           = gGpuPrimCursor;
        gGpuPrimCursor = tile + 1;
        {
            s32 tileX = w1 + 0x30;
            tile->x0  = x + tileX;
        }
        tile->y0                          = y + 0xE;
        tile->h                           = 2;
        GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0xff, 0xff, 0, 0);
        setlen(tile, 3);
        setcode(tile, 0x60);
        tile->w = w2 - w1;
        addPrim(gGpuCurrentOt - 2, tile);
    }

    {
        s32 left  = x + 4;
        s32 yb    = y + 0xB;
        s32 clut  = 0x3C0B;
        s32 right = x + 0x23;
        s32 y3    = y + 0x13;

        sp1            = gGpuPrimCursor;
        gGpuPrimCursor = sp1 + 1;
        sp1->x0        = left;
        sp1->y0        = yb;
        sp1->u0        = 0x98;
        sp1->v0        = 0x68;
        sp1->clut      = clut;
        setlen(sp1, 3);
        setcode(sp1, 0x75);
        addPrim(gGpuCurrentOt - 2, sp1);

        sp1            = gGpuPrimCursor;
        gGpuPrimCursor = sp1 + 1;
        sp1->x0        = right;
        sp1->y0        = yb;
        sp1->u0        = 0xA8;
        sp1->v0        = 0x68;
        sp1->clut      = clut;
        setlen(sp1, 3);
        setcode(sp1, 0x75);
        addPrim(gGpuCurrentOt - 2, sp1);

        poly1          = gGpuPrimCursor;
        gGpuPrimCursor = poly1 + 1;
        poly1->x2      = x + 0xC;
        poly1->x0      = x + 0xC;
        poly1->x3      = right;
        poly1->x1      = right;
        poly1->y1      = yb;
        poly1->y0      = yb;
        poly1->y3      = y3;
        poly1->y2      = y3;
        poly1->u0      = 0xA0;
        poly1->v0      = 0x68;
        poly1->u1      = 0xA8;
        poly1->v1      = 0x68;
        poly1->u2      = 0xA0;
        poly1->v2      = 0x70;
        poly1->u3      = 0xA8;
        poly1->v3      = 0x70;
        poly1->clut    = clut;
        poly1->tpage   = 0x3E;
        setlen(poly1, 9);
        setcode(poly1, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly1);

        sp3            = gGpuPrimCursor;
        sp3->x0        = left;
        gGpuPrimCursor = sp3 + 1;
        sp3->y0        = yb;
        sp3->u0        = 0x98;
        sp3->v0        = 0x68;
        sp3->clut      = clut;
        sp3->x0       += 0x2B;
        setlen(sp3, 3);
        setcode(sp3, 0x75);
        addPrim(gGpuCurrentOt - 2, sp3);

        sp3            = gGpuPrimCursor;
        sp3->x0        = right;
        gGpuPrimCursor = sp3 + 1;
        sp3->u0        = 0xA8;
        sp3->v0        = 0x68;
        setlen(sp3, 3);
        setcode(sp3, 0x75);
        sp3->x0  += 0x2B;
        sp3->y0   = yb;
        sp3->clut = clut;
        addPrim(gGpuCurrentOt - 2, sp3);

        poly2          = gGpuPrimCursor;
        gGpuPrimCursor = poly2 + 1;
        poly2->x2      = x + 0x37;
        poly2->x0      = x + 0x37;
        poly2->x3      = x + 0x4E;
        poly2->x1      = x + 0x4E;
        poly2->y1      = yb;
        poly2->y0      = yb;
        poly2->y3      = y3;
        poly2->y2      = y3;
        poly2->u0      = 0xA0;
        poly2->v0      = 0x68;
        poly2->u1      = 0xA8;
        poly2->v1      = 0x68;
        poly2->u2      = 0xA0;
        poly2->v2      = 0x70;
        poly2->u3      = 0xA8;
        poly2->v3      = 0x70;
        poly2->clut    = clut;
        poly2->tpage   = 0x3E;
        setlen(poly2, 9);
        setcode(poly2, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly2);
    }

    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 2, tp);

    {
        RECT rect;

        rect.x   = x;
        rect.y   = y;
        rect.w   = 0x5A;
        rect.h   = 0x14;
        rectMode = 2;
        if (cfg->statusFlags != 0) {
            rectMode = 4;
        }
        Ui_DrawTextInRect(&rect, -1, rectMode, NULL);
    }

    if (cfg->statusFlags != 0) {
        GpHudStatusBits statusBits;

        iconX      = x;
        iconY      = y + 0x14;
        statusBits = D_8009389C;
        for (i = 0; i < 7; i++) {
            flags = statusBits.bits;
            if (cfg->statusFlags & flags[i]) {
                sp5            = gGpuPrimCursor;
                gGpuPrimCursor = sp5 + 1;
                sp5->x0        = iconX;
                iconX         += 0xD;
                sp5->y0        = iconY;
                sp5->u0        = i * 0x10 + 0x60;
                sp5->w         = 0xE;
                sp5->h         = 0xE;
                sp5->v0        = 0x40;
                sp5->clut      = 0x3C08;
                setlen(sp5, 4);
                setcode(sp5, 0x65);
                addPrim(gGpuCurrentOt - 2, sp5);
            }
        }
        Ui_InsertDrawTPage(-2, 0);
    }

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType != 2) {
            Gp_DrawHudNumbers(0x2D, -0x64, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax, 0);
        }
    }
}

void func_800A63B4(s32 arg0, s32 arg1, s32 arg2)
{
    SPRT_8* p;
    s32     otIdx;
    s32     u;

    otIdx          = 0;
    arg0          -= 6;
    p              = gGpuPrimCursor;
    arg1          -= 8;
    gGpuPrimCursor = p + 1;
    p->x0          = arg0;
    p->y0          = arg1;
    if (arg2 == 1) {
        goto case1;
    }
    if (arg2 >= 2) {
        goto default_case;
    }
    if (arg2 != 0) {
        goto default_case;
    }
    p->u0 = 0xA0;
    p->v0 = 0x88;
    goto after_uv;
case1:
    u = 0xA8;
    goto store;
default_case:
    otIdx = -1;
    u     = 0xA0;
store:
    p->u0 = u;
    p->v0 = 0x80;
after_uv:
    p->clut = 0x3C0D;
    setlen(p, 3);
    setcode(p, 0x77);
    addPrim(gGpuCurrentOt + otIdx - 2, p);
}
