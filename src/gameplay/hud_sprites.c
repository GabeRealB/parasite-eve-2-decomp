#include "gameplay/hud_sprites.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/rand.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "area_entry.h"
#include "gameplay/attachment_state.h"
#include "attachment_state.h"
#include "gameplay/attachments.h"
#include "attachments.h"
#include "gameplay/display.h"
#include "ending.h"
#include "gameplay/enemy.h"
#include "hud.h"
#include "hud_sprites.h"
#include "items.h"
#include "linked_actors.h"
#include "gameplay/loading.h"
#include "loading.h"
#include "gameplay/pairsrc.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/scene.h"
#include "scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

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
        req.drawMode   = TEXT_DRAW_QUEUED;                                      \
        req.x          = obj.panel.contentOriginX.unsignedValue + 0x94;         \
        req.y          = (obj.panel.contentOriginY.unsignedValue + 9) + (line); \
        req.otIndex    = obj.panel.otIndex.signedValue + 1;                     \
        Text_DrawString(&req, Text_ItoaSigned(buf, (count)));                   \
        if ((count) == 0) {                                                     \
            flag = 1;                                                           \
        }                                                                       \
    }

/// 18-byte MATRIX rotation (3x3 s16). Assigned via unaligned lwl/lwr + lh/sh
/// (see Gp_ApplyView). The trailing s16 (not u8[2]) keeps the last two bytes
/// a halfword; a pure u8[18] emits lb/sb instead.
typedef struct _GBytes18 {
    u8  data[0x10];
    s16 field_10;
} GBytes18;

#include "gameplay/damage.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gamemain.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/wipsys.h"
#include <psyq/rand.h>

/// 0x1C-byte scratch from the scratch stack used by `Gp_HudTrackEnemy`.
/// `field_14` / `field_16` are the current screen X/Y; `field_18` /
/// `field_1A` hold the signed deltas before and after `>> 3`.
typedef struct _GpHudScratch {
    /* 0x00 */ byte pad_0[0x14];
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
} GpHudScratch;
STATIC_ASSERT_SIZEOF(GpHudScratch, 0x1C);

/// 0x30-byte stack scratch shared by the enemy HP-bar HUD (`Gp_DrawHudNumbers`).
/// The block is first initialised as a `UiObject` (`baseX` / `baseY` /
/// `drawOrder` / `mode`), then reused: `text.buf` is the `Text_ItoaUnsigned`
/// digit buffer with `text.req` (at +0x10) the matching draw request, while the
/// "????" case draws through `bar.req` (at +0) and the trailing
/// `Ui_DrawTextInRect` rectangle is `bar.rect` (also at +0x10).
typedef union GpHudBarScratch {
    UiObject obj;
    struct {
        /* 0x00 */ u8          buf[0x10];
        /* 0x10 */ TextDrawReq req;
    } text;
    struct {
        /* 0x00 */ TextDrawReq req;
        /* 0x10 */ RECT        rect;
    } bar;
} GpHudBarScratch;
STATIC_ASSERT_SIZEOF(GpHudBarScratch, 0x30);

/// 0x48-byte scratch from the scratch stack used by `Gp_UpdateLinkXforms`.
/// `mat` is the transpose of the player `workm`; `vec` at +0x40 is the
/// packed SVECTOR that `gte_stsv` / translation add-sub share. The
/// `stsv` dest pointer is `original_head - 8`, the same address as `vec`.
typedef struct _GpXformScratch {
    /* 0x00 */ MATRIX  mat;
    /* 0x20 */ byte    pad_20[0x20];
    /* 0x40 */ SVECTOR vec;
} GpXformScratch;
STATIC_ASSERT_SIZEOF(GpXformScratch, 0x48);

/// Working state of a relative transform between two coordinate frames, carved
/// from the scratch stack.
///
/// `rot` is the source frame's rotation transposed, so that multiplying a
/// matrix by it yields that matrix's orientation relative to the source;
/// `delta` is the target origin relative to the source, which the same
/// rotation turns into the destination translation.
typedef struct {
    MATRIX rot;   // source frame's rotation, transposed
    VECTOR delta; // target origin minus source origin
} _GpRelMatScratch;
STATIC_ASSERT_SIZEOF(_GpRelMatScratch, 0x30);

u16 D_80114BB0[16];

RECT D_80114BD0;

GpFadeWork D_80114BD8;

/// Resolve a camera-record cursor within its loaded room resource.
/// The address word uses the PS1 representation; the returned record is typed.
static __inline__ GpViewRec* gpViewAt(GpViewRec* records, s32 index)
{
    union {
        GpViewRec* records;
        u32        word;
    } base;
    union {
        GpViewRec* record;
        u32        word;
    } result;
    base.records = records;
    result.word  = index * sizeof(GpViewRec);
    result.word += base.word;
    return result.record;
}

static void Gp_HudTrackEnemy(GpEnemy* arg0, GpHudTrack* arg1);

/// Rotates `v` in place by `m` on the GTE, reading it through a copy.
static inline void _gpRotateVector(MATRIX* m, SVECTOR* v);

static void Gp_StartPadReplay(void);

static s32 Gp_IsStateF0AltClear(void);

static s32 func_800A7AE4(s32 arg0, s32 arg1);

static s32 Gp_StepAttachSlot(s32 arg0, s32 arg1);

static void Gp_EnqueueSndCdIfF0(u8 arg0);

static s32 Gp_CdIdleIfF0Active(void);

static s32 func_800A7E5C(s32 arg0);

static s32 func_800A7F2C(s32 arg0);

