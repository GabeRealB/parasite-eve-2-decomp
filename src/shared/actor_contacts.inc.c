#include "actor_contacts.h"

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>
#include <psyq/abs.h>
#include <psyq/memory.h>

#include "overlay.h"

#include "actors/actor.h"

#include "gte.h"
#include "types.h"

#include "gameplay/actor_render.h"
#include "gameplay/world_collision.h"

#include "main/coord.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/scratch.h"
#include "main/session.h"
#include "main/session_types.h"

static s32 ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count);
static s32 ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos);
static s32 ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push);

/// Turns joint `coord` by `yaw` about the world Y axis: builds its world
/// rotation in a matrix carved off the scratchpad head, applies the turn,
/// converts the result back into the parent's frame, writes the 3x3 into the
/// joint and refreshes it.
static void ActorContact_TurnJoint(GfxCoord* coord, s16 yaw)
{
    MATRIX*   rotation;
    GfxCoord* out;

    SCRATCH_STACK_RESERVE_BLOCK(MATRIX);
    rotation = SCRATCH_STACK_CURSOR(MATRIX);
    actorAccumulateRotation(coord, rotation, &gGfxViewCoord);
    RotMatrixY(yaw, rotation);
    out = actorLocalizeRotation(coord, rotation);
    memcpy(out->coord.m, rotation->m, sizeof(out->coord.m));
    out->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(out);
    SCRATCH_STACK_RELEASE_BLOCK(MATRIX);
}

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off the scratch stack, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `Mc_SaveData[0].state.actorsFrozen` is 1.
static s32 ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count)
{
    ActorRepelScratch* head;
    ActorRepelScratch* s;
    ActorRepelScratch* blk;
    SVECTOR*           offset;

    if (Mc_SaveData[0].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    head                                    = SCRATCH_STACK_CURSOR(ActorRepelScratch);
    blk                                     = head - 1;
    SCRATCH_STACK_CURSOR(ActorRepelScratch) = blk;
    s                                       = blk;
    Gp_UpdateCoord(coord);
    s->pos.vx  = coord->workm.t[0];
    s->pos.vy  = coord->workm.t[1];
    s->pos.vz  = coord->workm.t[2];
    s->last.vz = 0;
    s->last.vy = 0;
    s->last.vx = 0;
    s->hit     = 0;
    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            s->dist[s->i] = 0x7FFE;
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->pos, &recs[s->i], &s->offset);
            s->last.vx = s->offset.vx;
            s->last.vz = s->offset.vz;
        }
    }
    s->len = SquareRoot0(s->offset.vx * s->offset.vx + s->offset.vy * s->offset.vy +
                         s->offset.vz * s->offset.vz);
    if (s->len > 0x100) {
        offset = &s->offset;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(ActorRepelScratch);
    return s->hit;
}

/// Steers `coord` away from the obstacles among the first `count` contact
/// records: collects the bearing of up to eight records of kind 0x10000 or
/// 0x30000 (in the XZ plane, or XY when the facing column is near vertical),
/// discards any pair more than 0x400 apart, and for each remaining bearing
/// nudges both `coord`'s translation and `*pos` a short step away from it. `*pos`
/// accumulates the total nudge. Returns whether any record was of kind
/// 0x10000; returns 0 at once when `gGameSession->viewReady` or `Mc_SaveData[0].state.actorsFrozen`
/// is 1.
static s32 ActorContact_Steer(GfxCoord* coord, WorldCollisionContact* recs, s16 count, SVECTOR* pos)
{
    u8*                  head;
    OverlayAvoidScratch* s;
    s16                  diff;

    if (gGameSession->viewReady == 1 || Mc_SaveData[0].state.actorsFrozen == 1) {
        return 0;
    }

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(OverlayAvoidScratch);
    s                        = SCRATCH_STACK_CURSOR(OverlayAvoidScratch);
    s->blocked               = 0;
    pos->vz                  = 0;
    pos->vy                  = 0;
    pos->vx                  = 0;

    Gfx_MatrixCol1(&coord->workm, (SVECTOR*)(head - 0x34));
    VectorNormalSS((SVECTOR*)(head - 0x34), (SVECTOR*)(head - 0x34));

    if (ABS(s->dir.vz) < 0x818) {
        s->face = ratan2(-coord->workm.m[2][0], coord->workm.m[2][2]);
    } else {
        s->face = -ratan2(-coord->workm.m[0][2], coord->workm.m[1][2]);
    }

    s->eye.vx = (u16)coord->workm.t[0];
    s->eye.vy = (u16)coord->workm.t[1];
    s->eye.vz = (u16)coord->workm.t[2];
    s->count  = 0;

    for (s->i = 0; s->i < count; s->i++) {
        if (recs[s->i].key.value == 0) {
            break;
        }
        s->kind = recs[s->i].key.value & 0xFFFF0000;
        switch (s->kind) {
            case 0x10000:
                s->blocked = 1;
            case 0x30000:
                break;
            default:
                continue;
        }

        if (ABS(s->dir.vz) < 0x818) {
            s->angle[s->count] = overlayBearingXZ((SVECTOR3*)&recs[s->i].point, &s->eye);
        } else {
            s->angle[s->count] = overlayBearingXY((SVECTOR3*)&recs[s->i].point, &s->eye);
        }
        s->ok[s->count] = 1;
        s->count++;
        if (s->count >= 8) {
            break;
        }
    }

    for (s->i = 0; s->i < s->count; s->i++) {
        for (s->j = s->i + 1; s->j < s->count; s->j++) {
            s->diff = actorWrapAngle((u16)s->angle[s->i] - (u16)s->angle[s->j]);
            if (abs(s->diff) > 0x400) {
                s->ok[s->i] = 0;
                s->ok[s->j] = 0;
            }
        }
        if (s->ok[s->i] != 0) {
            diff = ((u16)s->angle[s->i] - (u16)s->face) +
                   ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
            s->diff = diff;
            Gfx_RotMatrixY(&s->m, diff, 1);
            Gfx_MatrixCol2(&s->m, &s->dir);
            VectorNormalSS(&s->dir, &s->dir);
            gte_lddp(-10);
            gte_ldsv(&s->dir);
            gte_gpf12();
            gte_stsv(&s->dir);
            pos->vx           += s->dir.vx;
            pos->vz           += s->dir.vz;
            coord->coord.t[0] += s->dir.vx;
            coord->coord.t[2] += s->dir.vz;
        }
    }

    SCRATCH_STACK_RELEASE_BYTES(sizeof(OverlayAvoidScratch));
    return s->blocked != 0;
}

