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

const char D_800938AC[8] = "????\0&!K";

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

/// Scratch-stack block in which the locked-on enemy's HP readout is placed for one frame.
///
/// The position starts as the anchor the readout is heading for. For a newly
/// locked enemy it stays there; otherwise it is replaced by the position kept
/// in `HudTargetHpReadout` advanced by one step toward the anchor. The readout
/// is drawn at the result, which is then stored back for the next frame.
/// Coordinates are pixels from the screen center, Y increasing downward.
///
/// Reserve the complete record on the scratch stack; none of its members
/// survive the matching release. The leading bytes are reserved with the block
/// and left untouched, so what the block was laid out to hold there is unproven.
typedef struct {
    byte unknown_0[0x14]; // Reserved with the block and never accessed; role unproven
    s16  x;               // Horizontal position: the anchor, then where the readout is drawn
    s16  y;               // Vertical position: the anchor, then where the readout is drawn
    s16  stepX;           // Horizontal distance left to the anchor, then an eighth of it, rounded down
    s16  stepY;           // Vertical distance left to the anchor, then an eighth of it, rounded down
} _HudTargetHpReadoutScratch;
STATIC_ASSERT_SIZEOF(_HudTargetHpReadoutScratch, 0x1C);

/// Stack workspace for one HUD hit-point readout.
///
/// `Gp_DrawHudNumbers` keeps a single 48-byte slot, the size of a `UiObject`.
/// Separate locals would not share it. The function stores a zero content
/// origin, ordering-table index -3 and the initial panel state through
/// `uiObject`, then uses the same bytes for the amount text and the frame.
/// It does not read those panel fields back. The "HP" label is a separate
/// request, not part of this slot.
///
/// When the maximum is known, `value` holds the current amount: `digits`
/// receives the decimal text and `request` draws it. The conversion writes at
/// most ten bytes: nine digits and a terminator. The request follows those
/// sixteen bytes, clear of the text. When the maximum is negative,
/// `hiddenAmount` draws the stand-in string from the bytes `digits` occupies.
/// `frame.rect` is the outer rectangle passed to `uiDrawRectFrame` after
/// that text, on both paths, and it starts at the same byte as `value.request`.
typedef union {
    UiObject uiObject;            // Content origin, ordering-table index and initial panel state
    struct {
        u8          digits[0x10]; // Decimal text of the current amount, NUL-terminated
        TextDrawReq request;      // Placement and style for `digits`
    } value;
    TextDrawReq hiddenAmount;     // Stand-in string when the maximum is negative
    struct {
        u8   amountBytes[0x10];   // Same bytes as `value.digits` and `hiddenAmount`; not read here
        RECT rect;                // Outer rectangle in draw-environment pixels
    } frame;
} _HudHpReadoutScratch;
STATIC_ASSERT_SIZEOF(_HudHpReadoutScratch, 0x30);
STATIC_ASSERT(OFFSET_OF(_HudHpReadoutScratch, value.request) == 0x10, hud_hp_readout_value_request);
STATIC_ASSERT(OFFSET_OF(_HudHpReadoutScratch, frame.rect) == 0x10, hud_hp_readout_frame);

/// Scratch-stack block for placing one tracked target in the player's frame.
///
/// The player's frame is the first coordinate of the player's model: its
/// origin is the player's world position, and `playerInverseRotation` turns
/// world axes into the player's when that rotation is orthonormal. `position`
/// is one target's point in whichever space the work has reached. The
/// per-frame refresh takes it from `Enemy::bodyPos`, local to the enemy's
/// coordinate, through world space into the player's frame, and stores the
/// result as `Enemy::playerRelPos`. The radar starts from that stored value
/// flattened onto the ground plane, scales it by the radar zoom, and rounds it
/// to the blip's pixel offset from the radar center.
///
/// Both users reserve the complete record on the scratch stack, although the
/// radar touches only `position`; none of its members survive the matching
/// release. The bytes between the two members are reserved with the block and
/// left untouched, so what the block was laid out to hold there is unproven.
typedef struct {
    MATRIX  playerInverseRotation; // Transpose of the player's world rotation, 12 fractional bits (`ONE` is 1.0); translation unused
    byte    unknown_20[0x20];      // Reserved with the block and never accessed; role unproven
    SVECTOR position;              // One target's X, Y, Z in game units, or radar pixels once rounded; fourth word unused
} _WorldTargetPlayerFrameScratch;
STATIC_ASSERT_SIZEOF(_WorldTargetPlayerFrameScratch, 0x48);

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