/// Updates both coordinate frames and writes the transform from `arg0` to
/// `root` into `result->coord`, using the transposed root rotation to rotate
/// the orientation and translation delta.
static __inline__ void coordToRoot(GfxCoord* arg0, GfxCoord* root, GfxCoord* result);

/// Points the active view at `arg0`: the transposed rotation goes to
/// `gGfxViewRotCoord.coord` and the negated translation to `gGfxViewCoord.coord.t`, with
/// `arg1` (optional) stored as the world offset in `Gfx_ViewOffsetCoord.coord.t`.
/// Coordinates that are not direct children of the root are first folded to
/// root space with `coordToRoot`.
static void Gp_SetViewFromCoord(GfxCoord* arg0, VECTOR* arg1);

/// Spawns the type-0xE view task and points its coordinate at the inverse of
/// `arg0` (transposed rotation, negated translation). `arg1` is the optional
/// world offset stored in the task's 0x10-byte payload.
static s32 Gp_SpawnViewCoordTask(GfxCoord* arg0, VECTOR* arg1);

static void Gp_ResetView(void);

static void func_800A8D5C(void);

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void Gp_DrawHudSprites(GpIdMapC* arg0)
{
    GpXformScratch* block;
    GpLinkNode*     node;
    s32             mode;
    s32             x;
    s32             cx;
    s32             cy;
    s32             y;
    s16             vx;
    s32             vz;
    s32             i;
    s32             n;
    s32             sy;
    DR_TPAGE*       tp;
    SPRT*           sp;
    SPRT*           sp2;
    POLY_GT4*       poly;

    x  = 0x61;
    y  = -0x6C;
    y -= gDisplayState.vramYOffset;
    cx = x + 0x23;
    cy = y + 0x23;
    func_800A63B4(cx, cy, 0);
    node  = Gp_LinkList;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpXformScratch);
    mode  = func_800B9D80(0x400);
    if (node != NULL) {
        do {
            if ((node->state.word & 5) != 1) {
                block->vec.vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
                block->vec.vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
                block->vec.vy = 0;
                if (mode == 0) {
                    gte_lddp(0x1555);
                    gte_ldsv(&block->vec);
                    gte_gpf12();
                    gte_stsv(&block->vec);
                } else {
                    gte_lddp(0xAAA);
                    gte_ldsv(&block->vec);
                    gte_gpf12();
                    gte_stsv(&block->vec);
                }
                if (node->state.b.flags & 1) {
                    goto next;
                }
                vx = block->vec.vx;
                if (vx < -0x1300 || vx > 0x1300) {
                    goto next;
                }
                vz = block->vec.vz;
                if (vz > 0x1300) {
                    goto next;
                }
                if (vz < -0x1300) {
                    goto next;
                }
                if (vx * vx + vz * vz > 0x168FFFF) {
                    goto next;
                }
                block->vec.vx = (s16)(vx + 0x80) >> 8;
                vz            = (s16)(block->vec.vz + 0x80) >> 8;
                block->vec.vz = vz;
                if (node->state.b.targeted != 0) {
                    func_800A63B4(cx + block->vec.vx, cy - vz, 2);
                } else {
                    func_800A63B4(cx + block->vec.vx, cy - vz, 1);
                }
            }
        next:
            node = node->next;
        } while (node != NULL);
    }
    sy = arg0->field_18;
    if (mode == 0) {
        sy *= 2;
    }
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 2, tp);
    tp             = gGpuPrimCursor;
    gGpuPrimCursor = tp + 1;
    setlen(tp, 1);
    tp->code[0] = 0xE100023E;
    addPrim(gGpuCurrentOt - 3, tp);
    if (mode == 1) {
        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = x + 0xD;
        sp->y0         = y + 0xC;
        sp->h          = 0x28;
        sp->w          = 0x28;
        sp->u0         = 0x60;
        sp->v0         = 0xC0;
        sp->clut       = 0x3C0C;
        setlen(sp, 4);
        setcode(sp, 0x65);
        addPrim(gGpuCurrentOt - 2, sp);
    }
    poly                              = gGpuPrimCursor;
    gGpuPrimCursor                    = poly + 1;
    GPU_PRIMITIVE_COLOR_WORD(poly, 2) = PRIM_RGBC(0xc0, 0xc0, 0xc0, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 3) = PRIM_RGBC(0x80, 0x80, 0x80, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 0) = PRIM_RGBC(0x40, 0x40, 0x40, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 1) = PRIM_RGBC(0x30, 0x30, 0x30, 0);
    poly->x1 = poly->x3 = x + 0x40;
    poly->y2 = poly->y3 = y + 0x40;
    poly->tpage         = 0x1E;
    poly->clut          = 0x3C0C;
    setUV4(poly, 0x60, 0x80, 0xA0, 0x80, 0x60, 0xC0, 0xA0, 0xC0);
    setPolyGT4(poly);
    poly->x0 = poly->x2 = x;
    poly->y0 = poly->y1 = y;
    addPrim(gGpuCurrentOt - 2, poly);
    if (arg0->field_16 != -1) {
        sp2            = gGpuPrimCursor;
        gGpuPrimCursor = sp2 + 1;
        sp2->x0        = x + 0xD;
        sp2->y0        = y + 0xC;
        sp2->h         = 0x28;
        sp2->w         = 0x28;
        if (arg0->field_16 != 4) {
            if (arg0->field_16 == 2) {
                sp2->u0 = 0x88;
            } else {
                sp2->u0 = 0xD8;
            }
        } else {
            sp2->u0 = 0xB0;
        }
        sp2->v0   = 0xC0;
        sp2->clut = 0x3C82;
        setlen(sp2, 4);
        setcode(sp2, 0x67);
        addPrim(gGpuCurrentOt - 3, sp2);
        Ui_InsertDrawTPage(-3, 1);
        n = sy;
        if (n > 0x1300) {
            n = 0x1300;
        }
        n >>= 8;
        n  -= 4;
        if (n <= 0) {
            n = 1;
        }
        for (i = 0; i < 0x10; i++) {
            if (i < n) {
                D_80114BB0[i] = 0x9E06;
            } else {
                D_80114BB0[i] = 0;
            }
            if (i == n && i != 0xF) {
                D_80114BB0[i] = 0x8D03;
            }
        }
        D_80114BD0.x = 0x20;
        D_80114BD0.y = 0xF2;
        D_80114BD0.w = 0x10;
        D_80114BD0.h = 1;
        LoadImage(&D_80114BD0, (u_long*)D_80114BB0);
        arg0->field_16 = -1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpXformScratch);
}

