#include "world_targets.h"

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

static __inline__ void project_slot(s32* sxy, WorldTargetReadout* slot);

static void* Gp_ScanLockNodes(Task* arg0, VECTOR3* out, s32 flag);

static void Gp_UpdateLockSlots(void);

static void* Gp_FindLockNodeAt(Task* arg0, VECTOR3* pos);

static void Gp_ClearLockSlots(void);

static s32 Gp_ProjectToSxy(WorldTargetNode* arg0, s32* sxy);

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

static __inline__ void project_slot(s32* sxy, WorldTargetReadout* slot)
{
    WorldTargetNode*               src;
    _WorldTargetProjectionScratch* projection;

    // The caller has matched this binding to a target still on the tracked list.
    src = slot->binding.node;
    SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetProjectionScratch);
    projection           = SCRATCH_STACK_CURSOR(_WorldTargetProjectionScratch);
    projection->point.vx = GP_NODE_ENEMY(src)->bodyPos.vx;
    projection->point.vy = GP_NODE_ENEMY(src)->bodyPos.vy;
    projection->point.vz = GP_NODE_ENEMY(src)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(src)->coord->workm);
    gte_ldv0(&projection->point);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&projection->depthCue);
    gte_stflg(&projection->projectionFlags);
    gte_stszotz(&projection->orderingDepth);
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetProjectionScratch);
}

void Gp_DrawTargetCursor(void)
{
    WorldTargetNode*             node;
    GameSession*                 sess;
    WorldCoordProjectionScratch* projection;
    POLY_FT4*                    prim;
    s32                          easing;
    s32                          frame;
    s32                          u;
    s32                          v;

    node = gWorldTargetListHead;
    if (Pad_RemapState->hideHud != 0) {
        return;
    }
    Gp_UpdateLockSlots();
    sess = gGameSession;
    if (sess->sceneUpdatesPaused == 1) {
        return;
    }
    if (Gp_StateC08.mode == ATTACHMENT_MODE_ARMED || Gp_StateC08.mode == ATTACHMENT_MODE_CAST) {
        return;
    }
    if (Gp_StateC08.mode == ATTACHMENT_MODE_WHEEL) {
        return;
    }
    if (sess->eventState != 0) {
        return;
    }
    if (sess->hideHud != 0) {
        return;
    }
    for (; node != NULL; node = node->next) {
        if (node->state.parts.targeted != 0 && !(node->state.parts.flags & WORLD_TARGET_NOT_LOCKABLE)) {
            easing               = 0;
            projection           = SCRATCH_STACK_RESERVE_BLOCK(WorldCoordProjectionScratch);
            projection->point.vx = GP_NODE_ENEMY(node)->bodyPos.vx;
            projection->point.vy = GP_NODE_ENEMY(node)->bodyPos.vy;
            projection->point.vz = GP_NODE_ENEMY(node)->bodyPos.vz;
            actorRenderComposeCoord(GP_NODE_ENEMY(node)->coord);
            gte_SetRotMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_SetTransMatrix(&GP_NODE_ENEMY(node)->coord->workm);
            gte_RotTransPers(&projection->point, &projection->screen, &projection->depthCue,
                             &projection->projectionFlags, &projection->orderingDepth);
            if (D_80115260 != node) {
                if (D_80115260 == NULL) {
                    D_80115264 = 0xFF;
                } else {
                    D_80115264 = 0;
                }
                D_80115260 = node;
            }
            if (D_80115264 < 5) {
                D_8010F9EC += ((projection->screen.vx << 8) - D_8010F9EC) >> 1;
                D_8010F9F0 += ((projection->screen.vy << 8) - D_8010F9F0) >> 1;
                if (projection->screen.vx == (D_8010F9EC >> 8) && projection->screen.vy == (D_8010F9F0 >> 8)) {
                    D_80115264 = 0xFF;
                } else {
                    D_80115264++;
                }
                easing                = 1;
                projection->screen.vx = D_8010F9EC >> 8;
                projection->screen.vy = D_8010F9F0 >> 8;
            } else {
                D_8010F9EC = projection->screen.vx << 8;
                D_8010F9F0 = projection->screen.vy << 8;
            }
            // Convert the eased projection to the cursor's display coordinates.
            projection->screen.vy -= gDisplayState.vramYOffset;
            frame                  = gDisplayState.animFrame % 24 / 3;
            prim                   = gGpuPrimCursor;
            gGpuPrimCursor         = prim + 1;
            if (easing == 1) {
                prim->x0 = prim->x2 = projection->screen.vx - 8;
                prim->x1 = prim->x3 = projection->screen.vx + 8;
                prim->y0 = prim->y1 = projection->screen.vy - 8;
                prim->y2 = prim->y3 = projection->screen.vy + 8;
            } else {
                prim->x0 = prim->x2 = projection->screen.vx - 0x10;
                prim->x1 = prim->x3 = projection->screen.vx + 0x10;
                prim->y0 = prim->y1 = projection->screen.vy - 0x10;
                prim->y2 = prim->y3 = projection->screen.vy + 0x10;
            }
            u = (frame & 3) << 5;
            v = (frame >> 2) << 5;
            setUV4(prim, u + 0x40, v, u + 0x60, v, u + 0x40, v + 0x20, u + 0x60, v + 0x20);
            prim->clut  = 0x3C81;
            prim->tpage = 0x3E;
            setlen(prim, 9);
            setcode(prim, 0x2F);
            addPrim(gGpuCurrentOt, prim);
            SCRATCH_STACK_RELEASE_BLOCK(WorldCoordProjectionScratch);
            break;
        }
    }
    if (node == NULL) {
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
        Gp_GetLockPos(best, out);
    }
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetLockScanScratch);
    return best;
}

