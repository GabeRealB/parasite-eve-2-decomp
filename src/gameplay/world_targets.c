#include "world_targets.h"
#include "scene_combat.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/gtemac.h>
#include <psyq/inline_c.h>
#include <psyq/stdio.h>

#include "common.h"
#include "gte.h"

#include "gameplay/actor_render.h"
#include "gameplay/area_entry.h"
#include "gameplay/attachment_state.h"
#include "gameplay/attachments.h"
#include "gameplay/battle_reward.h"
#include "gameplay/collision.h"
#include "gameplay/damage.h"
#include "gameplay/enemy.h"
#include "gameplay/gpu_image_upload.h"
#include "geometry.h"
#include "gameplay/hud_sprites.h"
#include "item_use.h"
#include "gameplay/items.h"
#include "items.h"
#include "gameplay/enemy_params.h"
#include "gameplay/scene.h"
#include "gameplay/world_collision.h"
#include "world_coords.h"
#include "gameplay/world_state.h"

#include "main/display.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/text.h"
#include "main/ui.h"

/// Scratch-stack block holding the line-of-sight segment of one lock-on scan.
///
/// The aim point is the aiming actor's world position raised by 1000 units.
/// Each candidate target's body point is placed in view space, and the target
/// can be locked only while no world occluder blocks the segment between the
/// two view-space points. The occluder test maps occluder geometry into the
/// same space through the current view matrix.
typedef struct {
    SVECTOR eyeView;      // Aim point in view space; segment end
    SVECTOR targetView;   // Candidate's body point, local to its coordinate until transformed into view space; segment start
    SVECTOR eyeWorld;     // Aim point in world space, before the view transform
    byte    unused18[32]; // Reserved with the block but never accessed; role unproven
} _WorldTargetLockScanScratch;
STATIC_ASSERT_SIZEOF(_WorldTargetLockScanScratch, 0x38);

/// Scratch-stack reservation for converting a target's body point to world space.
///
/// The leading eight bytes are reserved but never accessed. The relative matrix
/// occupies the remaining 32 bytes and is borrowed only during the conversion.
typedef struct {
    byte   unused[8];   // Reserved but never accessed; role unproven
    MATRIX bodyToWorld; // Target-coordinate to world-coordinate transform
} _WorldTargetPositionScratch;
STATIC_ASSERT_SIZEOF(_WorldTargetPositionScratch, 0x28);

/// Scratch-stack block for projecting one target's body point to the screen.
///
/// The point is projected through the working matrix of the target's
/// coordinate node. The screen pixels are stored outside the block, in a word
/// the caller supplies; the block holds only the input point and the
/// projection's other results. Reserve the complete record on the scratch
/// stack; none of its members survive the matching release.
typedef struct {
    SVECTOR point;           // Target body point, local to its coordinate node
    s32     depthCue;        // GTE IR0 depth-cue coefficient, with 12 fractional bits
    s32     projectionFlags; // GTE FLAG bits; negative when the summary error bit is set
    s32     orderingDepth;   // Quarter camera-space depth from SZ3 (0..16383)
} _WorldTargetProjectionScratch;
STATIC_ASSERT_SIZEOF(_WorldTargetProjectionScratch, 0x14);

/// Projected screen position of a target bound to a readout.
///
/// The projection writes both pixels as one word. The readout's on-screen
/// number reads the two signed components when it places itself beside the
/// target. The origin is the screen center and Y increases downward. Storing
/// either member replaces the other.
typedef union {
    DVECTOR xy;     // Signed screen pixels from the center (vx right, vy down)
    s32     packed; // Both pixels in the one word the projection stores
} WorldTargetScreenPos;
STATIC_ASSERT_SIZEOF(WorldTargetScreenPos, 4);

/// Lifetime and display limits of one floating readout.
enum {
    WORLD_TARGET_READOUT_FRAMES        = 20,    // Passes the number stays up after the latest addition
    WORLD_TARGET_READOUT_DEPARTED      = 4,     // Binding word once the target has left the tracked list; not a pointer
    WORLD_TARGET_READOUT_DISPLAY_LIMIT = 10000, // Absolute total at which the drawn figure stops growing
    WORLD_TARGET_READOUT_DISPLAY_MAX   = 9999   // Figure drawn once the stored total reaches the limit
};

/// One floating damage or heal number drawn beside a tracked target.
///
/// `Gp_LockSlots` holds 32 of these. The binding is NULL when the slot is
/// empty, a `WorldTargetNode` while that target stays on the tracked list, or
/// `WORLD_TARGET_READOUT_DEPARTED` once the target has left the list. A
/// non-negative total and a negative total use separate slots for the same
/// target. Each addition rearms `framesLeft` to `WORLD_TARGET_READOUT_FRAMES`
/// passes. Each draw stores a new projection in `screen` while the target
/// remains listed, and leaves the last projection in place after the target
/// leaves. That draw decrements `framesLeft` and clears the slot at zero.
/// The drawn figure is the absolute value, limited to
/// `WORLD_TARGET_READOUT_DISPLAY_MAX`. The stored total is not limited.
/// Releasing a slot clears the binding, the total and the countdown.
typedef struct {
    union {
        WorldTargetNode* node;       // Bound target, or NULL when the slot is empty
        u32              word;       // Same storage; WORLD_TARGET_READOUT_DEPARTED after the target leaves the list
    } binding;
    s16                  amount;     // Signed total (non-negative damage, negative heal)
    s16                  framesLeft; // Passes left before the slot is cleared
    WorldTargetScreenPos screen;     // Last projected position; not cleared on release
} WorldTargetReadout;
STATIC_ASSERT_SIZEOF(WorldTargetReadout, 0xC);
STATIC_ASSERT(OFFSET_OF(WorldTargetReadout, amount) == 4, WorldTargetReadout_amount);
STATIC_ASSERT(OFFSET_OF(WorldTargetReadout, framesLeft) == 6, WorldTargetReadout_framesLeft);
STATIC_ASSERT(OFFSET_OF(WorldTargetReadout, screen) == 8, WorldTargetReadout_screen);

/* Define BSS before API headers to preserve first-declaration order. */
WorldTargetReadout Gp_LockSlots[32];

SceneCombatState gSceneCombatState;

#include "gameplay/scene_combat.h"
#include "gameplay/world_targets.h"

static void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag);

static void _worldTargetDrawReadouts(void);

static void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos);