void Gp_DrawHudNumbers(s32 x, s32 y, s32 cur, s32 max, s32 kind)
{
    GpHudBarScratch s;
    TextDrawReq     req;
    TILE*           tile;
    SPRT*           sp;
    POLY_FT4*       poly;
    s32             span;
    s32             right;
    s32             w;
    s32             order;

    span = 0x25;
    if (cur < 0) {
        cur = 0;
    }
    y -= gDisplayState.vramYOffset;
    if (Pad_RemapState->field_A != 0) {
        return;
    }

    order                                    = -3;
    s.obj.panel.contentOriginX.unsignedValue = 0;
    s.obj.panel.contentOriginY.unsignedValue = 0;
    s.obj.panel.otIndex.signedValue          = order;
    s.obj.panel.state                        = USER_INTERFACE_PANEL_INITIAL;

    req.x          = x + 4;
    req.y          = y + 8;
    req.otIndex    = -2;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    Text_DrawString(&req, Gp_StrHP);

    if (max >= 0) {
        s32 val = cur;

        if (kind == 0) {
            s32 tx = x + 0x2B;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            s.text.req.x          = tx;
            s.text.req.y          = ty;
            s.text.req.otIndex    = -2;
            s.text.req.colorRgb   = 0x606060;
            s.text.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            s.text.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            s.text.req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&s.text.req, Text_ItoaUnsigned(s.text.buf, val));
        } else {
            s32 tx = x + 0x33;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            s.text.req.x          = tx;
            s.text.req.y          = ty;
            s.text.req.otIndex    = -2;
            s.text.req.colorRgb   = 0x606060;
            s.text.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            s.text.req.alignment  = TEXT_ALIGNMENT_RIGHT;
            s.text.req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            Text_DrawString(&s.text.req, Text_ItoaUnsigned(s.text.buf, val));
            span = 0x2D;
        }

        if (max == 0) {
            w = span;
        } else {
            w = cur * span / max;
        }

        if (w > 0) {
            tile           = gGpuPrimCursor;
            gGpuPrimCursor = tile + 1;
            if (span < w) {
                w = span;
            }
            tile->x0 = x + 5;
            tile->y0 = y + 0xE;
            tile->w  = w;
            tile->h  = 2;
            if (kind == 0) {
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = PRIM_RGBC(0x1f, 0x74, 0x01, 0);
            } else {
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = PRIM_RGBC(0x80, 0, 0, 0);
            }
            setlen(tile, 3);
            setcode(tile, 0x60);
            addPrim(gGpuCurrentOt - 2, tile);
        }

        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        sp->x0         = x + 4;
        sp->u0         = 0x98;
        sp->y0         = y + 0xB;
        sp->v0         = 0x68;
        sp->clut       = 0x3C0B;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt - 2, sp);

        sp             = gGpuPrimCursor;
        gGpuPrimCursor = sp + 1;
        right          = (span + x) - 2;
        sp->x0         = right;
        sp->y0         = y + 0xB;
        sp->clut       = 0x3C0B;
        sp->u0         = 0xA8;
        sp->v0         = 0x68;
        setlen(sp, 3);
        setcode(sp, 0x75);
        addPrim(gGpuCurrentOt - 2, sp);

        poly           = gGpuPrimCursor;
        gGpuPrimCursor = poly + 1;
        poly->x0 = poly->x2 = x + 0xC;
        poly->x1 = poly->x3 = right;
        poly->y2 = poly->y3 = y + 0x13;
        poly->u2 = poly->u0 = 0xA0;
        poly->v3 = poly->v2 = 0x70;
        poly->tpage         = 0x3E;
        poly->y0 = poly->y1 = y + 0xB;
        poly->v0            = 0x68;
        poly->u1            = 0xA8;
        poly->v1            = 0x68;
        poly->clut          = 0x3C0B;
        poly->u3            = 0xA8;
        setlen(poly, 9);
        setcode(poly, 0x2D);
        addPrim(gGpuCurrentOt - 2, poly);
    } else {
        s.bar.req.x          = x + 0x33;
        s.bar.req.y          = y + 0xA;
        s.bar.req.otIndex    = -2;
        s.bar.req.colorRgb   = 0x37A78;
        s.bar.req.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        s.bar.req.alignment  = TEXT_ALIGNMENT_RIGHT;
        s.bar.req.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        Text_DrawString(&s.bar.req, D_800938AC);
        span = 0x2D;
    }

    s.bar.rect.x = x;
    s.bar.rect.y = y;
    s.bar.rect.w = span + 0xA;
    s.bar.rect.h = 0x14;
    Ui_DrawTextInRect(&s.bar.rect, -1, 0x40002, NULL);
}

