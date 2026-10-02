#include "main/random.h"

/* Part of the library; see golem_pawn_rook.h. Inline helpers the fragments use. */

/// Every third frame while `field_6C4` is clear, kicks a dust effect off the
/// fourth body coordinate with a random upward velocity.
static __inline__ void golemPawnRookSpawnDust(Task* actor)
{
    GolemPawnRookWork* work;
    SVECTOR*           head;
    SVECTOR*           rot;

    work                          = actor->work;
    head                          = SCRATCH_STACK_CURSOR(SVECTOR);
    rot                           = head - 1;
    SCRATCH_STACK_CURSOR(SVECTOR) = rot;
    if (++work->field_6B0 >= 3) {
        work->field_6B0 = 0;
        head[-1].vx     = 0;
        rot->vz         = 0;
        rot->vy         = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1FF);
        Gp_SpawnEff(0x600E0, &actor->extra.tmd->coords[3], 0x100, rot);
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Takes a pending reaction: while `field_6B8` is 0, bit 1 of the spawn
/// context's `reactionFlags` is cleared and the enemy switches to entry 0xA of
/// the `field_6A6` table with animation 0x14.
static inline void golemPawnRookApplyReaction(Task* actor)
{
    Enemy*             spawn;
    GolemPawnRookWork* work;
    u8                 flags;

    spawn = actor->spawnArg2.pointer;
    flags = spawn->reactionFlags;
    work  = actor->work;
    if ((flags & ENEMY_REACTION_BUILDUP) && (work->field_6B8 == 0)) {
        spawn->reactionFlags = flags & ENEMY_REACTION_BUILDUP_CLEAR;
        work->field_6A6      = 0xA;
        work->field_694      = 0x14;
        work->field_6A8      = 0;
        work->field_6E0      = 1;
    }
}

/// Saves the root coordinate's translation in `field_678`..`field_680`, then
/// moves it `field_69C` along its facing, raising it by 0x80 while `field_6DE`
/// is below 2.
static inline void golemPawnRookStepRoot(Task* actor)
{
    GfxCoord*          coord;
    GolemPawnRookWork* work;

    coord              = actor->extra.tmd->coords;
    work               = actor->work;
    work->field_678    = coord->coord.t[0];
    work->field_67C    = coord->coord.t[1];
    work->field_680    = coord->coord.t[2];
    coord->coord.t[0] += (s32)(coord->coord.m[0][2] * work->field_69C) >> 0xC;
    if (work->field_6DE < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->field_69C) >> 0xC;
}

/// Advances animation slots 1..0x12 by one frame, or, when `field_694` names a
/// new animation, restarts the frame count and cross-fades every slot to it
/// over the animation's `gGolemPawnRookAnimBlendFrames` duration.
static inline void golemPawnRookTickAnim(Task* actor)
{
    GolemPawnRookWork* work;
    s16                duration;
    s32                i;

    work = actor->work;
    if (work->field_694 != work->field_696) {
        work->field_696 = work->field_694;
        work->field_698 = 0;
        duration        = gGolemPawnRookAnimBlendFrames[work->field_694];
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->field_694, 0, duration);
        }
    } else {
        work->field_698++;
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
    }
}

/// Updates the enemy's colour from `coord`'s world position and draws the
/// ground quad under part 3.
static inline void golemPawnRookDraw(Task* actor, GfxCoord* coord)
{
    VECTOR3   pos;
    GfxCoord* root;
    GfxCoord* part;

    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    Gp_UpdateActorColor(actor->spawnArg2.pointer, (VECTOR*)&pos, 0, 0);
    root   = actor->extra.tmd->coords;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    Gp_DrawEffGroundQuad(&pos, 0x300, 0x80);
}