/// Clears the player and companion actors' borrowed lock-on references to `node`.
///
/// Call before ending the target entry's lifetime. Each occupied actor task must
/// have a live `GameActor` work block. Only pointer identity is compared; the
/// caller manages the target entry's tracking state and storage.
static __inline__ void _worldTargetReleaseActorLocks(const WorldTargetNode* node)
{
    s32        actorSlot;
    Task**     taskSlot;
    Task*      actorTask;
    GameActor* actor;

    actorSlot = 0;
    taskSlot  = gPlayerActorTasks;
    do {
        actorTask = *taskSlot;
        if (actorTask != NULL) {
            actor = actorTask->work;
            if (actor->targetNode == node) {
                actor->targetNode = NULL;
            }
        }
        actorSlot++;
        taskSlot++;
    } while (actorSlot < PLAYER_ACTOR_TASK_COUNT);
}

/// Queues one raw texture or palette transfer without changing its upload record.
///
/// `scratchDestination` must hold a word-aligned writable `RECT`, separate from
/// `upload`, so SDK width/height clamping cannot change the source destination.
/// The upload must meet `GpuImageUpload`'s VRAM bounds and pixel-buffer contract.
/// The caller selects copy entries; this helper does not read the operation.
/// The scratch rectangle can be reused on return; pixel storage stays borrowed
/// until transfer completes. The SDK return value is ignored, and this helper
/// neither waits for transfer completion nor manages the scratch-stack cursor.
static __inline__ void _gpuUploadImageEntry(RECT* scratchDestination, const GpuImageUpload* upload)
{
    scratchDestination->x = upload->destination.x;
    scratchDestination->y = upload->destination.y;
    scratchDestination->w = upload->destination.w;
    scratchDestination->h = upload->destination.h;
    LoadImage(scratchDestination, upload->pixels);
}

/// Updates a live readout's packed screen position from its target's body point.
///
/// The binding must have been found on the tracked list and belong to a live
/// enemy with a coordinate whose working matrix is already composed. Body
/// coordinates narrow to signed 16 bits before projection. Screen pixels use
/// the screen center as origin, with Y downward; GTE errors are not filtered.
/// Requires an initialized scratch stack with room for the projection block.
/// Only the readout's screen word is changed; the scratch block is released
/// before returning, and the GTE transform registers are left changed.
static __inline__ void _worldTargetProjectReadout(WorldTargetReadout* readout)
{
    s32*                           packedScreen;
    const WorldTargetNode*         node;
    _WorldTargetProjectionScratch* projection;

    packedScreen = &readout->screen.packed;
    node         = readout->binding.node;
    SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetProjectionScratch);
    projection           = SCRATCH_STACK_CURSOR(_WorldTargetProjectionScratch);
    projection->point.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
    projection->point.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
    projection->point.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(node)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(node)->coord->workm);
    gte_ldv0(&projection->point);
    gte_rtps();
    gte_stsxy(packedScreen);
    gte_stdp(&projection->depthCue);
    gte_stflg(&projection->projectionFlags);
    gte_stszotz(&projection->orderingDepth);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetProjectionScratch);
}