static void Gp_HudTrackEnemy(GpEnemy* arg0, GpHudTrack* arg1)
{
    GpHudScratch* block;
    s32           val;

    block = SCRATCH_STACK_RESERVE_BLOCK(GpHudScratch);
    if (func_800B9D80(0x100000) != 0) {
        block->field_14 = 0x6A;
        block->field_16 = -0x35;
    } else {
        block->field_14 = 0x6A;
        block->field_16 = -0x64;
    }
    if (arg1->field_0 != arg0) {
        arg1->field_0 = arg0;
        arg1->field_4 = block->field_14;
        arg1->field_4 = block->field_16;
    } else {
        block->field_18   = block->field_14 - arg1->field_4;
        block->field_1A   = block->field_16 - arg1->field_6;
        block->field_18 >>= 3;
        block->field_1A >>= 3;
        block->field_14   = arg1->field_4 + block->field_18;
        block->field_16   = arg1->field_6 + block->field_1A;
    }
    if (arg0->param != NULL) {
        val = arg0->param->hpMax;
        if (arg0->node.state.b.flags & 8) {
            val = -1;
        }
        Gp_DrawHudNumbers(block->field_14 - 8, block->field_16, arg0->hp, val, 1);
    }
    arg1->field_4 = block->field_14;
    arg1->field_6 = block->field_16;
    SCRATCH_STACK_RELEASE_BLOCK(GpHudScratch);
}

/// Rotates `v` in place by `m` on the GTE, reading it through a copy.
static inline void _gpRotateVector(MATRIX* m, SVECTOR* v)
{
    SVECTOR tmp;

    tmp = *v;
    gte_ApplyMatrixSV(m, &tmp, v);
}

void Gp_UpdateLinkXforms(void)
{
    GpLinkNode*     node;
    Task*           slot;
    GfxCoord*       player;
    GpXformScratch* block;

    node = Gp_LinkList;
    slot = gameGetPtrSlot(3);
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    block  = SCRATCH_STACK_RESERVE_BLOCK(GpXformScratch);
    TransposeMatrix(&player->workm, &block->mat);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & 5) == 1) {
            continue;
        }
        block->vec.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        block->vec.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        block->vec.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        _gpRotateVector(&GP_NODE_ENEMY(node)->coord->workm, &block->vec);
        block->vec.vx += GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->vec.vy += GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->vec.vz += GP_NODE_ENEMY(node)->coord->workm.t[2];
        block->vec.vx -= player->workm.t[0];
        block->vec.vy -= player->workm.t[1];
        block->vec.vz -= player->workm.t[2];
        _gpRotateVector(&block->mat, &block->vec);
        GP_NODE_ENEMY(node)->playerRelPos.vx = block->vec.vx;
        GP_NODE_ENEMY(node)->playerRelPos.vy = block->vec.vy;
        GP_NODE_ENEMY(node)->playerRelPos.vz = block->vec.vz;
    }
    SCRATCH_STACK_RELEASE_BLOCK(GpXformScratch);
}

void Gp_StartAreaBgm(s16* arg0)
{
    PlayerStatus* cfg;
    s8            type;
    u8            mode;

    cfg  = &Player_Status;
    mode = gGameSession->restartMode;
    if (mode == 3 || mode == 0xFF || !CdCmd_IsIdle() || *arg0 != 0) {
        return;
    }
    if (gGameSession->deathSoundCountdown == GAME_SESSION_DEATH_SOUND_HOLD) {
        *arg0 = 1;
        return;
    }
    gGameSession->deathSoundCountdown--;
    if (gGameSession->deathSoundCountdown >= 0) {
        return;
    }
    if (cfg->hp <= 0) {
        SndEvt_EnqueueType6((gGameSession->deathVariant << 16) | 0x70000001, 0, 0);
    } else {
        type = Mc_SaveData[0].state.companionType;
        if (type == 1) {
            SndEvt_EnqueueType6(((gGameSession->deathVariant + 0x31) << 16) | 0x70000001, 0, 0);
        } else if (type == 3) {
            SndEvt_EnqueueType7(0x50000000, 1);
            SndEvt_EnqueueType6(0x55170008, 0, 0);
        }
    }
    *arg0 = 1;
}

u8* Gp_GetAttachLevels(void)
{
    PlayerStatus* p;
    s32           cond;

    p = &Player_Status;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        return Mc_SaveData[0].state.attachLevels;
    }
    return Gp_DebugAttachLevels;
}

s32 Gp_IsDebugAttachRoom(void)
{
    PlayerStatus* p;

    p = &Player_Status;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        return 0;
    }
    return p->resourceVariant == 4;
}

s32 Gp_IsStateF0Active(void)
{
    GpStateF0* p;

    p = &Gp_StateF0;
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        return 1;
    }
    return 0;
}

s32 func_800A7550(void)
{
    Gp_ApplyAttachStats(1, 0);
    return 0;
}

void Gp_ResetHudFx(GpIdMapC* arg0)
{
    PlayerStatus* cfg;
    GpStateBE8*   be8;
    GpStateC08*   p;

    cfg                                   = &Player_Status;
    be8                                   = &Gp_HpMpWork;
    be8->field_0                          = cfg->hp;
    be8->field_4                          = cfg->mp;
    arg0->field_16                        = -1;
    arg0->field_18                        = 0;
    p                                     = &Gp_StateC08;
    p->field_10                           = 0;
    p->field_C                            = 0;
    p->field_12                           = 0;
    p->field_D                            = 0;
    p->field_E                            = 0;
    p->field_14                           = 0;
    p->field_F                            = 0;
    p->field_16                           = 0;
    p->field_17                           = 0;
    p->field_A                            = 0;
    gGameSession->battleResetPending      = 0;
    Gp_ItemGrantCooldown                  = 0;
    gDisplayState.suppressDisconnectPause = 1;
    p->field_6                           &= ~2;
}

