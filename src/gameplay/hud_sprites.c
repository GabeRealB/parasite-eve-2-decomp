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
#include "gameplay/enemy_params.h"
#include "gameplay/player_actor.h"
#include "gameplay/player_state.h"
#include "gameplay/scene.h"
#include "gameplay/scene_combat.h"
#include "scene_runtime.h"
#include "gameplay/view.h"
#include "gameplay/world_targets.h"
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
#include "main/fs.h"
#include "main/random.h"
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

/// The nine rotation coefficients of a view `MATRIX`, assigned as one value.
///
/// Laid out as `MATRIX::m`: row-major signed coefficients with 12 fractional
/// bits (`ONE` is 1.0). Applying a `ViewCamera` assigns this through both
/// matrices' `m`, so exactly these 18 bytes move; the alignment bytes before
/// `MATRIX::t` and the translation, which belongs to a different coordinate
/// node, are not part of the value. Halfword alignment is all it requires.
typedef struct {
    s16 m[3][3]; // World-to-camera rotation, row-major
} _ViewRotation;
STATIC_ASSERT_SIZEOF(_ViewRotation, 0x12);

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

/// Scratch workspace for expressing a transform relative to a reference frame.
///
/// Both input transforms share a containing frame. The reference rotation's
/// transpose multiplies the target rotation and rotates their origin difference;
/// this inverts the reference rotation when it is orthonormal.
///
/// Requires an initialized scratch stack with room for one 48-byte reservation,
/// released after the conversion. Only the matrix rotation and vector XYZ are
/// initialized; the matrix translation and vector's fourth word are unused.
typedef struct {
    MATRIX transposedRotation; // Reference rotation transposed, scaled by ONE (4096); t unused
    VECTOR originDelta;        // Target origin minus reference origin in the containing frame, in signed game units
} _GfxRelativeTransformScratch;
STATIC_ASSERT_SIZEOF(_GfxRelativeTransformScratch, 0x30);

u16 D_80114BB0[16];

RECT D_80114BD0;

ScreenFade D_80114BD8;

/// Resolve a camera-record cursor within its loaded room resource.
/// The address word uses the PS1 representation; the returned record is typed.
static __inline__ ViewCamera* gpViewAt(ViewCamera* records, s32 index)
{
    union {
        ViewCamera* records;
        u32         word;
    } base;
    union {
        ViewCamera* record;
        u32         word;
    } result;
    base.records = records;
    result.word  = index * sizeof(ViewCamera);
    result.word += base.word;
    return result.record;
}