void worldTargetDrawOverlay(void)
{
    enum {
        WORLD_TARGET_CURSOR_FRACTION_BITS          = 8,
        WORLD_TARGET_CURSOR_EASING_PASSES          = 5,
        WORLD_TARGET_CURSOR_EASING_COMPLETE        = 0xFF,
        WORLD_TARGET_CURSOR_HIDDEN_PAUSE_STATE     = 1,
        WORLD_TARGET_CURSOR_FRAME_COUNT            = 8,
        WORLD_TARGET_CURSOR_TICKS_PER_FRAME        = 3,
        WORLD_TARGET_CURSOR_SMALL_HALF_SIZE        = 8,
        WORLD_TARGET_CURSOR_FULL_HALF_SIZE         = 16,
        WORLD_TARGET_CURSOR_CELL_SHIFT             = 5, // 32-pixel cells
        WORLD_TARGET_CURSOR_COLUMN_SHIFT           = 2, // Four columns, two rows
        WORLD_TARGET_CURSOR_TEXTURE_BASE_U         = 64,
        WORLD_TARGET_CURSOR_TEXTURE_MODE_4BIT      = 0,
        WORLD_TARGET_CURSOR_TEXTURE_PAGE_X         = 896,
        WORLD_TARGET_CURSOR_TEXTURE_PAGE_Y         = 256,
        WORLD_TARGET_CURSOR_CLUT_X                 = 16,
        WORLD_TARGET_CURSOR_CLUT_Y                 = 242,
        WORLD_TARGET_CURSOR_RAW_SEMITRANS_FT4_CODE = 0x2F // Unmodulated texture; packet RGB bytes are ignored
    };
    WorldTargetNode*             targetNode;
    GameSession*                 session;
    WorldCoordProjectionScratch* projection;
    POLY_FT4*                    cursorQuad;
    s32                          cursorEasing;
    s32                          cursorFrame;
    s32                          textureU;
    s32                          textureV;

    /// Eases a changed target and marks this pass for the small reticle.
    ///
    /// The node and projection arguments must be side-effect-free live pointers;
    /// the projection holds screen pixels before shake compensation. `easingActive`
    /// is a distinct writable s32 local initialized to zero. Arguments repeat;
    /// this macro captures and updates D_80115260, D_80115264, D_8010F9EC and
    /// D_8010F9F0, using the WORLD_TARGET_CURSOR_ easing constants above.
    /// Initial acquisition snaps. Changing targets halves displacement for at
    /// most five passes; convergence still draws small on its final easing pass.
    /// Expands multiple statements; use only standalone in a braced block.
#define WORLD_TARGET_EASE_CURSOR(targetNode, projection, easingActive)                                    \
    if (D_80115260 != (targetNode)) {                                                                     \
        if (D_80115260 == NULL) {                                                                         \
            D_80115264 = WORLD_TARGET_CURSOR_EASING_COMPLETE;                                             \
        } else {                                                                                          \
            D_80115264 = 0;                                                                               \
        }                                                                                                 \
        D_80115260 = (targetNode);                                                                        \
    }                                                                                                     \
    if (D_80115264 < WORLD_TARGET_CURSOR_EASING_PASSES) {                                                 \
        D_8010F9EC += (((projection)->screen.vx << WORLD_TARGET_CURSOR_FRACTION_BITS) - D_8010F9EC) >> 1; \
        D_8010F9F0 += (((projection)->screen.vy << WORLD_TARGET_CURSOR_FRACTION_BITS) - D_8010F9F0) >> 1; \
        if ((projection)->screen.vx == (D_8010F9EC >> WORLD_TARGET_CURSOR_FRACTION_BITS) &&               \
            (projection)->screen.vy == (D_8010F9F0 >> WORLD_TARGET_CURSOR_FRACTION_BITS)) {               \
            D_80115264 = WORLD_TARGET_CURSOR_EASING_COMPLETE;                                             \
        } else {                                                                                          \
            D_80115264++;                                                                                 \
        }                                                                                                 \
        (easingActive)          = 1;                                                                      \
        (projection)->screen.vx = D_8010F9EC >> WORLD_TARGET_CURSOR_FRACTION_BITS;                        \
        (projection)->screen.vy = D_8010F9F0 >> WORLD_TARGET_CURSOR_FRACTION_BITS;                        \
    } else {                                                                                              \
        D_8010F9EC = (projection)->screen.vx << WORLD_TARGET_CURSOR_FRACTION_BITS;                        \
        D_8010F9F0 = (projection)->screen.vy << WORLD_TARGET_CURSOR_FRACTION_BITS;                        \
    }

    targetNode = gWorldTargetListHead;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }
    // Readouts keep aging under the cursor-only gates below.
    _worldTargetDrawReadouts();
    session = gGameSession;
    if (session->sceneUpdatesPaused == WORLD_TARGET_CURSOR_HIDDEN_PAUSE_STATE) {
        return;
    }
    if (Gp_StateC08.mode == ATTACHMENT_MODE_ARMED || Gp_StateC08.mode == ATTACHMENT_MODE_CAST) {
        return;
    }
    if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return;
    }
    if (session->eventState != 0) {
        return;
    }
    if (session->hideHud != 0) {
        return;
    }
    for (; targetNode != NULL; targetNode = targetNode->next) {
        if (targetNode->state.parts.targeted != 0 && !(targetNode->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            // Project the first marked, lockable enemy's local body point.
            cursorEasing         = 0;
            projection           = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
            projection->point.vx = GP_NODE_ENEMY(targetNode)->bodyPos.vx;
            projection->point.vy = GP_NODE_ENEMY(targetNode)->bodyPos.vy;
            projection->point.vz = GP_NODE_ENEMY(targetNode)->bodyPos.vz;
            actorRenderComposeCoord(GP_NODE_ENEMY(targetNode)->coord);
            gte_SetRotMatrix(&GP_NODE_ENEMY(targetNode)->coord->workm);
            gte_SetTransMatrix(&GP_NODE_ENEMY(targetNode)->coord->workm);
            gte_RotTransPers(&projection->point, &projection->screen, &projection->depthCue,
                             &projection->projectionFlags, &projection->orderingDepth);
            WORLD_TARGET_EASE_CURSOR(targetNode, projection, cursorEasing);
#undef WORLD_TARGET_EASE_CURSOR
            // Remove screen shake after easing; draw the eight-cell cursor animation.
            projection->screen.vy -= gDisplayState.vramYOffset;
            cursorFrame            = gDisplayState.animFrame % (WORLD_TARGET_CURSOR_FRAME_COUNT * WORLD_TARGET_CURSOR_TICKS_PER_FRAME) /
                          WORLD_TARGET_CURSOR_TICKS_PER_FRAME;
            cursorQuad     = gGpuPrimCursor;
            gGpuPrimCursor = cursorQuad + 1;
            if (cursorEasing == 1) {
                cursorQuad->x0 = cursorQuad->x2 = projection->screen.vx - WORLD_TARGET_CURSOR_SMALL_HALF_SIZE;
                cursorQuad->x1 = cursorQuad->x3 = projection->screen.vx + WORLD_TARGET_CURSOR_SMALL_HALF_SIZE;
                cursorQuad->y0 = cursorQuad->y1 = projection->screen.vy - WORLD_TARGET_CURSOR_SMALL_HALF_SIZE;
                cursorQuad->y2 = cursorQuad->y3 = projection->screen.vy + WORLD_TARGET_CURSOR_SMALL_HALF_SIZE;
            } else {
                cursorQuad->x0 = cursorQuad->x2 = projection->screen.vx - WORLD_TARGET_CURSOR_FULL_HALF_SIZE;
                cursorQuad->x1 = cursorQuad->x3 = projection->screen.vx + WORLD_TARGET_CURSOR_FULL_HALF_SIZE;
                cursorQuad->y0 = cursorQuad->y1 = projection->screen.vy - WORLD_TARGET_CURSOR_FULL_HALF_SIZE;
                cursorQuad->y2 = cursorQuad->y3 = projection->screen.vy + WORLD_TARGET_CURSOR_FULL_HALF_SIZE;
            }
            textureU = (cursorFrame & ((1 << WORLD_TARGET_CURSOR_COLUMN_SHIFT) - 1)) << WORLD_TARGET_CURSOR_CELL_SHIFT;
            textureV = (cursorFrame >> WORLD_TARGET_CURSOR_COLUMN_SHIFT) << WORLD_TARGET_CURSOR_CELL_SHIFT;
            setUV4(cursorQuad, textureU + WORLD_TARGET_CURSOR_TEXTURE_BASE_U, textureV,
                   textureU + WORLD_TARGET_CURSOR_TEXTURE_BASE_U + (1 << WORLD_TARGET_CURSOR_CELL_SHIFT), textureV,
                   textureU + WORLD_TARGET_CURSOR_TEXTURE_BASE_U, textureV + (1 << WORLD_TARGET_CURSOR_CELL_SHIFT),
                   textureU + WORLD_TARGET_CURSOR_TEXTURE_BASE_U + (1 << WORLD_TARGET_CURSOR_CELL_SHIFT), textureV + (1 << WORLD_TARGET_CURSOR_CELL_SHIFT));
            cursorQuad->clut  = getClut(WORLD_TARGET_CURSOR_CLUT_X, WORLD_TARGET_CURSOR_CLUT_Y);
            cursorQuad->tpage = getTPage(WORLD_TARGET_CURSOR_TEXTURE_MODE_4BIT, GPU_BLEND_ADD,
                                         WORLD_TARGET_CURSOR_TEXTURE_PAGE_X, WORLD_TARGET_CURSOR_TEXTURE_PAGE_Y);
            setlen(cursorQuad, (sizeof(*cursorQuad) - sizeof(cursorQuad->tag)) / sizeof(u_long));
            setcode(cursorQuad, WORLD_TARGET_CURSOR_RAW_SEMITRANS_FT4_CODE);
            addPrim(gGpuCurrentOt, cursorQuad);
            SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
            break;
        }
    }
    if (targetNode == NULL) {
        D_80115260 = NULL;
        D_80115264 = 0;
    }
}