static void Gp_StartPadReplay(void)
{
    DisplayState* ds;

    srand(1);
    Gp_LcgState              = 0;
    ds                       = &gDisplayState;
    ds->animFrame            = 0;
    gDisplayState.frameCount = 0;
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
}

void Gp_PlayClockState2(Task* arg0)
{
    GameSession* session;
    GpFadeWork*  p;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        arg0->killCountdown = 0;
        Gp_StartAreaBgm(&arg0->killCountdown);
        session             = gGameSession;
        Gp_StateC08.field_3 = 0;
        if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            p          = &D_80114BD8;
            p->field_0 = 0;
            p->field_1 = 0;
            p->field_2 = session->deathFadeFrames;
            Task_SpawnPtr(1, 0x31, 0, p);
        }
        arg0->spawnArg1.value = 0;
        arg0->state++;
    }
}

void Gp_PlayClockState3(Task* arg0)
{
    Gp_StartAreaBgm(&arg0->killCountdown);
    arg0->spawnArg1.value++;
    if (arg0->spawnArg1.value == 0x40) {
        if (gGameSession->restartMode == GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            gDisplayState.skipDraw = 1;
        }
        arg0->spawnArg1.value = 0;
        arg0->state++;
    }
}

void func_800A77B4(Task* arg0)
{
    TaskFuncTable6 sp;

    sp = Gp_PlayClockStates;
    sp.funcs[arg0->state](arg0);
}

void func_800A7824(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 == 0) {
        Gp_DrawAimCircle(0, arg1, arg2, 5);
    }
}

void Gp_HudTrackSlot0(GpHudTrack* arg0)
{
    GpLinkNode* target;
    Task*       work;
    GameActor*  actor;
    GpLinkNode* node;

    work   = Gp_ActorSlots[0];
    target = NULL;
    if (work != NULL) {
        actor = work->work;
        if (actor != NULL) {
            target = actor->field_90C;
        }
        node = Gp_LinkList;
        if (node != NULL) {
            do {
                if (node == target) {
                    if (!(node->state.b.flags & 1)) {
                        Gp_HudTrackEnemy(GP_NODE_ENEMY(node), arg0);
                        return;
                    }
                }
                node = node->next;
            } while (node != NULL);
        }
    }
}

static s32 Gp_IsStateF0AltClear(void)
{
    return Gp_StateF0.prefix.bytes.field_1 == 0;
}

void Gp_EnqueueAttach7Cd(void)
{
    Gp_EnqueueSndCd(Gp_GetAttachLevel(7) + 0x15);
}

void Gp_DrawItemObtained(Task* arg0)
{
    UiObject* obj;

    obj = arg0->spawnArg2.pointer;
    if (arg0->spawnArg1.value == 2) {
        if (arg0->state == 0) {
            Ui_UpdateLayoutSize(&(obj)->panel, Text_MeasureWidth(Gp_StrBonusItem) + 0xA, 0);
            obj->panel.bounds.unsignedRect.x -= 0xF;
            obj->panel.bounds.unsignedRect.y += 9;
            arg0->state++;
        }
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrBonusItem, 0x606060, 1, 0);
    } else {
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrItemObtained, 0x606060, 1, 0);
    }
}

void Gp_DrawItemTitle(Task* arg0)
{
    UiObject* obj;

    obj           = arg0->spawnArg2.pointer;
    obj->field_2E = 0;
    Ui_DrawTitle(&(obj)->panel, Gp_StrItem);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            obj->field_2E = 6;
        }
    }
}

void Gp_TriggerPeIfArmed(void)
{
    u8 state;

    state = Gp_StateF0.prefix.bytes.field_0;
    if ((state == 1) || (state == 3)) {
        if (gGameSession->battleResetPending == 0) {
            Gp_TriggerPeState(1, PLAYER_STATUS_ALL_EFFECTS);
            Gp_PulseState1C80();
            gDisplayState.suppressDisconnectPause = 0;
            Display_InitModeObj(&D_8010CABC, 1, 0, 0x102);
        }
    }
}

static s32 func_800A7AE4(s32 arg0, s32 arg1)
{
    return (arg0 / 3) * 16 + (arg0 % 3) * 4 + arg1 + 0x300;
}

s32 Gp_GetAttachLevel(s32 arg0)
{
    PlayerStatus* p;
    s32           cond;
    s32           ret;
    u8*           table;

    ret = 1;
    if (arg0 < 0xC) {
        p = &Player_Status;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = Mc_SaveData[0].state.attachLevels;
        } else {
            table = Gp_DebugAttachLevels;
        }
        ret = table[arg0];
        if (ret == 0) {
            ret = 1;
        }
        if (p->statusFlags & PLAYER_STATUS_BERSERKER) {
            if (ret < 3) {
                ret++;
            }
        }
    }
    return ret;
}

static s32 Gp_StepAttachSlot(s32 arg0, s32 arg1)
{
    PlayerStatus* p;
    McSaveData*   save;
    s32           cond;
    u8*           table;

    p = &Player_Status;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        table = Mc_SaveData[0].state.attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        save = &Mc_SaveData[0];
        do {
            if (arg1 > 0) {
                do {
                    arg0++;
                    if (arg0 >= 0xC) {
                        arg0 = 0;
                    }
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
                arg1--;
            } else {
                do {
                    arg0--;
                    if (arg0 < 0) {
                        arg0 += 0xC;
                    }
                } while (table[arg0] == 0 && save->state.cheatMode == 0);
                arg1++;
            }
        } while (arg1 != 0);
    }
    return arg0;
}

s32 func_800A7CB0(s32 unused)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return 0;
    }
    return 0;
}