static void Gp_HudTrackEnemy(Enemy* arg0, HudTargetHpReadout* readout);

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
    GpXformScratch*  block;
    WorldTargetNode* node;
    s32              mode;
    s32              x;
    s32              cx;
    s32              cy;
    s32              y;
    s16              vx;
    s32              vz;
    s32              i;
    s32              n;
    s32              sy;
    DR_TPAGE*        tp;
    SPRT*            sp;
    SPRT*            sp2;
    POLY_GT4*        poly;

    x  = 0x61;
    y  = -0x6C;
    y -= gDisplayState.vramYOffset;
    cx = x + 0x23;
    cy = y + 0x23;
    func_800A63B4(cx, cy, 0);
    node  = gWorldTargetListHead;
    block = SCRATCH_STACK_RESERVE_BLOCK(GpXformScratch);
    mode  = func_800B9D80(0x400);
    if (node != NULL) {
        do {
            if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
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
                if (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
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
                if (node->state.parts.targeted != 0) {
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
    GPU_PRIMITIVE_COLOR_WORD(poly, 2) = GPU_PACK_COLOR_WORD(0xc0, 0xc0, 0xc0, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 3) = GPU_PACK_COLOR_WORD(0x80, 0x80, 0x80, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 0) = GPU_PACK_COLOR_WORD(0x40, 0x40, 0x40, 0);
    GPU_PRIMITIVE_COLOR_WORD(poly, 1) = GPU_PACK_COLOR_WORD(0x30, 0x30, 0x30, 0);
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
    if (Pad_RemapState->hideHud != 0) {
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
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0x1f, 0x74, 0x01, 0);
            } else {
                GPU_PRIMITIVE_COLOR_WORD(tile, 0) = GPU_PACK_COLOR_WORD(0x80, 0, 0, 0);
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

static void Gp_HudTrackEnemy(Enemy* arg0, HudTargetHpReadout* readout)
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
    if (readout->enemy != arg0) {
        // A new target starts at the anchor. The second store also lands in
        // `x`; both members are rewritten after the draw.
        readout->enemy = arg0;
        readout->x     = block->field_14;
        readout->x     = block->field_16;
    } else {
        block->field_18   = block->field_14 - readout->x;
        block->field_1A   = block->field_16 - readout->y;
        block->field_18 >>= 3;
        block->field_1A >>= 3;
        block->field_14   = readout->x + block->field_18;
        block->field_16   = readout->y + block->field_1A;
    }
    if (arg0->param != NULL) {
        val = arg0->param->hpMax;
        if (arg0->node.state.parts.flags & WORLD_TARGET_HIDE_HP) {
            val = -1;
        }
        Gp_DrawHudNumbers(block->field_14 - 8, block->field_16, arg0->hp, val, 1);
    }
    readout->x = block->field_14;
    readout->y = block->field_16;
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
    WorldTargetNode* node;
    Task*            slot;
    GfxCoord*        player;
    GpXformScratch*  block;

    node = gWorldTargetListHead;
    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    block  = SCRATCH_STACK_RESERVE_BLOCK(GpXformScratch);
    TransposeMatrix(&player->workm, &block->mat);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) == WORLD_TARGET_NOT_LOCKABLE) {
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

    cfg  = &gPlayerStatus;
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
        type = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType;
        if (type == 1) {
            SndEvt_EnqueueType6(((gGameSession->deathVariant + 0x31) << 16) | 0x70000001, 0, 0);
        } else if (type == 3) {
            SndEvt_EnqueueType7(SOUND_AREA_BANK_ALL, 1);
            SndEvt_EnqueueType6(SOUND_SHELTER_B6_GROWTH_ALLY_DEATH, 0, 0);
        }
    }
    *arg0 = 1;
}

u8* Gp_GetAttachLevels(void)
{
    PlayerStatus* p;
    s32           cond;

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        return gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    }
    return Gp_DebugAttachLevels;
}

s32 Gp_IsDebugAttachRoom(void)
{
    PlayerStatus* p;

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        return 0;
    }
    return p->resourceVariant == 4;
}

s32 Gp_IsStateF0Active(void)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
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
    PlayerStatus*    cfg;
    HudHpMp*         hudHpMp;
    AttachmentState* attachment;

    cfg                                   = &gPlayerStatus;
    hudHpMp                               = &Gp_HpMpWork;
    hudHpMp->hp                           = cfg->hp;
    hudHpMp->mp                           = cfg->mp;
    arg0->field_16                        = -1;
    arg0->field_18                        = 0;
    attachment                            = &Gp_StateC08;
    attachment->antibodyTicks             = 0;
    attachment->antibodyCombo             = 0;
    attachment->energyShotTicks           = 0;
    attachment->energyShotCombo           = 0;
    attachment->queuedIndex               = 0;
    attachment->metabolismTicks           = 0;
    attachment->metabolismCombo           = 0;
    attachment->mindWard                  = 0;
    attachment->bodyWard                  = 0;
    attachment->mode                      = ATTACHMENT_MODE_IDLE;
    gGameSession->battleResetPending      = 0;
    Gp_ItemGrantCooldown                  = 0;
    gDisplayState.suppressDisconnectPause = 1;
    attachment->flags                    &= ~ATTACHMENT_FLAG_SWAP_LOCK;
}