static void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag)
{
    _WorldTargetLockScanScratch* block;
    GameActor*                   actor;
    GfxCoord*                    coord;
    GfxCoord*                    nodeCoord;
    WorldTargetNode*             node;
    WorldTargetNode*             best;
    s32                          bestAngle;
    u32                          bestDist;
    s32                          baseAngle;
    s32                          angle;
    u32                          dist;
    s32                          sub;
    SVECTOR                      tmp;
    SVECTOR*                     srcp;

    best = NULL;
    SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetLockScanScratch);
    block              = SCRATCH_STACK_CURSOR(_WorldTargetLockScanScratch);
    actor              = arg0->work;
    coord              = arg0->extra.tmd->coords;
    block->eyeWorld.vx = coord->coord.t[0];
    block->eyeWorld.vy = coord->coord.t[1] - 1000;
    block->eyeWorld.vz = coord->coord.t[2];
    actorRenderComposeCoord(&gGfxViewCoord);
    srcp = &block->eyeWorld;
    gte_SetRotMatrix(&gGfxViewCoord.workm);
    gte_ldv0(srcp);
    gte_rtv0();
    gte_stsv(&block->eyeView);
    block->eyeView.vx += gGfxViewCoord.workm.t[0];
    block->eyeView.vy += gGfxViewCoord.workm.t[1];
    block->eyeView.vz += gGfxViewCoord.workm.t[2];

    if (actor->targetNode != NULL && flag != 0) {
        node      = actor->targetNode;
        baseAngle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
    } else {
        baseAngle = 0;
    }
    bestAngle = 0x3000;
    bestDist  = 0x7FFFFFFF;
    dist      = 0;
    for (node = gWorldTargetListHead; node != NULL; node = node->next) {
        if (node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE) {
            continue;
        }
        angle = ratan2(GP_NODE_ENEMY(node)->playerRelPos.vx, GP_NODE_ENEMY(node)->playerRelPos.vz);
        if (flag == 0) {
            dist = GP_NODE_ENEMY(node)->playerRelPos.vz * GP_NODE_ENEMY(node)->playerRelPos.vz + GP_NODE_ENEMY(node)->playerRelPos.vx * GP_NODE_ENEMY(node)->playerRelPos.vx + GP_NODE_ENEMY(node)->playerRelPos.vy * GP_NODE_ENEMY(node)->playerRelPos.vy;
            if (angle < 0) {
                angle = -angle;
            }
            if (dist <= 0x300000) {
                sub    = 0x300000 - dist;
                sub  >>= 13;
                sub   *= 3;
                angle -= sub;
                if (angle < 0) {
                    angle = 0;
                }
                dist += sub / 3;
            }
            angle >>= 10;
            if (node == actor->targetNode) {
                angle += 0x1000;
            }
            if (angle == bestAngle && dist > bestDist) {
                angle = 0x2000;
            }
        } else {
            angle -= baseAngle;
            if (angle < 0) {
                angle += 0x1000;
            }
            if (angle >= 0x1000) {
                angle -= 0x1000;
            }
            if (flag == 1) {
                angle = -angle;
            }
            if (node == actor->targetNode) {
                angle += 0x1000;
            }
        }
        if (angle > bestAngle) {
            continue;
        }
        actorRenderComposeCoord(GP_NODE_ENEMY(node)->coord);
        block->targetView.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        block->targetView.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        block->targetView.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        nodeCoord            = GP_NODE_ENEMY(node)->coord;
        tmp                  = block->targetView;
        gte_SetRotMatrix(&nodeCoord->workm);
        gte_ldv0(&tmp);
        gte_rtv0();
        gte_stsv(&block->targetView);
        block->targetView.vx += GP_NODE_ENEMY(node)->coord->workm.t[0];
        block->targetView.vy += GP_NODE_ENEMY(node)->coord->workm.t[1];
        block->targetView.vz += GP_NODE_ENEMY(node)->coord->workm.t[2];
        if (func_800E0308(&block->targetView, &block->eyeView) != 1) {
            bestAngle = angle;
            best      = node;
            bestDist  = dist;
        }
    }
    if (best != NULL) {
        worldTargetGetBodyPosition(best, out);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetLockScanScratch);
    return best;
}

void worldTargetAddReadoutAmount(WorldTargetNode* node, s32 amount, s32 unusedArg)
{
    WorldTargetReadout* found;
    s32                 readoutIndex;
    WorldTargetReadout* readout;

    // Finds the existing sign-class total, writing found and advancing the scan locals.
    // Reads Gp_LockSlots and captures node, amount, found, readoutIndex and readout.
    // Inputs may be read repeatedly; each break exits only the search loop.
#define WORLD_TARGET_FIND_READOUT()                                       \
    {                                                                     \
        found        = NULL;                                              \
        readoutIndex = 0;                                                 \
        readout      = Gp_LockSlots;                                      \
        for (; readoutIndex < ARRAY_SIZE(Gp_LockSlots); readoutIndex++) { \
            if (readout->binding.node == node) {                          \
                if (amount >= 0) {                                        \
                    if (readout->amount >= 0) {                           \
                        found = readout;                                  \
                        break;                                            \
                    }                                                     \
                    readout++;                                            \
                } else if (readout->amount < 0) {                         \
                    found = readout;                                      \
                    break;                                                \
                } else {                                                  \
                    readout++;                                            \
                }                                                         \
            } else {                                                      \
                readout++;                                                \
            }                                                             \
        }                                                                 \
    }

    // Keep damage and healing in separate totals for the same target.
    WORLD_TARGET_FIND_READOUT();
#undef WORLD_TARGET_FIND_READOUT
    if (found == NULL) {
        for (readoutIndex = 0, readout = Gp_LockSlots; readoutIndex < ARRAY_SIZE(Gp_LockSlots); readoutIndex++, readout++) {
            if (readout->binding.node == NULL) {
                found                 = readout;
                readout->binding.node = node;
                found->amount         = 0;
                break;
            }
        }
    }
    if (found != NULL) {
        found->framesLeft = WORLD_TARGET_READOUT_FRAMES;
        found->amount    += amount;
    }
}