static void Gp_EnqueueSndCdIfF0(u8 arg0)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        Gp_EnqueueSndCd(arg0);
    }
}

static s32 Gp_CdIdleIfF0Active(void)
{
    GpStateF0* p;
    s32        cond;

    p = &Gp_StateF0;
    if ((p->prefix.bytes.field_0 == 1 && p->field_6 != 0) || p->prefix.bytes.field_1 != 0) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        return CdCmd_IsIdle() & 0xFFFF;
    }
    return 1;
}

void func_800A7DB8(s32 arg0)
{
    if (!(Gp_StateC08.field_6 & 1)) {
        Gp_StateC08.field_E = arg0;
    }
}

void func_800A7DE0(void)
{
    GpStateC08* p;

    CdCmd_EnqueueLoadFile(0, 0, 4);
    p = &Gp_StateC08;
    if (p->field_A >= 2) {
        p->field_3 = 2;
    }
    p->field_E         = 0;
    p->field_A         = 0;
    D_80115768         = 0;
    Gp_StateF0.field_4 = 0;
    p->field_7         = 0;
    p->field_8         = 0;
}

void func_800A7E4C(void)
{
    Gp_ItemGrantCooldown = 5;
}

static s32 func_800A7E5C(s32 arg0)
{
    Task*         work;
    GameActor*    actor;
    PlayerStatus* p;
    s32           flag;

    flag = 0;
    work = Gp_ActorSlots[0];
    if (work != NULL) {
        actor = work->work;
        p     = &Player_Status;
        if (actor->field_954 == 0) {
            if (actor->field_956 == 0 || actor->field_956 == 2) {
                if (gGameSession->dirActionBusy == 0) {
                    if (p->interactionPressed == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (arg0 == 0) {
        if (Gp_StateC08.field_6 & 2) {
            flag = 0;
        }
    }
    if (flag != 0) {
        if (Gp_ItemGrantCooldown <= 0) {
            if (Gp_StateF0.prefix.bytes.field_1 == 0) {
                return 1;
            }
        }
    }
    return 0;
}

void func_800A7F24(void)
{
}

static s32 func_800A7F2C(s32 arg0)
{
    return arg0 - 0x10;
}

s32 Gp_SpendMp(s32 arg0)
{
    PlayerStatus* p;
    s32           ret;

    p   = &Player_Status;
    ret = 1;
    if (p->mp >= arg0) {
        p->mp -= arg0;
    } else {
        p->mp = 0;
        ret   = 0;
    }
    return ret;
}

/// Updates both coordinate frames and writes the transform from `arg0` to
/// `root` into `result->coord`, using the transposed root rotation to rotate
/// the orientation and translation delta.
static __inline__ void coordToRoot(GfxCoord* arg0, GfxCoord* root, GfxCoord* result)
{
    _GpRelMatScratch* tmp;
    MATRIX*           rootm;
    MATRIX*           world;
    MATRIX*           out;

    Gp_UpdateCoord(arg0);
    Gp_UpdateCoord(root);

    rootm = &root->workm;
    world = &arg0->workm;
    tmp   = SCRATCH_STACK_CURSOR(_GpRelMatScratch) - 1;
    out   = &result->coord;

    SCRATCH_STACK_CURSOR(_GpRelMatScratch) = tmp;

    gte_TransposeMatrix(rootm, &tmp->rot);

    gte_MulMatrix0(&tmp->rot, world, out);

    tmp->delta.vx = world->t[0] - rootm->t[0];
    tmp->delta.vy = world->t[1] - rootm->t[1];
    tmp->delta.vz = world->t[2] - rootm->t[2];
    ApplyMatrixLV(&tmp->rot, &tmp->delta, (VECTOR*)out->t);

    SCRATCH_STACK_RELEASE_BLOCK(_GpRelMatScratch);
}

/// Points the active view at `arg0`: the transposed rotation goes to
/// `gGfxViewRotCoord.coord` and the negated translation to `gGfxViewCoord.coord.t`, with
/// `arg1` (optional) stored as the world offset in `Gfx_ViewOffsetCoord.coord.t`.
/// Coordinates that are not direct children of the root are first folded to
/// root space with `coordToRoot`.
static void Gp_SetViewFromCoord(GfxCoord* arg0, VECTOR* arg1)
{
    GfxCoord* root;
    GfxCoord* parent;
    GfxCoord  rel;

    if (arg1 != NULL) {
        Gfx_ViewOffsetCoord.coord.t[0] = arg1->vx;
        Gfx_ViewOffsetCoord.coord.t[1] = arg1->vy;
        Gfx_ViewOffsetCoord.coord.t[2] = arg1->vz;
    } else {
        Gfx_ViewOffsetCoord.coord.t[0] = 0;
        Gfx_ViewOffsetCoord.coord.t[1] = 0;
        Gfx_ViewOffsetCoord.coord.t[2] = 0;
    }

    parent = arg0->parent;
    root   = &gGfxViewCoord;
    if (parent == root) {
        gte_TransposeMatrix(&arg0->coord, &gGfxViewRotCoord.coord);
        root->coord.t[0] = -arg0->coord.t[0];
        root->coord.t[1] = -arg0->coord.t[1];
        root->coord.t[2] = -arg0->coord.t[2];
    } else {
        coordToRoot(arg0, root, &rel);
        gte_TransposeMatrix(&rel.coord, &gGfxViewRotCoord.coord);
        root->coord.t[0] = -rel.coord.t[0];
        root->coord.t[1] = -rel.coord.t[1];
        root->coord.t[2] = -rel.coord.t[2];
    }
    arg0->composeStamp = GRAPHICS_COORD_DIRTY;

    Gfx_ViewOffsetCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    gGfxViewRotCoord.composeStamp    = GRAPHICS_COORD_DIRTY;
    gGfxViewCoord.composeStamp       = GRAPHICS_COORD_DIRTY;
}

/// Spawns the type-0xE view task and points its coordinate at the inverse of
/// `arg0` (transposed rotation, negated translation). `arg1` is the optional
/// world offset stored in the task's 0x10-byte payload.
static s32 Gp_SpawnViewCoordTask(GfxCoord* arg0, VECTOR* arg1)
{
    GfxCoord* coord;
    Task*     task;
    VECTOR*   pos;
    GfxCoord* root;
    GfxCoord* parent;
    GfxCoord  rel;

    task = Task_Spawn(0, 0xE, 0, 0);
    if (task == NULL) {
        return 0;
    }
    pos = memCalloc(sizeof(VECTOR), 0);
    if (pos == NULL) {
        taskKill(task);
        return 0;
    }
    task->work = pos;
    coord      = task->extra.tmd->coords;
    if (arg1 != NULL) {
        pos->vx = arg1->vx;
        pos->vy = arg1->vy;
        pos->vz = arg1->vz;
    } else {
        pos->vx = 0;
        pos->vy = 0;
        pos->vz = 0;
    }

    parent = arg0->parent;
    root   = &gGfxViewCoord;
    if (parent == root) {
        gte_TransposeMatrix(&arg0->coord, &coord->coord);
        coord->coord.t[0] = -arg0->coord.t[0];
        coord->coord.t[1] = -arg0->coord.t[1];
        coord->coord.t[2] = -arg0->coord.t[2];
    } else {
        coordToRoot(arg0, root, &rel);
        gte_TransposeMatrix(&rel.coord, &coord->coord);
        coord->coord.t[0] = -rel.coord.t[0];
        coord->coord.t[1] = -rel.coord.t[1];
        coord->coord.t[2] = -rel.coord.t[2];
    }
    return 1;
}

void func_800A8654(Task* task)
{
    VECTOR*               vec;
    GfxCoord*             src;
    GfxCoord*             c1;
    GfxCoord*             c2;
    GfxCoord*             c3;
    ModelObjectCoordBody* body;
    s32                   i;
    s32                   j;

    i              = 0;
    c1             = &Gfx_ViewOffsetCoord;
    body           = task->extra.coordBody;
    vec            = (VECTOR*)task->work;
    src            = body->coord;
    c1->coord.t[0] = vec->vx;
    c2             = &gGfxViewRotCoord;
    c1->coord.t[1] = vec->vy;
    c1->coord.t[2] = vec->vz;

    for (; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            *(s16*)((i * 6 + j * 2) + (s32)c2->coord.m) = src->coord.m[i][j];
        }
    }

    c3             = &gGfxViewCoord;
    c3->coord.t[0] = src->coord.t[0];
    c3->coord.t[1] = src->coord.t[1];
    c3->coord.t[2] = src->coord.t[2];

    Gfx_ViewOffsetCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    gGfxViewRotCoord.composeStamp    = GRAPHICS_COORD_DIRTY;
    gGfxViewCoord.composeStamp       = GRAPHICS_COORD_DIRTY;
    taskKill(task);
}

void Gp_LoadStageView(void)
{
    GameLocationKey* sess;
    GpViewTbl*       tbl;
    GpViewRec*       recs;
    GpViewRec*       rec;
    GfxCoord*        c1;
    MATRIX*          rot;
    VECTOR3*         trans;
    u8               idx;

    sess = &gGameSession->location.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;
    rec   = gpViewAt(recs, idx);

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(GBytes18*)rot = *(GBytes18*)(rec - 1);
    *trans          = *(VECTOR3*)&(rec - 1)->mtx.t;

    rec--;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = rec->screenDistance;
    gte_SetGeomScreen(rec->screenDistance);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.composeStamp                  = GRAPHICS_COORD_DIRTY;
    PARENT_OF(rot, GfxCoord, coord)->composeStamp     = GRAPHICS_COORD_DIRTY;
    PARENT_OF(trans, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

void Gp_WorldToLocal(MATRIX* arg0, MATRIX* arg1, MATRIX* arg2)
{
    _GpRelMatScratch* tmp;

    tmp                                    = SCRATCH_STACK_CURSOR(_GpRelMatScratch) - 1;
    SCRATCH_STACK_CURSOR(_GpRelMatScratch) = tmp;

    gte_TransposeMatrix(arg0, &tmp->rot);

    gte_MulMatrix0(&tmp->rot, arg1, arg2);

    tmp->delta.vx = arg1->t[0] - arg0->t[0];
    tmp->delta.vy = arg1->t[1] - arg0->t[1];
    tmp->delta.vz = arg1->t[2] - arg0->t[2];
    ApplyMatrixLV(&tmp->rot, &tmp->delta, (VECTOR*)arg2->t);

    SCRATCH_STACK_RELEASE_BLOCK(_GpRelMatScratch);
}

s32 Gp_TrySpawnViewTask(GpViewRec* arg0)
{
    return Task_Spawn(0, 0xF, 0, arg0) != NULL;
}

void Gp_ApplyView(GpViewRec* arg0)
{
    GfxCoord* c1;
    MATRIX*   rot;
    VECTOR3*  trans;

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(GBytes18*)rot = *(GBytes18*)arg0;
    *trans          = *MATRIX_TRANS(&arg0->mtx);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = arg0->screenDistance;
    gte_SetGeomScreen(arg0->screenDistance);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.composeStamp                  = GRAPHICS_COORD_DIRTY;
    PARENT_OF(rot, GfxCoord, coord)->composeStamp     = GRAPHICS_COORD_DIRTY;
    PARENT_OF(trans, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

static void Gp_ResetView(void)
{
    MATRIX*            m;
    volatile GfxCoord* c1;
    GfxCoord*          c2;
    GfxCoord*          c3;
    s32                one;

    c1             = &Gfx_ViewOffsetCoord;
    one            = ONE;
    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = one;

    *(volatile s32*)&gGfxViewRotCoord.coord = one;
    m                                       = &gGfxViewRotCoord.coord;
    c2                                      = PARENT_OF(m, GfxCoord, coord);
    MATRIX_PAIR(m, 1, 1)                    = one;
    m->m[2][2]                              = one;

    c3                   = &gGfxViewCoord;
    MATRIX_PAIR(m, 0, 2) = 0;
    MATRIX_PAIR(m, 2, 0) = 0;
    c3->coord.t[0]       = 0;
    c3->coord.t[1]       = 0;
    c3->coord.t[2]       = 0;
    c1->composeStamp     = GRAPHICS_COORD_DIRTY;
    c2->composeStamp     = GRAPHICS_COORD_DIRTY;
    c3->composeStamp     = GRAPHICS_COORD_DIRTY;
}

void Gp_SpawnViewTasks(void)
{
    GameLocationKey* sess;
    GpViewTbl*       tbl;
    GpViewRec*       recs;
    GpViewRec*       rec;
    u8               idx;

    sess = &gGameSession->location.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();
    rec  = gpViewAt(recs, idx);
    Task_SpawnPtr(0, 0xF, 0, (rec - 1));
    Task_Spawn(0, 0x17, 0, 0);
}

GpViewRec* Gp_GetStageView(GameLocationKey* arg0)
{
    GpViewTbl* tbl;
    GpViewRec* recs;
    u8         idx;

    tbl  = Gp_ViewTables[arg0->stage - 1];
    recs = tbl->field_0[arg0->area - 1];
    idx  = Gp_GetViewIndex();
    return &recs[idx - 1];
}

void Gp_ApplyViewTask(Task* task)
{
    GfxCoord*  c1;
    MATRIX*    rot;
    VECTOR3*   trans;
    GpViewRec* rec;

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;
    rec   = task->spawnArg2.pointer;

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(GBytes18*)rot = *(GBytes18*)rec;
    *trans          = *MATRIX_TRANS(&rec->mtx);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = rec->screenDistance;
    gte_SetGeomScreen(rec->screenDistance);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.composeStamp                  = GRAPHICS_COORD_DIRTY;
    PARENT_OF(rot, GfxCoord, coord)->composeStamp     = GRAPHICS_COORD_DIRTY;
    PARENT_OF(trans, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
    taskKill(task);
}

static void func_800A8D5C(void)
{
    VECTOR   vec;
    GfxCoord coord;
    s32      one;
    MATRIX*  m;

    vec.vx                          = 0;
    vec.vy                          = 0;
    vec.vz                          = ONE;
    one                             = ONE;
    m                               = &coord.coord;
    coord.parent                    = &gGfxViewCoord;
    *(s32*)&coord.coord             = one;
    MATRIX_PAIR(&coord.coord, 0, 2) = 0;
    MATRIX_PAIR(m, 1, 1)            = one;
    MATRIX_PAIR(&coord.coord, 2, 0) = 0;
    m->m[2][2]                      = one;
    coord.coord.t[0]                = 0;
    coord.coord.t[1]                = 0;
    coord.coord.t[2]                = 0;
    Gp_SpawnViewCoordTask(&coord, &vec);
}

void Gp_SpawnCurView(s32 arg0)
{
    GameLocationKey* sess;
    GpViewTbl*       tbl;
    GpViewRec*       recs;
    GpViewRec*       rec;
    u8               idx;

    sess = &gGameSession->location.loc;
    tbl  = Gp_ViewTables[sess->stage - 1];
    recs = tbl->field_0[sess->area - 1];
    idx  = Gp_GetViewIndex();
    rec  = gpViewAt(recs, idx);
    Task_SpawnPtr(0, 0xF, 0, (rec - 1));
    if (arg0 == 0) {
        Task_Spawn(0, 0x17, 0, 0);
    }
    if (arg0 == 1) {
        Task_SpawnOnDefaultListA(0, 0x17, 0, 0);
    }
}

void Gp_ViewGateTask(Task* task)
{
    GameSession* sess;
    McSaveData*  save;
    CdCmdQueue*  q;
    s32          loc;

    gGameSession->viewReady = 0;
    if (task->state == 0) {
        task->state = 3;
    }
    save = &Mc_SaveData[0];
    if (task->spawnArg1.value != save->state.at4.loc.view) {
        gGameSession->viewDirty = 1;
    }
    sess = gGameSession;
    if (sess->viewDirty != 0) {
        q = &CdCmd_Queue;
        if ((q->scenePayloadAvailable == 0) || (q->scenePayloadLoading == 0)) {
            sess->location.loc.view = save->state.at4.loc.view;
            Pad_SetCooldown(0);
            Gp_SpawnViewTasks();
            if (Display_SpawnWithOtSmall(0, 0x1E, 0, 0) != 0) {
                loc                   = gGameSession->location.loc.view;
                task->killCountdown   = 2;
                task->spawnArg1.value = loc;
                if (task->state == 3) {
                    task->state = 1;
                }
            }
        }
    }
    if (task->state == 1) {
        Display_AcquireRef();
        task->state += 1;
    }
    if (task->state == 2) {
        task->killCountdown--;
        if (task->killCountdown == 0) {
            Display_ReleaseRef();
            gGameSession->viewReady = 1;
            task->state             = 3;
        }
    }
}
