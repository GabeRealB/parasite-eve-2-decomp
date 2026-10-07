#include "linked_actors.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/inline_c.h>

#include "common.h"
#include "gte.h"

#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "ending.h"
#include "gameplay/enemy.h"
#include "hud.h"
#include "hud_sprites.h"
#include "gameplay/scene.h"
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
        textDrawString(&req, (str));                                            \
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
        textDrawString(&req, textItoaSigned(buf, (count)));                     \
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

/// Scratch-stack workspace for drawing the wireframe of a Parasite Energy area.
///
/// The wireframe is a set of polylines. Each vertex is built in the area's own
/// frame, with the area's axis along negative Y, projected through the GTE and
/// joined to the vertex before it by a flat line. `radius` and `extent` are in
/// world units: the dimensions of an `AttachmentAreaParam` once scaled. Reserve
/// the complete record; nothing in it survives the release.
typedef struct {
    SVECTOR point;            // Vertex being projected, in the area's frame; before that, the centre's offset rotated into view space
    MATRIX  viewRotation;     // Area-to-view rotation loaded into the GTE; only its rotation elements are used
    s32     radius;           // Radius of the area about its axis
    s32     extent;           // Extent of the area along its axis (an ellipsoid's semi-axis, a cylinder's length)
    s32     ringRadius;       // Radius of the ring the vertex lies on; an ellipsoid narrows it with height
    s32     depthCue;         // GTE IR0 depth-cue coefficient of the last projection; stored, never read
    s32     projectionFlags;  // GTE FLAG bits of the last projection; stored, never read
    s32     orderingDepth;    // Quarter camera-space depth of the last projected vertex (0..16383)
    u32     previousScreenXy; // Screen position of the vertex before it, in the same packing
    u32     screenXy;         // Screen position of the last projected vertex (X in bits 0..15, Y in bits 16..31)
    byte    unknown_48[8];    // Never accessed; role unproven
    VECTOR  viewCentre;       // View-space centre of an area that is not centred on the player's root
} _AttachmentAreaWireframeScratch;
STATIC_ASSERT_SIZEOF(_AttachmentAreaWireframeScratch, 0x60);

// The image stores this head alone in the linked_actors BSS subsegment.
WorldTargetNode* gWorldTargetListHead;

static __inline__ void Gp_RingPointXZ(_AttachmentAreaWireframeScratch* scratch, s32 ang);

static __inline__ void Gp_ProjectRingPt(_AttachmentAreaWireframeScratch* scratch);

static __inline__ void Gp_LinkRingSeg(_AttachmentAreaWireframeScratch* scratch);

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
                attachmentAddTargetContact(claim, idx);
            }
        }
    }
}

static __inline__ void Gp_RingPointXZ(_AttachmentAreaWireframeScratch* scratch, s32 ang)
{
    scratch->point.vx = (scratch->ringRadius * rcos(ang)) >> 12;
    scratch->point.vz = (scratch->ringRadius * rsin(ang)) >> 12;
}

static __inline__ void Gp_ProjectRingPt(_AttachmentAreaWireframeScratch* scratch)
{
    gte_ldv0(&scratch->point);
    gte_rtps();
    gte_stsxy(&scratch->screenXy);
    gte_stdp(&scratch->depthCue);
    gte_stflg(&scratch->projectionFlags);
    gte_stszotz(&scratch->orderingDepth);
}

static __inline__ void Gp_LinkRingSeg(_AttachmentAreaWireframeScratch* scratch)
{
    LINE_F2* prim;

    prim                              = gGpuPrimCursor;
    gGpuPrimCursor                    = prim + 1;
    GPU_PRIMITIVE_COLOR_WORD(prim, 0) = GPU_PACK_COLOR_WORD(0, 0xc0, 0x40, 0);
    GPU_PRIMITIVE_XY_WORD(prim, 0)    = scratch->previousScreenXy;
    GPU_PRIMITIVE_XY_WORD(prim, 1)    = scratch->screenXy;
    setlen(prim, 3);
    setcode(prim, 0x40);
    addPrim(GPU_ORDERING_TABLE_ENTRY_AT_BYTE_OFFSET(((((u32)scratch->orderingDepth << gDisplayState.otDepthShift) >> 2) & GPU_ORDERING_TABLE_DEPTH_BYTE_MASK)),
            prim);
}