/// Draws and ages every floating damage/heal readout once.
///
/// The tracked list must be acyclic and its nodes must belong to live enemies
/// with composed coordinate working matrices. A departed binding is compared
/// only by its word and keeps its last projection. Occupied slots draw before
/// their signed 16-bit countdown is decremented; expiry clears the binding,
/// amount and countdown. Empty slots have their amount/countdown cleared too.
/// Requires the scratch stack, small-font and UI-frame resources, writable OT
/// entry -10 and sufficient primitive-arena space. Packets remain borrowed by
/// the GPU until the frame is consumed. The caller controls HUD suppression;
/// this pass counts calls rather than elapsed or unpaused gameplay frames.
static void _worldTargetDrawReadouts(void)
{
    enum {
        WORLD_TARGET_READOUT_DAMAGE_OFFSET_X      = 10,
        WORLD_TARGET_READOUT_DAMAGE_OFFSET_Y      = 4,
        WORLD_TARGET_READOUT_HEAL_OFFSET_X        = -10,
        WORLD_TARGET_READOUT_HEAL_OFFSET_Y        = -16,
        WORLD_TARGET_READOUT_LEFT_LIMIT_X         = -136,
        WORLD_TARGET_READOUT_RIGHT_LIMIT_X        = 137,
        WORLD_TARGET_READOUT_BOTTOM_LIMIT_Y       = 85,
        WORLD_TARGET_READOUT_TOP_LIMIT_Y          = -100,
        WORLD_TARGET_READOUT_LEFT_BAND_X          = -143,
        WORLD_TARGET_READOUT_RIGHT_BAND_X         = 143,
        WORLD_TARGET_READOUT_BOTTOM_BAND_Y        = 77,
        WORLD_TARGET_READOUT_TOP_BAND_Y           = -93,
        WORLD_TARGET_READOUT_EDGE_PHASE_MASK      = 7,
        WORLD_TARGET_READOUT_OT_INDEX             = -10,
        WORLD_TARGET_READOUT_DAMAGE_RGB           = GPU_PACK_COLOR_WORD(120, 122, 3, 0),
        WORLD_TARGET_READOUT_HEAL_RGB             = GPU_PACK_COLOR_WORD(8, 128, 128, 0),
        WORLD_TARGET_READOUT_TEXT_RIGHT_OFFSET_X  = 14,
        WORLD_TARGET_READOUT_FRAME_TOP_INSET      = 8,
        WORLD_TARGET_READOUT_FRAME_HEIGHT         = 12,
        WORLD_TARGET_READOUT_FRAME_LEFT_3_DIGITS  = 16,
        WORLD_TARGET_READOUT_FRAME_WIDTH_3_DIGITS = 32,
        WORLD_TARGET_READOUT_FRAME_LEFT_4_DIGITS  = 24,
        WORLD_TARGET_READOUT_FRAME_WIDTH_4_DIGITS = 40,
        WORLD_TARGET_READOUT_FRAME_LEFT_2_DIGITS  = 8,
        WORLD_TARGET_READOUT_FRAME_WIDTH_2_DIGITS = 24,
        WORLD_TARGET_READOUT_4_DIGIT_MINIMUM      = 1000,
        WORLD_TARGET_READOUT_3_DIGIT_MINIMUM      = 100
    };
    RECT                frameRect;
    u8                  numberBuffer[16];
    TextDrawReq         request;
    s32                 readoutIndex;
    WorldTargetReadout* readout;
    u8*                 numberText;
    TextDrawReq*        numberRequest;
    s32                 frameX;
    s32                 baselineY;
    s32                 displayAmount;
    s32                 rightEdgeX;
    s32                 otIndex;
    WorldTargetNode*    bindingNode;
    WorldTargetNode*    listedNode;
    s32                 targetListed;

    /// Places one readout beside its target, folding off-screen anchors into edge bands.
    ///
    /// The caller supplies the WORLD_TARGET_READOUT_* placement constants above.
    /// `readout` is a side-effect-free pointer to a readable readout; `signedTotal`
    /// receives its amount. The other outputs are frame X and text baseline Y in
    /// screen-centered pixels. All writable arguments must be distinct s32 local
    /// lvalues without side effects; arguments are repeated. Right/top bands
    /// reverse the low-three-bit variation. Expands several statements: use only
    /// as a standalone statement in a braced block. No readout changes.
#define WORLD_TARGET_PLACE_READOUT(readout, signedTotal, frameX, baselineY)                                      \
    (signedTotal) = (readout)->amount;                                                                           \
    if ((signedTotal) >= 0) {                                                                                    \
        (frameX)    = (readout)->screen.xy.vx + WORLD_TARGET_READOUT_DAMAGE_OFFSET_X;                            \
        (baselineY) = (readout)->screen.xy.vy + WORLD_TARGET_READOUT_DAMAGE_OFFSET_Y;                            \
    } else {                                                                                                     \
        (frameX)    = (readout)->screen.xy.vx + WORLD_TARGET_READOUT_HEAL_OFFSET_X;                              \
        (baselineY) = (readout)->screen.xy.vy + WORLD_TARGET_READOUT_HEAL_OFFSET_Y;                              \
    }                                                                                                            \
    if ((frameX) < WORLD_TARGET_READOUT_LEFT_LIMIT_X) {                                                          \
        (frameX) = ((frameX) & WORLD_TARGET_READOUT_EDGE_PHASE_MASK) + WORLD_TARGET_READOUT_LEFT_BAND_X;         \
    }                                                                                                            \
    if ((frameX) >= WORLD_TARGET_READOUT_RIGHT_LIMIT_X) {                                                        \
        (frameX) = -((frameX) & WORLD_TARGET_READOUT_EDGE_PHASE_MASK) + WORLD_TARGET_READOUT_RIGHT_BAND_X;       \
    }                                                                                                            \
    if ((baselineY) >= WORLD_TARGET_READOUT_BOTTOM_LIMIT_Y) {                                                    \
        (baselineY) = ((baselineY) & WORLD_TARGET_READOUT_EDGE_PHASE_MASK) + WORLD_TARGET_READOUT_BOTTOM_BAND_Y; \
    }                                                                                                            \
    if ((baselineY) < WORLD_TARGET_READOUT_TOP_LIMIT_Y) {                                                        \
        (baselineY) = -((baselineY) & WORLD_TARGET_READOUT_EDGE_PHASE_MASK) + WORLD_TARGET_READOUT_TOP_BAND_Y;   \
    }

    readout       = Gp_LockSlots;
    readoutIndex  = 0;
    numberText    = numberBuffer;
    numberRequest = &request;
    otIndex       = WORLD_TARGET_READOUT_OT_INDEX;
    do {
        bindingNode = readout->binding.node;
        if (bindingNode != NULL) {
            // Validate the binding against live entries before interpreting it as a node.
            targetListed = 0;
            for (listedNode = gWorldTargetListHead; listedNode != NULL; listedNode = listedNode->next) {
                if (bindingNode == listedNode) {
                    targetListed = 1;
                    break;
                }
            }
            if (targetListed != 0) {
                _worldTargetProjectReadout(readout);
            } else {
                // Keep the last projection after the target leaves, until the countdown ends.
                readout->binding.word = WORLD_TARGET_READOUT_DEPARTED;
            }

            WORLD_TARGET_PLACE_READOUT(readout, displayAmount, frameX, baselineY);
#undef WORLD_TARGET_PLACE_READOUT

            rightEdgeX         = frameX + WORLD_TARGET_READOUT_TEXT_RIGHT_OFFSET_X;
            request.x          = rightEdgeX;
            request.y          = baselineY;
            request.otIndex    = otIndex;
            request.colorRgb   = WORLD_TARGET_READOUT_DAMAGE_RGB;
            request.glyphTable = TEXT_GLYPH_TABLE_SMALL;
            request.alignment  = TEXT_ALIGNMENT_RIGHT;
            request.drawMode   = TEXT_DRAW_OUTLINED;

            displayAmount = readout->amount;
            if (displayAmount < 0) {
                request.colorRgb = WORLD_TARGET_READOUT_HEAL_RGB;
                displayAmount    = -displayAmount;
            }
            if (displayAmount >= WORLD_TARGET_READOUT_DISPLAY_LIMIT) {
                displayAmount = WORLD_TARGET_READOUT_DISPLAY_MAX;
            }

            // The largest displayed value has four digits plus NUL, within numberBuffer.
            request.x        = rightEdgeX;
            request.drawMode = TEXT_DRAW_FILL_ONLY;
            textDrawString(numberRequest, textItoaSigned(numberText, displayAmount));
            // Restore the right anchor; prepend the outline so it executes before the fill.
            request.x        = rightEdgeX;
            request.drawMode = TEXT_DRAW_OUTLINE_ONLY;
            textDrawString(numberRequest, textItoaSigned(numberText, displayAmount));

            frameRect.x = frameX - WORLD_TARGET_READOUT_FRAME_LEFT_3_DIGITS;
            frameRect.y = baselineY - WORLD_TARGET_READOUT_FRAME_TOP_INSET;
            frameRect.w = WORLD_TARGET_READOUT_FRAME_WIDTH_3_DIGITS;
            frameRect.h = WORLD_TARGET_READOUT_FRAME_HEIGHT;
            if (displayAmount >= WORLD_TARGET_READOUT_4_DIGIT_MINIMUM) {
                frameRect.x = frameX - WORLD_TARGET_READOUT_FRAME_LEFT_4_DIGITS;
                frameRect.w = WORLD_TARGET_READOUT_FRAME_WIDTH_4_DIGITS;
            } else if (displayAmount < WORLD_TARGET_READOUT_3_DIGIT_MINIMUM) {
                frameRect.x = frameX - WORLD_TARGET_READOUT_FRAME_LEFT_2_DIGITS;
                frameRect.w = WORLD_TARGET_READOUT_FRAME_WIDTH_2_DIGITS;
            }
            uiDrawRectFrame(&frameRect, WORLD_TARGET_READOUT_OT_INDEX, USER_INTERFACE_PANEL_TITLE_STYLE, NULL);

            {
                s16 framesLeft;
                framesLeft = readout->framesLeft;
                framesLeft--;
                readout->framesLeft = framesLeft;
                if (framesLeft <= 0) {
                    readout->amount       = 0;
                    readout->framesLeft   = 0;
                    readout->binding.node = NULL;
                }
            }
        } else {
            readout->amount     = 0;
            readout->framesLeft = 0;
        }
        readoutIndex++;
        readout++;
    } while (readoutIndex < ARRAY_SIZE(Gp_LockSlots));
}