/// Preserves the camera cursor supplied by the area-table accessor.
///
/// The cursor uses a mapped 1-based index; users step back to the selected
/// record before reading it. The inline boundary preserves scaled-index-first
/// address evaluation required by the callers' matching instruction order.
static inline ViewCamera* _viewCameraCursorRef(ViewCamera* cursor)
{
    return cursor;
}

/// Resolve a camera-record cursor within its loaded room resource.
#define gpViewAt(rows, index) _viewCameraCursorRef(&(rows)[index])

static void Gp_HudTrackEnemy(Enemy* arg0, HudTargetHpReadout* readout);

static inline void _worldTargetRotatePosition(const MATRIX* rotation, SVECTOR* position);

static void Gp_StartPadReplay(void);

static s32 _sceneIsBattleEndDelayClear(void);

static s32 _attachmentMakeTextId(s32 abilityIndex, s32 level);

static s32 Gp_StepAttachSlot(s32 arg0, s32 arg1);

static void Gp_EnqueueSndCdIfF0(u8 arg0);

static s32 Gp_CdIdleIfF0Active(void);

static s32 func_800A7E5C(s32 arg0);

static s32 func_800A7F2C(s32 value);

static __inline__ void _gfxCoordToReference(GfxCoord* coord, GfxCoord* reference, GfxCoord* outCoord);

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

static void _viewSetFromCoord(GfxCoord* cameraCoord, const VECTOR* offset);

/// Spawns the type-0xE view task and points its coordinate at the inverse of
/// `arg0` (transposed rotation, negated translation). `arg1` is the optional
/// world offset stored in the task's 0x10-byte payload.
static s32 Gp_SpawnViewCoordTask(GfxCoord* arg0, VECTOR* arg1);

static void _viewResetTransform(void);

static void func_800A8D5C(void);

/// Installs a camera record in the active view chain and resets projection.
///
/// Copies the ONE-scaled world-to-camera rotation and negated world origin
/// into their separate nodes, clears the outer XYZ offset, and invalidates
/// all three composition caches. Parent links and the remaining matrix
/// components stay intact. Projection uses the low 16 bits of
/// `camera->screenDistance` in pixels, with a screen offset of (0, 0).
///
/// Requires an initialized view chain and a readable, word-aligned camera
/// disjoint from its nodes. Borrows the record only for the call, retains no
/// pointer, and changes GTE projection state without composing the view.
static __inline__ void _viewWriteCameraState(const ViewCamera* camera)
{
    GfxCoord*      viewOffset;
    _ViewRotation* viewRotation;
    VECTOR3*       viewTranslation;

    viewRotation    = (_ViewRotation*)gGfxViewRotCoord.coord.m;
    viewTranslation = MATRIX_TRANS(&gGfxViewCoord.coord);
    viewOffset      = &Gfx_ViewOffsetCoord;

    // Copy only coefficients and XYZ into separate nodes; leave matrix alignment bytes intact.
    *viewRotation    = *(const _ViewRotation*)camera->transform.m;
    *viewTranslation = *(const VECTOR3*)camera->transform.t;

    viewOffset->coord.t[0] = 0;
    viewOffset->coord.t[1] = 0;
    viewOffset->coord.t[2] = 0;

    gDisplayState.screenDistance = camera->screenDistance;
    gte_SetGeomScreen(camera->screenDistance);
    gte_SetGeomOffset(0, 0);

    viewOffset->composeStamp                                    = GRAPHICS_COORD_DIRTY;
    PARENT_OF(viewRotation, GfxCoord, coord.m)->composeStamp    = GRAPHICS_COORD_DIRTY;
    PARENT_OF(viewTranslation, GfxCoord, coord.t)->composeStamp = GRAPHICS_COORD_DIRTY;
}

#undef DRAW_PROMPT_LABEL
#undef DRAW_PROMPT_COUNT