void Gp_DrawAimCircle(s32 arg0, s32 arg1, s32 arg2, s32 arg3)
{
    Task*                            slot;
    GfxCoord*                        coord;
    GfxCoord*                        other;
    _AttachmentAreaWireframeScratch* scratch;
    s32                              base;
    s32                              limit;
    s32                              ang;
    s32                              i;
    s32                              t;
    s32                              pass;

    slot            = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_AttachmentAreaWireframeScratch);
    coord           = slot->extra.tmd->coords;
    scratch->radius = arg1;
    scratch->extent = arg2;
    base            = gDisplayState.animFrame << 4;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    if (arg3 & 4) {
        other             = &slot->extra.tmd->coords[4];
        scratch->point.vx = 0;
        scratch->point.vy = 0x12C;
        scratch->point.vz = 0;
        _gfxLoadRotSv(&coord->workm, &scratch->point);
        gte_rtv0();
        gte_stsv(&scratch->point);
        scratch->viewCentre.vx = other->workm.t[0] + scratch->point.vx;
        scratch->viewCentre.vy = other->workm.t[1] + scratch->point.vy;
        scratch->viewCentre.vz = other->workm.t[2] + scratch->point.vz;
        gte_SetTransVector(&scratch->viewCentre);
    } else if ((arg3 & 2) == 0) {
        gte_SetTransMatrix(&coord->workm);
    } else {
        arg3             &= ~2;
        scratch->point.vx = 0;
        scratch->point.vy = 0;
        scratch->point.vz = arg1;
        _gfxLoadRotSv(&coord->workm, &scratch->point);
        gte_rtv0();
        gte_stsv(&scratch->point);
        scratch->viewCentre.vx = coord->workm.t[0] + scratch->point.vx;
        scratch->viewCentre.vy = coord->workm.t[1] + scratch->point.vy;
        scratch->viewCentre.vz = coord->workm.t[2] + scratch->point.vz;
        gte_SetTransVector(&scratch->viewCentre);
    }

    if (arg3 & 4) {
        scratch->viewRotation = coord->workm;
        gfxRotMatrixX(&scratch->viewRotation, -0x400, GRAPHICS_ROTATION_COMPOSE);
        arg3 &= ~4;
    } else {
        scratch->viewRotation = gGfxViewCoord.workm;
    }
    gte_SetRotMatrix(&scratch->viewRotation);

    limit = 0x400;
    if (arg3 == 0) {
        limit = 0x300;
    }

    for (i = 0; i < 12; i++) {
        ang = base + ((i << 12) / 12);
        for (t = 0; t <= limit; ang += 0x73, t += 0x80) {
            if (arg3 == 0) {
                scratch->ringRadius = (scratch->radius * rcos(t)) >> 12;
                scratch->point.vy   = -(scratch->extent * rsin(t)) >> 12;
                Gp_RingPointXZ(scratch, ang);
            } else {
                scratch->point.vy = -(scratch->extent * t) >> 10;
                scratch->point.vx = (scratch->radius * rcos(ang)) >> 12;
                scratch->point.vz = (scratch->radius * rsin(ang)) >> 12;
            }
            Gp_ProjectRingPt(scratch);
            if (t > 0) {
                Gp_LinkRingSeg(scratch);
            }
            scratch->previousScreenXy = scratch->screenXy;
        }
    }

    base = -base;
    for (pass = 0; pass < 2; pass++) {
        if (pass == 0) {
            if (arg3 == 0) {
                scratch->ringRadius = (scratch->radius * rcos(0x300)) >> 12;
                scratch->point.vy   = -(scratch->extent * rsin(0x300)) >> 12;
            } else {
                scratch->ringRadius = scratch->radius;
                scratch->point.vy   = -scratch->extent;
            }
        } else {
            scratch->point.vy   = 0;
            scratch->ringRadius = scratch->radius;
        }
        for (i = 0; i < 25; i++) {
            ang = base + ((i << 12) / 24);
            Gp_RingPointXZ(scratch, ang);
            Gp_ProjectRingPt(scratch);
            if (i != 0) {
                Gp_LinkRingSeg(scratch);
            }
            scratch->previousScreenXy = scratch->screenXy;
        }
    }

    SCRATCH_STACK_RELEASE_BLOCK(_AttachmentAreaWireframeScratch);
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
                            attachmentAddTargetContact(enemy, idx);
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
                            attachmentAddTargetContact(claim, idx);
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
    textDrawString(&req, textItoaUnsigned(buf, val));
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
    textDrawString(&hpReq, Gp_StrHP);

    mpReq.x          = obj->panel.contentOriginX.unsignedValue + 0x2E + x;
    mpReq.y          = obj->panel.contentOriginY.unsignedValue + 8 + y;
    mpReq.otIndex    = obj->panel.otIndex.signedValue + 1;
    mpReq.colorRgb   = color;
    mpReq.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    mpReq.alignment  = TEXT_ALIGNMENT_LEFT;
    mpReq.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&mpReq, Gp_StrMP);
}