void worldTargetUnlinkNode(WorldTargetNode* node)
{
    enum {
        WORLD_TARGET_OFF_LIST     = 0,
        WORLD_TARGET_ON_LIST      = 1,
        WORLD_TARGET_NOT_TARGETED = 0
    };
    WorldTargetNode** incomingLink;

    _worldTargetReleaseActorLocks(node);

    // The incoming link is either the list head or a predecessor's successor.
    if (node->state.parts.onList == WORLD_TARGET_ON_LIST) {
        incomingLink = &gWorldTargetListHead;
        while (*incomingLink != node) {
            if (*incomingLink == NULL) {
                break;
            }
            incomingLink = &(*incomingLink)->next;
        }
        if (*incomingLink != NULL) {
            *incomingLink = node->next;
        }
        node->state.parts.onList   = WORLD_TARGET_OFF_LIST;
        node->state.parts.targeted = WORLD_TARGET_NOT_TARGETED;
    }
}

void worldTargetLinkNode(WorldTargetNode* node)
{
    enum {
        WORLD_TARGET_OFF_LIST     = 0,
        WORLD_TARGET_ON_LIST      = 1,
        WORLD_TARGET_NOT_TARGETED = 0
    };
    WorldTargetNode** incomingLink;

    if (node->state.parts.onList == WORLD_TARGET_OFF_LIST) {
        incomingLink = &gWorldTargetListHead;
        while (*incomingLink != NULL) {
            incomingLink = &(*incomingLink)->next;
        }
        *incomingLink              = node;
        node->next                 = NULL;
        node->state.parts.targeted = WORLD_TARGET_NOT_TARGETED;
        node->state.parts.onList   = WORLD_TARGET_ON_LIST;
        node->state.parts.flags   &= WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    } else {
        node->state.parts.flags &= WORLD_TARGET_NOT_LOCKABLE_CLEAR;
    }
}

s32 worldTargetGetActorLockMask(const WorldTargetNode* node)
{
    s32              mask;
    s32              actorSlot;
    s32              slotBit;
    Task**           taskSlot;
    Task*            actorTask;
    const GameActor* actor;

    mask      = 0;
    actorSlot = 0;
    slotBit   = 1;
    taskSlot  = gPlayerActorTasks;
    do {
        actorTask = *taskSlot;
        if (actorTask != NULL) {
            actor = actorTask->work;
            if (actor->targetNode == node) {
                mask |= slotBit << actorSlot;
            }
        }
        actorSlot++;
        taskSlot++;
    } while (actorSlot < PLAYER_ACTOR_TASK_COUNT);
    return mask;
}

void worldTargetSetPlayerLock(WorldTargetNode* node)
{
    enum { WORLD_TARGET_NOT_TARGETED = 0,
           WORLD_TARGET_TARGETED     = 1 };
    Task*            playerTask;
    GameActor*       actor;
    WorldTargetNode* previous;
    u8               flags;

    playerTask = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (playerTask != NULL) {
        actor    = playerTask->work;
        previous = actor->targetNode;
        if (previous != NULL) {
            previous->state.parts.targeted = WORLD_TARGET_NOT_TARGETED;
        }
        actor->targetNode = node;
    }
    flags                      = node->state.parts.flags;
    node->state.parts.targeted = WORLD_TARGET_TARGETED;
    node->state.parts.flags    = flags & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
}

void worldTargetDisableNodeLockOn(WorldTargetNode* node)
{
    enum { WORLD_TARGET_NOT_TARGETED = 0 };
    u8 flags;

    _worldTargetReleaseActorLocks(node);
    flags                      = node->state.parts.flags;
    node->state.parts.targeted = WORLD_TARGET_NOT_TARGETED;
    node->state.parts.flags    = flags | WORLD_TARGET_NOT_LOCKABLE;
}

void* Gp_FindLockNode(Task* arg0)
{
    VECTOR3 pos;

    return Gp_ScanLockNodes(arg0, &pos, 0);
}

void* Gp_FindLockNodePad(Task* arg0)
{
    VECTOR3  pos;
    VECTOR3* p;
    s32      flag;

    p = &pos;
    if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_LEFT) != 0) {
        flag = 1;
    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, p, flag);
}

static void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos)
{
    s32 flag;

    if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_LEFT) != 0) {
        flag = 1;
    } else if (padCheckButtons(0, PAD_BUTTON_QUERY_HELD_ANY, PAD_BUTTON_RIGHT) != 0) {
        flag = -1;
    } else {
        flag = 0;
    }
    return Gp_ScanLockNodes(arg0, pos, flag);
}