static s32 ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2)
{
    OverlayDeltaFlag* s;
    s32               val;

    s        = SCRATCH_STACK_RESERVE_BLOCK(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                    += s->delta.vx.w >> 16;
        coord->coord.t[2]                    += s->delta.vz.w >> 16;
        ActorContact_GetScratchPosition()->vx = s->delta.vx.w >> 16;
        ActorContact_GetScratchPosition()->vy = s->delta.vy.w >> 16;
        ActorContact_GetScratchPosition()->vz = s->delta.vz.w >> 16;
        val                                   = s->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                ActorContact_GetScratchPosition()->vx++;
            } else {
                coord->coord.t[0]--;
                ActorContact_GetScratchPosition()->vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                ActorContact_GetScratchPosition()->vz++;
            } else {
                coord->coord.t[2]--;
                ActorContact_GetScratchPosition()->vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayDeltaFlag);
    return s->moved;
}

/// Pushes `coord` `push` units away from each obstacle among the first
/// `count` contact records (kind 0x10000 or 0x30000) whose bearing lies within
/// 0x400 of every other obstacle's. Bearings are taken in world space from the
/// frame's position, relative to the point one unit in front of it. Returns
/// whether any push was applied; returns 0 at once when
/// `gGameSession->viewReady` is 1.
static s32 ActorContact_Push(GfxCoord* coord, WorldCollisionContact* recs, s16 count, s16 push)
{
    OverlayBisectorScratch* st;
    s32                     hit;

    if (gGameSession->viewReady == 1) {
        return 0;
    }

    SCRATCH_STACK_RESERVE_BLOCK(OverlayBisectorScratch);
    st         = SCRATCH_STACK_CURSOR(OverlayBisectorScratch);
    st->eye.vx = (u16)coord->coord.t[0];
    st->eye.vy = (u16)coord->coord.t[1];
    st->eye.vz = (u16)coord->coord.t[2];

    overlayToWorld(coord->parent, &st->eye);

    st->aim.vx = 0;
    st->aim.vy = 0;
    st->aim.vz = 0x1000;

    overlayToWorld2(coord, &st->aim);

    for (st->i = 0; st->i < count; st->i++) {
        if (recs[st->i].key.value == 0) {
            st->angle[st->i] = 0x7FFE;
            break;
        }
        st->kind = recs[st->i].key.value & 0xFFFF0000;
        if ((st->kind != 0x10000) && (st->kind != 0x30000)) {
            st->angle[st->i] = 0x7FFF;
        } else {
            st->delta.vx     = (u16)recs[st->i].point.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)recs[st->i].point.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)recs[st->i].point.vz - (u16)st->eye.vz;
            st->angle[st->i] = ratan2(st->delta.vx, st->delta.vz);

            st->delta.vx     = (u16)st->aim.vx - (u16)st->eye.vx;
            st->delta.vy     = (u16)st->aim.vy - (u16)st->eye.vy;
            st->delta.vz     = (u16)st->aim.vz - (u16)st->eye.vz;
            st->angle[st->i] = (u16)st->angle[st->i] - ratan2(st->delta.vx, st->delta.vz);

            st->angle[st->i] = actorWrapAngle(st->angle[st->i]);
        }
    }

    st->hit = 0;
    for (st->i = 0; st->i < count; st->i++) {
        if (st->angle[st->i] == 0x7FFE) {
            break;
        }
        if (st->angle[st->i] == 0x7FFF) {
            continue;
        }
        for (st->j = 0; st->j < count; st->j++) {
            if (st->i == st->j) {
                continue;
            }
            if (st->angle[st->j] == 0x7FFF) {
                continue;
            }
            if (st->angle[st->j] != 0x7FFE) {
                st->diff = (u16)st->angle[st->j] - (u16)st->angle[st->i];
                st->diff = actorWrapAngle(st->diff);
                if (abs(st->diff) > 0x400) {
                    break;
                }
                if (st->angle[st->j] != 0x7FFE) {
                    if (st->j + 1 < count) {
                        continue;
                    }
                }
            }
            st->hit = 1;
            Gfx_RotMatrixY(&st->m,
                           st->angle[st->i] + (s16)ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]),
                           1);
            Gfx_MatrixCol2(&st->m, &st->aim);
            VectorNormalSS(&st->aim, &st->aim);
            gte_lddp(-push);
            gte_ldsv(&st->aim);
            gte_gpf12();
            gte_stsv(&st->delta);
            coord->coord.t[0] += st->delta.vx;
            coord->coord.t[2] += st->delta.vz;
            break;
        }
    }

    hit = st->hit;
    SCRATCH_STACK_RELEASE_BLOCK(OverlayBisectorScratch);
    return hit;
}