void Gp_DrawHudSprites(HudState* hud)
{
    _WorldTargetPlayerFrameScratch* block;
    WorldTargetNode*                node;
    s32                             mode;
    s32                             x;
    s32                             cx;
    s32                             cy;
    s32                             y;
    s16                             vx;
    s32                             vz;
    s32                             i;
    s32                             n;
    s32                             range;
    DR_TPAGE*                       tp;
    SPRT*                           sp;
    SPRT*                           sp2;
    POLY_GT4*                       poly;

    x  = 0x61;
    y  = -0x6C;
    y -= gDisplayState.vramYOffset;
    cx = x + 0x23;
    cy = y + 0x23;
    func_800A63B4(cx, cy, 0);
    node  = gWorldTargetListHead;
    block = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetPlayerFrameScratch);
    mode  = func_800B9D80(0x400);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) != WORLD_TARGET_NOT_LOCKABLE) {
            block->position.vx = GP_NODE_ENEMY(node)->playerRelPos.vx;
            block->position.vz = GP_NODE_ENEMY(node)->playerRelPos.vz;
            block->position.vy = 0;
            if (mode == 0) {
                gte_lddp(0x1555);
                gte_ldsv(&block->position);
                gte_gpf12();
                gte_stsv(&block->position);
            } else {
                gte_lddp(0xAAA);
                gte_ldsv(&block->position);
                gte_gpf12();
                gte_stsv(&block->position);
            }
            if (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
                continue;
            }
            vx = block->position.vx;
            if (vx < -0x1300 || vx > 0x1300) {
                continue;
            }
            vz = block->position.vz;
            if (vz > 0x1300) {
                continue;
            }
            if (vz < -0x1300) {
                continue;
            }
            if (vx * vx + vz * vz > 0x168FFFF) {
                continue;
            }
            block->position.vx = (s16)(vx + 0x80) >> 8;
            vz                 = (s16)(block->position.vz + 0x80) >> 8;
            block->position.vz = vz;
            if (node->state.parts.targeted != 0) {
                func_800A63B4(cx + block->position.vx, cy - vz, 2);
            } else {
                func_800A63B4(cx + block->position.vx, cy - vz, 1);
            }
        }
    }
    range = hud->radarRange;
    if (mode == 0) {
        range *= 2;
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
    if (hud->radarRangeIcon != HUD_RADAR_RANGE_NONE) {
        sp2            = gGpuPrimCursor;
        gGpuPrimCursor = sp2 + 1;
        sp2->x0        = x + 0xD;
        sp2->y0        = y + 0xC;
        sp2->h         = 0x28;
        sp2->w         = 0x28;
        if (hud->radarRangeIcon != HUD_RADAR_RANGE_PROJECTILE) {
            if (hud->radarRangeIcon == HUD_RADAR_RANGE_AROUND) {
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
        uiQueueTexturePage(-3, 1);
        n = range;
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
        hud->radarRangeIcon = HUD_RADAR_RANGE_NONE;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetPlayerFrameScratch);
}

void Gp_DrawHudNumbers(s32 x, s32 y, s32 cur, s32 max, s32 kind)
{
    _HudHpReadoutScratch scratch;
    TextDrawReq          req;
    TILE*                tile;
    SPRT*                sp;
    POLY_FT4*            poly;
    s32                  span;
    s32                  right;
    s32                  w;
    s32                  order;

    span = 0x25;
    if (cur < 0) {
        cur = 0;
    }
    y -= gDisplayState.vramYOffset;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }

    // Panel view of the readout slot. The amount text and frame reuse these bytes.
    order                                               = -3;
    scratch.uiObject.panel.contentOriginX.unsignedValue = 0;
    scratch.uiObject.panel.contentOriginY.unsignedValue = 0;
    scratch.uiObject.panel.otIndex.signedValue          = order;
    scratch.uiObject.panel.state                        = USER_INTERFACE_PANEL_INITIAL;

    req.x          = x + 4;
    req.y          = y + 8;
    req.otIndex    = -2;
    req.colorRgb   = 0x606060;
    req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
    req.alignment  = TEXT_ALIGNMENT_LEFT;
    req.drawMode   = TEXT_DRAW_OUTLINED;
    textDrawString(&req, Gp_StrHP);

    if (max >= 0) {
        s32 val = cur;

        if (kind == 0) {
            s32 tx = x + 0x2B;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            scratch.value.request.x          = tx;
            scratch.value.request.y          = ty;
            scratch.value.request.otIndex    = -2;
            scratch.value.request.colorRgb   = 0x606060;
            scratch.value.request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            scratch.value.request.alignment  = TEXT_ALIGNMENT_RIGHT;
            scratch.value.request.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&scratch.value.request, textItoaUnsigned(scratch.value.digits, val));
        } else {
            s32 tx = x + 0x33;
            s32 ty = y + 0xA;

            if (val < 0) {
                val = 0;
            }
            scratch.value.request.x          = tx;
            scratch.value.request.y          = ty;
            scratch.value.request.otIndex    = -2;
            scratch.value.request.colorRgb   = 0x606060;
            scratch.value.request.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
            scratch.value.request.alignment  = TEXT_ALIGNMENT_RIGHT;
            scratch.value.request.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
            textDrawString(&scratch.value.request, textItoaUnsigned(scratch.value.digits, val));
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
        scratch.hiddenAmount.x          = x + 0x33;
        scratch.hiddenAmount.y          = y + 0xA;
        scratch.hiddenAmount.otIndex    = -2;
        scratch.hiddenAmount.colorRgb   = 0x37A78;
        scratch.hiddenAmount.glyphTable = TEXT_GLYPH_TABLE_MEDIUM;
        scratch.hiddenAmount.alignment  = TEXT_ALIGNMENT_RIGHT;
        scratch.hiddenAmount.drawMode   = TEXT_DRAW_TRANSLUCENT_OUTLINED;
        textDrawString(&scratch.hiddenAmount, D_800938AC);
        span = 0x2D;
    }

    // The frame reuses the value request's bytes after the amount has been drawn.
    scratch.frame.rect.x = x;
    scratch.frame.rect.y = y;
    scratch.frame.rect.w = span + 0xA;
    scratch.frame.rect.h = 0x14;
    uiDrawRectFrame(&scratch.frame.rect, -1, 0x40002, NULL);
}

static void Gp_HudTrackEnemy(Enemy* arg0, HudTargetHpReadout* readout)
{
    _HudTargetHpReadoutScratch* scratch;
    s32                         val;

    scratch = SCRATCH_STACK_RESERVE_BLOCK(_HudTargetHpReadoutScratch);
    if (func_800B9D80(0x100000) != 0) {
        scratch->x = 0x6A;
        scratch->y = -0x35;
    } else {
        scratch->x = 0x6A;
        scratch->y = -0x64;
    }
    if (readout->enemy != arg0) {
        // A new target starts at the anchor. The second store also lands in
        // `x`; both members are rewritten after the draw.
        readout->enemy = arg0;
        readout->x     = scratch->x;
        readout->x     = scratch->y;
    } else {
        // The same target eases an eighth of the way from where it was last
        // drawn toward the anchor.
        scratch->stepX   = scratch->x - readout->x;
        scratch->stepY   = scratch->y - readout->y;
        scratch->stepX >>= 3;
        scratch->stepY >>= 3;
        scratch->x       = readout->x + scratch->stepX;
        scratch->y       = readout->y + scratch->stepY;
    }
    if (arg0->param != NULL) {
        val = arg0->param->hpMax;
        if (arg0->node.state.parts.flags & WORLD_TARGET_HIDE_HP) {
            val = -1;
        }
        Gp_DrawHudNumbers(scratch->x - 8, scratch->y, arg0->hp, val, 1);
    }
    readout->x = scratch->x;
    readout->y = scratch->y;
    SCRATCH_STACK_RELEASE_BLOCK(_HudTargetHpReadoutScratch);
}

/// Rotates a tracked position in place with GTE signed-halfword saturation.
///
/// `rotation` uses ONE (4096) for 1.0; translation is ignored. Reads the
/// complete eight-byte position through a snapshot and replaces XYZ only,
/// preserving its pad. Changes GTE rotation, vector and arithmetic state.
/// The matrix must be word-aligned and the position halfword-aligned.
static inline void _worldTargetRotatePosition(const MATRIX* rotation, SVECTOR* position)
{
    SVECTOR inputPosition;

    inputPosition = *position;
    gte_ApplyMatrixSV(rotation, &inputPosition, position);
}

void Gp_UpdateLinkXforms(void)
{
    WorldTargetNode*                node;
    Task*                           slot;
    GfxCoord*                       player;
    _WorldTargetPlayerFrameScratch* block;

    node = gWorldTargetListHead;
    slot = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (slot == NULL) {
        return;
    }
    player = slot->extra.tmd->coords;
    block  = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetPlayerFrameScratch);
    TransposeMatrix(&player->workm, &block->playerInverseRotation);
    for (; node != NULL; node = node->next) {
        if ((node->state.word & WORLD_TARGET_SCAN_MASK) == WORLD_TARGET_NOT_LOCKABLE) {
            continue;
        }
        block->position.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        block->position.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        block->position.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        _worldTargetRotatePosition(&GP_NODE_ENEMY(node)->coord->workm, &block->position);
        block->position.vx += GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->position.vy += GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->position.vz += GP_NODE_ENEMY(node)->coord->workm.t[2];
        block->position.vx -= player->workm.t[0];
        block->position.vy -= player->workm.t[1];
        block->position.vz -= player->workm.t[2];
        _worldTargetRotatePosition(&block->playerInverseRotation, &block->position);
        GP_NODE_ENEMY(node)->playerRelPos.vx = block->position.vx;
        GP_NODE_ENEMY(node)->playerRelPos.vy = block->position.vy;
        GP_NODE_ENEMY(node)->playerRelPos.vz = block->position.vz;
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetPlayerFrameScratch);
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
        sndEvtRequestScriptStart((gGameSession->deathVariant << 16) | 0x70000001, 0, 0);
    } else {
        type = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.companionType;
        if (type == 1) {
            sndEvtRequestScriptStart(((gGameSession->deathVariant + 0x31) << 16) | 0x70000001, 0, 0);
        } else if (type == 3) {
            sndEvtRequestScriptStop(SOUND_AREA_BANK_ALL, SOUND_SCRIPT_STOP_KEEP_RELEASE);
            sndEvtRequestScriptStart(SOUND_SHELTER_B6_GROWTH_ALLY_DEATH, 0, 0);
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

s32 attachmentIsTrainingMode(void)
{
    enum { ATTACHMENT_TRAINING_RESOURCE_VARIANT = 4 };
    PlayerStatus* player;

    player = &gPlayerStatus;
    if ((GAME_LOCATION_WORD(gGameSession->location.loc) & GAME_LOCATION_STAGE_AREA_MASK) !=
        GAME_LOCATION_KEY(GAME_STAGE_ACROPOLIS, GAME_AREA_MIST_SHOOTING_GALLERY, 0, 0)) {
        return 0;
    }
    return player->resourceVariant == ATTACHMENT_TRAINING_RESOURCE_VARIANT;
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
    Gp_ApplyAttachStats(1, NULL);
    return 0;
}

void Gp_ResetHudFx(HudState* hud)
{
    PlayerStatus*    cfg;
    HudHpMp*         hudHpMp;
    AttachmentState* attachment;

    cfg                                   = &gPlayerStatus;
    hudHpMp                               = &Gp_HpMpWork;
    hudHpMp->hp                           = cfg->hp;
    hudHpMp->mp                           = cfg->mp;
    hud->radarRangeIcon                   = HUD_RADAR_RANGE_NONE;
    hud->radarRange                       = 0;
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
        Gp_ReplayCursor = (u16*)(FILE_SYSTEM_FIXED_REPLAY_BASE + 0xD4C);
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

/// Returns whether the scene's post-battle hold has no frames remaining.
static s32 _sceneIsBattleEndDelayClear(void)
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
            uiSetPanelContentSize(&(obj)->panel, textMeasureLineWidth(Gp_StrBonusItem) + 0xA, 0);
            obj->panel.bounds.unsignedRect.x -= 0xF;
            obj->panel.bounds.unsignedRect.y += 9;
            arg0->state++;
        }
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrBonusItem, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    } else {
        textDrawUiLine(obj, obj->panel.contentLeft.signedValue + 6, 7, Gp_StrItemObtained, 0x606060, TEXT_DRAW_OUTLINED, TEXT_ALIGNMENT_LEFT);
    }
}