void worldTargetGetBodyPosition(const WorldTargetNode* node, VECTOR3* outPosition)
{
    GfxCoord*                    worldCoord;
    GfxCoord*                    bodyCoord;
    _WorldTargetPositionScratch* scratchTop;
    MATRIX*                      bodyToWorld;

    if (node == NULL) {
        printf(Gp_StrGetLockPosNull);
        outPosition->vx = 0;
        outPosition->vy = 0;
        outPosition->vz = 0;
        return;
    }

    bodyCoord  = GP_NODE_ENEMY(node)->coord;
    worldCoord = &gGfxViewCoord;
    if (bodyCoord == worldCoord) {
        outPosition->vx = GP_NODE_ENEMY(node)->bodyPos.vx;
        outPosition->vy = GP_NODE_ENEMY(node)->bodyPos.vy;
        outPosition->vz = GP_NODE_ENEMY(node)->bodyPos.vz;
        return;
    }

    // Remove the view transform before applying the enemy's local body point.
    scratchTop                                        = SCRATCH_STACK_CURSOR(_WorldTargetPositionScratch);
    SCRATCH_STACK_CURSOR(_WorldTargetPositionScratch) = scratchTop - 1;
    actorRenderComposeCoord(bodyCoord);
    bodyToWorld = &(scratchTop - 1)->bodyToWorld;
    gfxMakeRelativeTransform(&worldCoord->workm, &bodyCoord->workm, bodyToWorld);
    gte_SetRotMatrix(bodyToWorld);
    gte_SetTransMatrix(bodyToWorld);
    gte_ldlvl(&GP_NODE_ENEMY(node)->bodyPos);
    gte_rtirtr();
    gte_stlvl(outPosition);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetPositionScratch);
}

/// Empties every floating readout, retaining each slot's last screen position.
static void _worldTargetClearReadouts(void)
{
    s32                 readoutIndex;
    WorldTargetReadout* readout;

    readout      = Gp_LockSlots;
    readoutIndex = 0;
    do {
        readoutIndex++;
        readout->binding.node = NULL;
        readout->amount       = 0;
        readout->framesLeft   = 0;
        readout++;
    } while (readoutIndex < ARRAY_SIZE(Gp_LockSlots));
}

void worldTargetResetAreaTracking(void)
{
    enum { WORLD_TARGET_CURSOR_RESET_FIXED8 = 0xFFF00000 }; // -4096 pixels, eight fractional bits

    gWorldTargetListHead = NULL;
    _worldTargetClearReadouts();
    D_8010F9F0 = WORLD_TARGET_CURSOR_RESET_FIXED8;
    D_8010F9EC = WORLD_TARGET_CURSOR_RESET_FIXED8;
}

/// Projects an enemy target's body point and returns its quarter-depth for ordering.
///
/// Retained standalone projection with no current caller. `node` must be a live
/// enemy's embedded target entry; its coordinate's working matrix must already
/// be composed. Body coordinates narrow to signed 16 bits. `packedScreen` must
/// provide one writable, word-aligned s32 for the two signed screen pixels,
/// centered on the screen with Y downward. GTE error flags are not filtered;
/// the return is SZ3 >> 2 (0..16383). Requires an initialized scratch stack
/// with space for the projection block, released before returning. No pointer
/// is retained; the GTE transform registers are left changed.
static s32 _worldTargetProjectBodyPoint(const WorldTargetNode* node, s32* packedScreen)
{
    _WorldTargetProjectionScratch* projection;
    s32                            orderingDepth;

    projection           = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetProjectionScratch);
    projection->point.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
    projection->point.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
    projection->point.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(node)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(node)->coord->workm);
    gte_ldv0(&projection->point);
    gte_rtps();
    gte_stsxy(packedScreen);
    gte_stdp(&projection->depthCue);
    gte_stflg(&projection->projectionFlags);
    gte_stszotz(&projection->orderingDepth);
    orderingDepth = projection->orderingDepth;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetProjectionScratch);
    return orderingDepth;
}

void worldTargetClearActorTargetMarks(void)
{
    enum { WORLD_TARGET_NOT_TARGETED = 0 };
    s32              actorSlot;
    Task**           taskSlot;
    Task*            actorTask;
    const GameActor* actor;
    WorldTargetNode* node;

    actorSlot = 0;
    taskSlot  = gPlayerActorTasks;
    do {
        actorTask = *taskSlot;
        if (actorTask != NULL) {
            actor = actorTask->work;
            node  = actor->targetNode;
            if (node != NULL) {
                node->state.parts.targeted = WORLD_TARGET_NOT_TARGETED;
            }
        }
        actorSlot++;
        taskSlot++;
    } while (actorSlot < PLAYER_ACTOR_TASK_COUNT);
}

s32 Gp_GrantLocationItems(InventoryItemRange* arg0)
{
    GameLocationKey*       loc;
    InventoryBattleReward* rec;
    s32                    key;
    s32                    ret;
    s32                    i;
    u16                    item;
    s8                     mode;
    u8                     stage;
    u8                     area;
    u8                     sub;

    ret   = 0;
    loc   = &gGameSession->location.loc;
    stage = loc->stage;
    area  = loc->area;
    sub   = loc->variant;
    key   = GAME_LOCATION_KEY(stage, area, sub, 0);
    mode  = gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode;
    if ((mode == 0) || (mode == 2)) {
        rec = D_8010F9F4[stage];
    } else {
        rec = D_8010FA0C[stage];
    }
    if (rec->areaLayoutKey != INVENTORY_BATTLE_REWARD_LIST_END) {
        do {
            if (rec->areaLayoutKey == key) {
                for (i = 0; i < ARRAY_SIZE(rec->items); i++) {
                    item = rec->items[i];
                    if (item != 0) {
                        if ((i != INVENTORY_BATTLE_REWARD_BONUS_SLOT) || (func_800B9D80(0x80000) != 0)) {
                            if (func_800B7420(item) == 0) {
                                ret = 1;
                                if (i == INVENTORY_BATTLE_REWARD_BONUS_SLOT) {
                                    ret = 2;
                                }
                                Gp_GiveItem(arg0, item, -1);
                            }
                        }
                    }
                }
                return ret;
            }
            rec++;
        } while (rec->areaLayoutKey != INVENTORY_BATTLE_REWARD_LIST_END);
    }
    return ret;
}