static void Gp_StartPadReplay(void)
{
    DisplayState* ds;

    srand(1);
    gRandomLcgState          = 0;
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
    Gp_ReplayButtons                  = 0xFFFF;
    Gp_ReplayFramesLeft               = 1;
    Pad_RemapState->inputOverrideMode = GAME_DEBUG_INPUT_OVERRIDE_REPLAY;
}

void Gp_PlayClockState2(Task* arg0)
{
    GameSession* session;
    ScreenFade*  fade;

    arg0->killCountdown--;
    if (arg0->killCountdown <= 0) {
        arg0->killCountdown = 0;
        Gp_StartAreaBgm(&arg0->killCountdown);
        session                 = gGameSession;
        Gp_StateC08.effectPhase = ATTACHMENT_EFFECT_IDLE;
        if (session->restartMode != GAME_SESSION_RESTART_PRESERVE_DISPLAY) {
            fade             = &D_80114BD8;
            fade->blend      = SCREEN_FADE_SUBTRACT;
            fade->phase      = SCREEN_FADE_RUNNING;
            fade->rampFrames = session->deathFadeFrames;
            Task_SpawnPtr(1, 0x31, 0, fade);
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

void Gp_HudTrackSlot0(HudTargetHpReadout* readout)
{
    WorldTargetNode* target;
    Task*            work;
    GameActor*       actor;
    WorldTargetNode* node;

    work   = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    target = NULL;
    if (work != NULL) {
        actor = work->work;
        if (actor != NULL) {
            target = actor->targetNode;
        }
        node = gWorldTargetListHead;
        if (node != NULL) {
            do {
                if (node == target) {
                    if (!(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
                        Gp_HudTrackEnemy(GP_NODE_ENEMY(node), readout);
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
    return gSceneCombatState.signals.bytes.endDelayFrames == 0;
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
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrBonusItem, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    } else {
        Text_DrawPrompt(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrItemObtained, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

void Gp_DrawItemTitle(Task* arg0)
{
    UiObject* obj;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    Ui_DrawTitle(&(obj)->panel, Gp_StrItem);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (Pad_CheckButtons(0, 1, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
            obj->result = USER_INTERFACE_RESULT_CONFIRM;
        }
    }
}

void Gp_TriggerPeIfArmed(void)
{
    u8 state;

    state = gSceneCombatState.signals.bytes.battlePhase;
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
        p = &gPlayerStatus;
        if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
            cond = 0;
        } else {
            cond = p->resourceVariant == 4;
        }
        if (cond == 0) {
            table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
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

    p = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) != GAME_LOCATION_KEY(1, 20, 0, 0)) {
        cond = 0;
    } else {
        cond = p->resourceVariant == 4;
    }
    if (cond == 0) {
        table = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.attachLevels;
    } else {
        table = Gp_DebugAttachLevels;
    }
    if (arg1 != 0) {
        save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
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
    SceneCombatState* combat;
    s32               cond;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
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
    SceneCombatState* combat;
    s32               cond;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
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
    SceneCombatState* combat;
    s32               cond;

    combat = &gSceneCombatState;
    if ((combat->signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_ENGAGED && combat->battleRefs != 0) || combat->signals.bytes.endDelayFrames != 0) {
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
    if (!(Gp_StateC08.flags & ATTACHMENT_FLAG_EVENT_LOCK)) {
        Gp_StateC08.queuedIndex = arg0;
    }
}

void func_800A7DE0(void)
{
    AttachmentState* attachment;

    CdCmd_EnqueueLoadFile(0, 0, 4);
    attachment = &Gp_StateC08;
    if (attachment->mode >= ATTACHMENT_MODE_ARMED) {
        attachment->effectPhase = ATTACHMENT_EFFECT_CANCELLED;
    }
    attachment->queuedIndex        = 0;
    attachment->mode               = ATTACHMENT_MODE_IDLE;
    D_80115768                     = 0;
    gSceneCombatState.actorControl = SCENE_COMBAT_ACTORS_RUNNING;
    attachment->previewSound       = 0;
    attachment->soundStep          = ATTACHMENT_SOUND_IDLE;
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
    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (work != NULL) {
        actor = work->work;
        p     = &gPlayerStatus;
        if (actor->mode == GAME_ACTOR_MODE_NORMAL) {
            if (actor->state == 0 || actor->state == 2) {
                if (gGameSession->dirActionBusy == 0) {
                    if (p->interactionPressed == 0) {
                        flag = 1;
                    }
                }
            }
        }
    }
    if (arg0 == 0) {
        if (Gp_StateC08.flags & ATTACHMENT_FLAG_SWAP_LOCK) {
            flag = 0;
        }
    }
    if (flag != 0) {
        if (Gp_ItemGrantCooldown <= 0) {
            if (gSceneCombatState.signals.bytes.endDelayFrames == 0) {
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

    p   = &gPlayerStatus;
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
    _GfxRelativeTransformScratch* scratch;
    MATRIX*                       rootm;
    MATRIX*                       world;
    MATRIX*                       out;

    Gp_UpdateCoord(arg0);
    Gp_UpdateCoord(root);

    rootm   = &root->workm;
    world   = &arg0->workm;
    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxRelativeTransformScratch);
    out     = &result->coord;

    gte_TransposeMatrix(rootm, &scratch->transposedRotation);

    gte_MulMatrix0(&scratch->transposedRotation, world, out);

    scratch->originDelta.vx = world->t[0] - rootm->t[0];
    scratch->originDelta.vy = world->t[1] - rootm->t[1];
    scratch->originDelta.vz = world->t[2] - rootm->t[2];
    // The SDK writes only XYZ into the matrix's three-word translation.
    ApplyMatrixLV(&scratch->transposedRotation, &scratch->originDelta, (VECTOR*)out->t);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxRelativeTransformScratch);
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
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    ViewCamera*      camera;
    GfxCoord*        c1;
    MATRIX*          rot;
    VECTOR3*         trans;
    u8               idx;

    sess        = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[sess->stage - 1];
    cameras     = cameraTable->cameras[sess->area - 1];
    idx         = Gp_GetViewIndex();

    rot    = &gGfxViewRotCoord.coord;
    trans  = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1     = &Gfx_ViewOffsetCoord;
    camera = gpViewAt(cameras, idx);

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(_ViewRotation*)rot->m = *(_ViewRotation*)(camera - 1)->transform.m;
    *trans                  = *MATRIX_TRANS(&(camera - 1)->transform);

    camera--;

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = camera->screenDistance;
    gte_SetGeomScreen(camera->screenDistance);
    gte_SetGeomOffset(0, 0);

    Gfx_ViewOffsetCoord.composeStamp                  = GRAPHICS_COORD_DIRTY;
    PARENT_OF(rot, GfxCoord, coord)->composeStamp     = GRAPHICS_COORD_DIRTY;
    PARENT_OF(trans, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Writes the translation component of a reference-relative transform.
///
/// The input origins share a containing frame. Computes
/// `out->t = scratch->transposedRotation.m * (target->t - reference->t)`
/// with GTE long-vector arithmetic, preserving signed 32-bit coordinate units.
/// The caller must supply the reference rotation's transpose in
/// `scratch->transposedRotation`, with `ONE` (4096) representing 1.0.
/// The transpose gives an inverse frame conversion for orthonormal rotations.
///
/// All pointers must be word-aligned and the caller-owned workspace disjoint
/// from the matrices. Overwrites `scratch->originDelta` XYZ; its fourth word
/// is unused. Saves all three differences before storing the three `out->t`
/// words, so `out` may equal either input matrix. Leaves the output rotation
/// and alignment bytes untouched. Changes GTE rotation and arithmetic state;
/// retains no pointers and does not reserve or release the workspace.
static __inline__ void _gfxWriteRelativeTranslation(const MATRIX* reference, const MATRIX* target, MATRIX* out,
                                                    _GfxRelativeTransformScratch* scratch)
{
    scratch->originDelta.vx = target->t[0] - reference->t[0];
    scratch->originDelta.vy = target->t[1] - reference->t[1];
    scratch->originDelta.vz = target->t[2] - reference->t[2];
    // The SDK output view writes XYZ only, without a VECTOR's fourth word.
    ApplyMatrixLV(&scratch->transposedRotation, &scratch->originDelta, (VECTOR*)out->t);
}

void gfxMakeRelativeTransform(const MATRIX* reference, const MATRIX* target, MATRIX* out)
{
    _GfxRelativeTransformScratch* scratch;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_GfxRelativeTransformScratch);

    // Save the reference rotation before writing a possibly aliased output.
    gte_TransposeMatrix(reference, &scratch->transposedRotation);
    gte_MulMatrix0(&scratch->transposedRotation, target, out);
    _gfxWriteRelativeTranslation(reference, target, out, scratch);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxRelativeTransformScratch);
}

s32 Gp_TrySpawnViewTask(ViewCamera* camera)
{
    return Task_Spawn(0, 0xF, 0, camera) != NULL;
}

void Gp_ApplyView(ViewCamera* camera)
{
    GfxCoord* c1;
    MATRIX*   rot;
    VECTOR3*  trans;

    rot   = &gGfxViewRotCoord.coord;
    trans = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1    = &Gfx_ViewOffsetCoord;

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(_ViewRotation*)rot->m = *(_ViewRotation*)camera->transform.m;
    *trans                  = *MATRIX_TRANS(&camera->transform);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = camera->screenDistance;
    gte_SetGeomScreen(camera->screenDistance);
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
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    ViewCamera*      camera;
    u8               idx;

    sess        = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[sess->stage - 1];
    cameras     = cameraTable->cameras[sess->area - 1];
    idx         = Gp_GetViewIndex();
    camera      = gpViewAt(cameras, idx);
    Task_SpawnPtr(0, 0xF, 0, (camera - 1));
    Task_Spawn(0, 0x17, 0, 0);
}

ViewCamera* Gp_GetStageView(GameLocationKey* arg0)
{
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    u8               idx;

    cameraTable = Gp_ViewTables[arg0->stage - 1];
    cameras     = cameraTable->cameras[arg0->area - 1];
    idx         = Gp_GetViewIndex();
    return &cameras[idx - 1];
}

void Gp_ApplyViewTask(Task* task)
{
    GfxCoord*   c1;
    MATRIX*     rot;
    VECTOR3*    trans;
    ViewCamera* camera;

    rot    = &gGfxViewRotCoord.coord;
    trans  = MATRIX_TRANS(&gGfxViewCoord.coord);
    c1     = &Gfx_ViewOffsetCoord;
    camera = task->spawnArg2.pointer;

    // Keep rotation and translation in their separate camera coordinate nodes.
    *(_ViewRotation*)rot->m = *(_ViewRotation*)camera->transform.m;
    *trans                  = *MATRIX_TRANS(&camera->transform);

    c1->coord.t[0] = 0;
    c1->coord.t[1] = 0;
    c1->coord.t[2] = 0;

    gDisplayState.screenDistance = camera->screenDistance;
    gte_SetGeomScreen(camera->screenDistance);
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
    ViewCameraTable* cameraTable;
    ViewCamera*      cameras;
    ViewCamera*      camera;
    u8               idx;

    sess        = &gGameSession->location.loc;
    cameraTable = Gp_ViewTables[sess->stage - 1];
    cameras     = cameraTable->cameras[sess->area - 1];
    idx         = Gp_GetViewIndex();
    camera      = gpViewAt(cameras, idx);
    Task_SpawnPtr(0, 0xF, 0, (camera - 1));
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
    save = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
    if (task->spawnArg1.value != save->state.location.loc.view) {
        gGameSession->viewDirty = 1;
    }
    sess = gGameSession;
    if (sess->viewDirty != 0) {
        q = &gCdCmdQueue;
        if ((q->scenePayloadAvailable == 0) || (q->scenePayloadLoading == 0)) {
            sess->location.loc.view = save->state.location.loc.view;
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