void func_800A57B0(HudState* hud)
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
        pendingHp = hud->previewCastCost << 1;
    } else {
        pendingMp = hud->previewCastCost;
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
        uiDrawRectFrame(&rect, -1, rectMode, NULL);
    }

    if (cfg->statusFlags != 0) {
        iconX = x;
        iconY = y + 0x14;
        {
            // Effect each icon of the status strip stands for, in the strip's
            // left-to-right order. Active effects are drawn packed from the left.
            u16 iconStatusMasks[7] = {
                PLAYER_STATUS_DARKNESS,
                PLAYER_STATUS_PARALYSIS,
                PLAYER_STATUS_POISON,
                PLAYER_STATUS_SILENCE,
                0x20, // Timed effect whose gameplay meaning is unproven
                PLAYER_STATUS_CONFUSION,
                PLAYER_STATUS_BERSERKER,
            };

            for (i = 0; i < ARRAY_SIZE(iconStatusMasks); i++) {
                if (cfg->statusFlags & iconStatusMasks[i]) {
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
        }
        uiQueueTexturePage(-2, 0);
    }

    if (gPlayerActorTasks[PLAYER_ACTOR_TASK_COMPANION] != NULL) {
        if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType != 2) {
            hudDrawHpReadout(0x2D, -0x64, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHp, gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionHpMax, HUD_HP_READOUT_COMPANION);
        }
    }
}

void func_800A63B4(s32 arg0, s32 arg1, s32 arg2)
{
    SPRT_8* p;
    s32     otIdx;

    otIdx          = 0;
    arg0          -= 6;
    p              = gGpuPrimCursor;
    arg1          -= 8;
    gGpuPrimCursor = p + 1;
    p->x0          = arg0;
    p->y0          = arg1;
    switch (arg2) {
        case 0:
            p->u0 = 0xA0;
            p->v0 = 0x88;
            break;
        case 1:
            p->u0 = 0xA8;
            p->v0 = 0x80;
            break;
        case 2:
        default:
            otIdx = -1;
            p->u0 = 0xA0;
            p->v0 = 0x80;
            break;
    }
    p->clut = 0x3C0D;
    setlen(p, 3);
    setcode(p, 0x77);
    addPrim(gGpuCurrentOt + otIdx - 2, p);
}