s32 actorRenderUploadTexture(Task* actorTask, GpuImageUpload* uploadList, const RECT* textureRect)
{
    enum {
        ACTOR_RENDER_TEXTURE_PAGE_WORD_SHIFT = 6,
        ACTOR_RENDER_TEXTURE_BASE_X_WORDS    = 0x180,
        ACTOR_RENDER_TEXTURE_BASE_Y_ROWS     = 0x100,
        ACTOR_RENDER_TEXTURE_LIST_PRESENT    = 0,
        ACTOR_RENDER_TEXTURE_LIST_ABSENT     = 1
    };
    s32        listAbsent;
    TmdObject* model;
    s32        baseXWords;

    model      = actorTask->extra.tmd;
    listAbsent = ACTOR_RENDER_TEXTURE_LIST_PRESENT;
    if (uploadList != NULL) {
        // Translate the mixed-unit model rectangle into a VRAM destination.
        uploadList->destination.x = (model->texturePageOffset << ACTOR_RENDER_TEXTURE_PAGE_WORD_SHIFT) +
                                    (baseXWords = (textureRect->x + 1) / 2 + ACTOR_RENDER_TEXTURE_BASE_X_WORDS);
        uploadList->destination.y = textureRect->y + ACTOR_RENDER_TEXTURE_BASE_Y_ROWS;
        uploadList->destination.w = textureRect->w;
        uploadList->destination.h = textureRect->h;
        gpuUploadImages(uploadList);
    } else {
        listAbsent = ACTOR_RENDER_TEXTURE_LIST_ABSENT;
    }
    return listAbsent;
}

void gpuUploadImages(const GpuImageUpload* uploadList)
{
    RECT* scratchDestination;
    s32   reachedEnd;

    reachedEnd         = 0;
    scratchDestination = SCRATCH_STACK_RESERVE_BLOCK(RECT);

    // The SDK copies each rectangle into its queue but borrows the pixel data.
    do {
        switch (uploadList->operation) {
            case GPU_IMAGE_UPLOAD_COPY:
                _gpuUploadImageEntry(scratchDestination, uploadList);
                break;
            case GP_IMG_REC_END:
                reachedEnd = 1;
                break;
            default:
                reachedEnd = 1;
                break;
        }
        uploadList++;
    } while (reachedEnd == 0);

    SCRATCH_STACK_RELEASE_BLOCK(RECT);
}

void sceneResetCombatState(void)
{
    SceneCombatState* combat;
    McSaveData*       save;
    u8                difficulty;

    // Clear encounter accounting and group coordination before the new tasks run.
    combat                                      = &gSceneCombatState;
    gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
    combat->signals.bytes.endDelayFrames        = 0;
    combat->signals.bytes.actionFlags           = 0;
    combat->signals.bytes.enemyAlert            = 0;
    combat->actorControl                        = SCENE_COMBAT_ACTORS_RUNNING;
    combat->peTargetCount                       = 0;
    combat->battleRefs                          = 0;
    combat->expReward                           = 0;
    combat->bpReward                            = 0;
    combat->mpReward                            = 0;
    combat->lifeDrainHp                         = 0;
    combat->actor00700DeathAlert                = 0;
    combat->actor03700Flags                     = 0;
    combat->actor03700Wave                      = 0;
    combat->actor02400Alert                     = 0;
    combat->actor01600Wave                      = 0;
    combat->pairedEnemySignals                  = 0;
    combat->maggotCaterpillarEntranceReady      = 0;
    combat->madChaserAlertOwner                 = 0;
    combat->shrineEnemyPhase                    = SCENE_COMBAT_SHRINE_HIDDEN;
    combat->actor02500EntranceReady             = 0;
    combat->maggotCaterpillarAmbushReady        = 0;
    combat->actor00400HideRequested             = 0;
    combat->zebraStalkerGroupPhase              = SCENE_COMBAT_ZEBRA_STALKER_WAITING;
    combat->enemySoundBankQueued                = 0;
    combat->generatorDeathStarted               = 0;
    combat->zebraStalkerDeathAlert              = 0;
    combat->actor00300AttackAlert               = 0;
    combat->golemPawnRookDeathAlert             = 0;
    combat->field_2A                            = 0;
    // A cleared normal save uses the replay difficulty row; training stays normal.
    if (attachmentIsTrainingMode() == 1) {
        combat->difficulty = SCENE_COMBAT_DIFFICULTY_NORMAL;
    } else {
        save               = &gMcSaveData[MEMORY_CARD_SAVE_LIVE];
        difficulty         = (u8)save->state.gameMode;
        combat->difficulty = difficulty;
        if (difficulty == SCENE_COMBAT_DIFFICULTY_NORMAL) {
            if (save->state.clearCount != 0) {
                combat->difficulty = SCENE_COMBAT_DIFFICULTY_REPLAY;
            }
        }
    }
}

void sceneEngageBattle(s32 unusedArg)
{
    if (gSceneCombatState.signals.bytes.battlePhase == SCENE_COMBAT_BATTLE_IDLE) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_ENGAGED;
    }
}

void sceneLatchActionSignal(s32 actionSignal)
{
    if (actionSignal != SCENE_COMBAT_ACTION_SIGNAL_NONE) {
        gSceneCombatState.signals.bytes.actionFlags |= 1 << (actionSignal - 1);
    }
}

void sceneSetEnemyAlert(s32 alertClass)
{
    gSceneCombatState.signals.bytes.enemyAlert = alertClass;
}

void sceneAcquireBattleRef(s32 unusedArg)
{
    gSceneCombatState.battleRefs++;
}

void Gp_ReleaseStateF0Add(Task* arg0, s32 arg1)
{
    SceneCombatState* combat;
    SceneCombatState* rewards;
    EnemyParams*      params;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                sndEvtRequestMidiStop(0, 0xB4);
            }
        }
        params = ((Enemy*)arg0->spawnArg2.pointer)->param;
        if (params != NULL) {
            rewards             = &gSceneCombatState;
            rewards->expReward += params->exp;
            rewards->bpReward  += params->bp;
            rewards->mpReward  += params->mp;
        }
    }
}

void Gp_ReleaseStateF0Clear(Task* unusedTask, s32 unusedArg)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            combat->expReward                           = 0;
            combat->bpReward                            = 0;
            combat->mpReward                            = 0;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                sndEvtRequestMidiStop(0, 0xB4);
            }
        }
    }
}

void Gp_ReleaseStateF0(Task* arg0, s32 arg1)
{
    SceneCombatState* combat;

    combat = &gSceneCombatState;
    if (combat->battleRefs != 0) {
        combat->battleRefs--;
        if (combat->battleRefs == 0) {
            gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_FINISHED;
            combat->signals.bytes.actionFlags           = 0;
            combat->signals.bytes.enemyAlert            = 0;
            combat->signals.bytes.endDelayFrames        = SCENE_COMBAT_END_DELAY_FRAMES;
            if (!(gGameSession->flowFlags & GAME_SESSION_FLOW_SKIP_AREA_MUSIC)) {
                sndEvtRequestMidiStop(0, 0xB4);
            }
        }
    }
}