void Gp_DrawItemTitle(Task* arg0)
{
    UiObject* obj;

    obj         = arg0->spawnArg2.pointer;
    obj->result = USER_INTERFACE_RESULT_NONE;
    uiDrawTitle(&(obj)->panel, Gp_StrItem);
    if (obj->panel.control.word == USER_INTERFACE_PANEL_ACTIVE) {
        if (padCheckButtons(0, PAD_BUTTON_QUERY_PRESSED, Pad_MaskConfirm | Pad_MaskCancel) != 0) {
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

/// Packs an ability index and level into its Parasite Energy text identifier.
///
/// Ability indices 0..17 occupy groups of three in bits 4 and above, with
/// the within-group index in bits 2..3. Level 0..3 occupies bits 0..1;
/// text lookup treats level 0 as level 1. Inputs are not checked or masked.
static s32 _attachmentMakeTextId(s32 abilityIndex, s32 level)
{
    enum { ATTACHMENT_TEXT_ID_BASE = 0x300 };

    return (abilityIndex / 3) * 16 + (abilityIndex % 3) * 4 + level + ATTACHMENT_TEXT_ID_BASE;
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

void viewChangeStub(void)
{
}

/// Subtracts 16 from a signed value; its domain and purpose are unproven.
static s32 func_800A7F2C(s32 value)
{
    return value - 0x10;
}

s32 playerStateSpendMp(s32 amount)
{
    PlayerStatus* player;
    s32           fullyPaid;

    player    = &gPlayerStatus;
    fullyPaid = 1;
    if (player->mp >= amount) {
        player->mp -= amount;
    } else {
        player->mp = 0;
        fullyPaid  = 0;
    }
    return fullyPaid;
}

/// Writes a coordinate's transform relative to another composed coordinate.
///
/// Refreshes `coord` then `reference` through their live, acyclic parent chains.
/// Their caches must compose into the same frame. Writes only `outCoord->coord`
/// rotation and translation: transpose(reference.workm.m) times coord.workm.m
/// and the origin difference. The transpose inverts an orthonormal rotation;
/// coefficients use ONE (4096), translations signed game-coordinate units.
/// Other output fields and matrix alignment bytes remain untouched. The output
/// local matrix must be disjoint from both input cached matrices and scratch;
/// `outCoord` may be either input node. The caller invalidates its composition
/// stamp when needed. Requires one 48-byte scratch block, released before return;
/// changes GTE rotation and arithmetic state and retains no pointers.
static __inline__ void _gfxCoordToReference(GfxCoord* coord, GfxCoord* reference, GfxCoord* outCoord)
{
    _GfxRelativeTransformScratch* scratch;
    MATRIX*                       referenceMatrix;
    MATRIX*                       coordMatrix;
    MATRIX*                       outMatrix;

    actorRenderComposeCoord(coord);
    actorRenderComposeCoord(reference);

    referenceMatrix = &reference->workm;
    coordMatrix     = &coord->workm;
    scratch         = SCRATCH_STACK_RESERVE_BLOCK(_GfxRelativeTransformScratch);
    outMatrix       = &outCoord->coord;

    gte_TransposeMatrix(referenceMatrix, &scratch->transposedRotation);

    gte_MulMatrix0(&scratch->transposedRotation, coordMatrix, outMatrix);

    _gfxWriteRelativeTranslation(referenceMatrix, coordMatrix, outMatrix, scratch);

    SCRATCH_STACK_RELEASE_BLOCK(_GfxRelativeTransformScratch);
}

/// Sets the active view from a camera coordinate and an optional outer offset.
///
/// The view applies negated origin before transposed rotation, in signed game
/// units and ONE-scaled coefficients. A direct child of `gGfxViewCoord` uses
/// its local transform; other coordinates are composed relative to that node.
/// The latter path requires a live acyclic chain and 48 free scratch bytes.
/// `offset` supplies XYZ after rotation, or NULL clears it. Invalidates the
/// source and all three view caches; projection settings and parents survive.
/// Input storage must be disjoint from the active view nodes and scratch stack.
static void _viewSetFromCoord(GfxCoord* cameraCoord, const VECTOR* offset)
{
    GfxCoord* viewOrigin;
    GfxCoord* cameraParent;
    GfxCoord  relative;

    if (offset != NULL) {
        Gfx_ViewOffsetCoord.coord.t[0] = offset->vx;
        Gfx_ViewOffsetCoord.coord.t[1] = offset->vy;
        Gfx_ViewOffsetCoord.coord.t[2] = offset->vz;
    } else {
        Gfx_ViewOffsetCoord.coord.t[0] = 0;
        Gfx_ViewOffsetCoord.coord.t[1] = 0;
        Gfx_ViewOffsetCoord.coord.t[2] = 0;
    }

    // A direct child already expresses its camera pose in the required frame.
    cameraParent = cameraCoord->parent;
    viewOrigin   = &gGfxViewCoord;
    if (cameraParent == viewOrigin) {
        gte_TransposeMatrix(&cameraCoord->coord, &gGfxViewRotCoord.coord);
        viewOrigin->coord.t[0] = -cameraCoord->coord.t[0];
        viewOrigin->coord.t[1] = -cameraCoord->coord.t[1];
        viewOrigin->coord.t[2] = -cameraCoord->coord.t[2];
    } else {
        _gfxCoordToReference(cameraCoord, viewOrigin, &relative);
        gte_TransposeMatrix(&relative.coord, &gGfxViewRotCoord.coord);
        viewOrigin->coord.t[0] = -relative.coord.t[0];
        viewOrigin->coord.t[1] = -relative.coord.t[1];
        viewOrigin->coord.t[2] = -relative.coord.t[2];
    }
    cameraCoord->composeStamp = GRAPHICS_COORD_DIRTY;

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
        _gfxCoordToReference(arg0, root, &rel);
        gte_TransposeMatrix(&rel.coord, &coord->coord);
        coord->coord.t[0] = -rel.coord.t[0];
        coord->coord.t[1] = -rel.coord.t[1];
        coord->coord.t[2] = -rel.coord.t[2];
    }
    return 1;
}

void viewApplyCoordTask(Task* task)
{
    const VECTOR*         offset;
    const GfxCoord*       cameraCoord;
    GfxCoord*             viewOffset;
    GfxCoord*             viewOrigin;
    ModelObjectCoordBody* body;
    s32                   row;
    s32                   column;

    row                    = 0;
    viewOffset             = &Gfx_ViewOffsetCoord;
    body                   = task->extra.coordBody;
    offset                 = task->work;
    cameraCoord            = body->coord;
    viewOffset->coord.t[0] = offset->vx;
    viewOffset->coord.t[1] = offset->vy;
    viewOffset->coord.t[2] = offset->vz;

    // Transfer only the nine coefficients; the origin belongs to its own node.
    for (; row < (s32)ARRAY_SIZE(gGfxViewRotCoord.coord.m); row++) {
        for (column = 0; column < (s32)ARRAY_SIZE(gGfxViewRotCoord.coord.m[row]); column++) {
            gGfxViewRotCoord.coord.m[row][column] = cameraCoord->coord.m[row][column];
        }
    }

    viewOrigin             = &gGfxViewCoord;
    viewOrigin->coord.t[0] = cameraCoord->coord.t[0];
    viewOrigin->coord.t[1] = cameraCoord->coord.t[1];
    viewOrigin->coord.t[2] = cameraCoord->coord.t[2];

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
    idx         = viewGetMappedIndex();

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

void viewApplyCamera(const ViewCamera* camera)
{
    _viewWriteCameraState(camera);
}

/// Restores identity view rotation and zero origin with outer Z at ONE (4096 game units).
///
/// Invalidates the three caches while preserving their parent links and all
/// projection settings.
static void _viewResetTransform(void)
{
    MATRIX*   rotation;
    GfxCoord* viewOffset;
    GfxCoord* viewRotation;
    GfxCoord* viewOrigin;
    s32       one;

    viewOffset             = &Gfx_ViewOffsetCoord;
    one                    = ONE;
    viewOffset->coord.t[0] = 0;
    viewOffset->coord.t[1] = 0;
    viewOffset->coord.t[2] = one;

    rotation     = &gGfxViewRotCoord.coord;
    viewRotation = PARENT_OF(rotation, GfxCoord, coord);
    gfxSetRotIdentity(rotation);

    viewOrigin                 = &gGfxViewCoord;
    viewOrigin->coord.t[0]     = 0;
    viewOrigin->coord.t[1]     = 0;
    viewOrigin->coord.t[2]     = 0;
    viewOffset->composeStamp   = GRAPHICS_COORD_DIRTY;
    viewRotation->composeStamp = GRAPHICS_COORD_DIRTY;
    viewOrigin->composeStamp   = GRAPHICS_COORD_DIRTY;
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
    idx         = viewGetMappedIndex();
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
    idx         = viewGetMappedIndex();
    return &cameras[idx - 1];
}

void viewApplyCameraTask(Task* task)
{
    const ViewCamera* camera;

    camera = task->spawnArg2.pointer;
    _viewWriteCameraState(camera);
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
    idx         = viewGetMappedIndex();
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
            displayReleaseMenuHold();
            gGameSession->viewReady = 1;
            task->state             = 3;
        }
    }
}