void func_800DA6E8(void* arg0, s32 arg1, s32 arg2)
{
    WorldTargetReadout* found;
    s32                 i;
    WorldTargetReadout* p;

    found = NULL;
    i     = 0;
    p     = Gp_LockSlots;
    // Non-negative totals and healing totals occupy separate slots.
loop:
    if (p->binding.node == arg0) {
        if (arg1 >= 0) {
            if (p->amount >= 0) {
                found = p;
                goto done;
            }
            p++;
        } else if (p->amount < 0) {
            found = p;
            goto done;
        } else {
            p++;
        }
    } else {
        p++;
    }
    i++;
    if (i < 0x20) {
        goto loop;
    }
done:
    if (found == NULL) {
        i = 0;
        p = Gp_LockSlots;
    loop2:
        if (p->binding.node == NULL) {
            found           = p;
            p->binding.node = arg0;
            found->amount   = 0;
        } else {
            i++;
            p++;
            if (i < 0x20) {
                goto loop2;
            }
        }
        if (found != NULL) {
            goto update;
        }
    } else {
    update:
        found->framesLeft = WORLD_TARGET_READOUT_FRAMES;
        found->amount    += arg1;
    }
}

static void Gp_UpdateLockSlots(void)
{
    RECT                rect;
    u8                  buf[16];
    TextDrawReq         req;
    s32                 i;
    WorldTargetReadout* slot;
    u8*                 bufp;
    TextDrawReq*        reqp;
    s32                 x;
    s32                 y;
    s32                 val;
    s32                 x14;
    s32                 ot;
    void*               obj;
    WorldTargetNode*    node;
    s32                 found;

    slot = Gp_LockSlots;
    i    = 0;
    bufp = buf;
    reqp = &req;
    ot   = -0xA;
    do {
        obj = slot->binding.node;
        if (obj == NULL) {
            goto empty;
        }
        node  = gWorldTargetListHead;
        found = 0;
        if (node != NULL) {
            do {
                if (obj == node) {
                    found = 1;
                    goto check_found;
                }
                node = node->next;
            } while (node != NULL);
        }
    check_found:
        if (found != 0) {
            // Project the bound target into this slot's screen position.
            project_slot(&slot->screen.packed, slot);
        } else {
            // The target has left the tracked list. Keep the last projection until the countdown ends.
            slot->binding.word = WORLD_TARGET_READOUT_DEPARTED;
        }

        val = slot->amount;
        if (val >= 0) {
            x = slot->screen.xy.vx + 0xA;
            y = slot->screen.xy.vy + 4;
        } else {
            x = slot->screen.xy.vx - 0xA;
            y = slot->screen.xy.vy - 0x10;
        }
        if (x < -0x88) {
            x = (x & 7) - 0x8F;
        }
        if (x >= 0x89) {
            x = -(x & 7) + 0x8F;
        }
        if (y >= 0x55) {
            y = (y & 7) + 0x4D;
        }
        if (y < -0x64) {
            y = -(y & 7) - 0x5D;
        }

        x14            = x + 0xE;
        req.x          = x14;
        req.y          = y;
        req.otIndex    = ot;
        req.colorRgb   = 0x37A78;
        req.glyphTable = TEXT_GLYPH_TABLE_SMALL;
        req.alignment  = TEXT_ALIGNMENT_RIGHT;
        req.drawMode   = TEXT_DRAW_OUTLINED;

        val = slot->amount;
        if (val < 0) {
            req.colorRgb = 0x808008;
            val          = -val;
        }
        if (val >= WORLD_TARGET_READOUT_DISPLAY_LIMIT) {
            val = WORLD_TARGET_READOUT_DISPLAY_MAX;
        }

        req.x        = x14;
        req.drawMode = TEXT_DRAW_FILL_ONLY;
        textDrawString(reqp, textItoaSigned(bufp, val));
        // Queue the outline after the fill so it executes first in the same OT entry.
        req.x        = x14;
        req.drawMode = TEXT_DRAW_OUTLINE_ONLY;
        textDrawString(reqp, textItoaSigned(bufp, val));

        rect.x = x - 0x10;
        rect.y = y - 8;
        rect.w = 0x20;
        rect.h = 0xC;
        if (val >= 0x3E8) {
            rect.x = x - 0x18;
            rect.w = 0x28;
        } else if (val < 0x64) {
            rect.x = x - 8;
            rect.w = 0x18;
        }
        uiDrawRectFrame(&rect, -0xA, 2, NULL);

        {
            s16 timer;
            timer = slot->framesLeft;
            timer--;
            slot->framesLeft = timer;
            if (timer > 0) {
                goto next;
            }
        }
        slot->amount       = 0;
        slot->framesLeft   = 0;
        slot->binding.node = NULL;
        goto next;

    empty:
        slot->amount     = 0;
        slot->framesLeft = 0;
    next:
        i++;
        slot++;
    } while (i < 0x20);
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
                goto clearTrackingState;
            }
            incomingLink = &(*incomingLink)->next;
        }
        if (*incomingLink != NULL) {
            *incomingLink = node->next;
        }
    clearTrackingState:
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

void Gp_AssignNodeSlot0(WorldTargetNode* node)
{
    Task*            work;
    GameActor*       actor;
    WorldTargetNode* previous;
    u8               val;

    work = gPlayerActorTasks[PLAYER_ACTOR_TASK_PLAYER];
    if (work != NULL) {
        actor    = work->work;
        previous = actor->targetNode;
        if (previous != NULL) {
            previous->state.parts.targeted = 0;
        }
        actor->targetNode = node;
    }
    val                        = node->state.parts.flags;
    node->state.parts.targeted = 1;
    node->state.parts.flags    = val & WORLD_TARGET_NOT_LOCKABLE_CLEAR;
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

void Gp_GetLockPos(WorldTargetNode* arg0, VECTOR3* out)
{
    GfxCoord* world;
    GfxCoord* coord;
    u8*       head;
    MATRIX*   mat;

    if (arg0 == NULL) {
        printf(Gp_StrGetLockPosNull);
        out->vx = 0;
        out->vy = 0;
        out->vz = 0;
        return;
    }

    coord = GP_NODE_ENEMY(arg0)->coord;
    world = &gGfxViewCoord;
    if (coord == world) {
        out->vx = GP_NODE_ENEMY(arg0)->bodyPos.vx;
        out->vy = GP_NODE_ENEMY(arg0)->bodyPos.vy;
        out->vz = GP_NODE_ENEMY(arg0)->bodyPos.vz;
        return;
    }

    head                       = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(void) = head - 0x28;
    actorRenderComposeCoord(coord);
    mat = (MATRIX*)(head - 0x20);
    gfxMakeRelativeTransform(&world->workm, &coord->workm, mat);
    gte_SetRotMatrix(mat);
    gte_SetTransMatrix(mat);
    gte_ldlvl(&GP_NODE_ENEMY(arg0)->bodyPos);
    gte_rtirtr();
    gte_stlvl(out);
    SCRATCH_STACK_RELEASE_BYTES(0x28);
}

static void Gp_ClearLockSlots(void)
{
    s32                 i;
    WorldTargetReadout* p;

    p = Gp_LockSlots;
    i = 0;
    do {
        i++;
        p->binding.node = NULL;
        p->amount       = 0;
        p->framesLeft   = 0;
        p++;
    } while (i < 0x20);
}

void Gp_ResetLinkState(void)
{
    gWorldTargetListHead = NULL;
    Gp_ClearLockSlots();
    D_8010F9F0 = 0xFFF00000;
    D_8010F9EC = 0xFFF00000;
}

static s32 Gp_ProjectToSxy(WorldTargetNode* arg0, s32* sxy)
{
    _WorldTargetProjectionScratch* projection;
    s32                            ret;

    projection           = SCRATCH_STACK_RESERVE_BLOCK(_WorldTargetProjectionScratch);
    projection->point.vx = GP_NODE_ENEMY(arg0)->bodyPos.vx;
    projection->point.vy = GP_NODE_ENEMY(arg0)->bodyPos.vy;
    projection->point.vz = GP_NODE_ENEMY(arg0)->bodyPos.vz;
    gte_SetRotMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_SetTransMatrix(&GP_NODE_ENEMY(arg0)->coord->workm);
    gte_ldv0(&projection->point);
    gte_rtps();
    gte_stsxy(sxy);
    gte_stdp(&projection->depthCue);
    gte_stflg(&projection->projectionFlags);
    gte_stszotz(&projection->orderingDepth);
    ret = projection->orderingDepth;
    SCRATCH_STACK_RELEASE_BLOCK(_WorldTargetProjectionScratch);
    return ret;
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

void Gp_InitStateF0(void)
{
    SceneCombatState* combat;
    McSaveData*       save;
    u8                difficulty;

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
    if (Gp_IsDebugAttachRoom() == 1) {
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

void Gp_ArmStateF0(s32 arg0)
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
                SndEvt_EnqueueType2(0, 0xB4);
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
                SndEvt_EnqueueType2(0, 0xB4);
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
                SndEvt_EnqueueType2(0, 0xB4);
            }
        }
    }
}
