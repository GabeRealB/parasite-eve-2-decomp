#include "main/random.h"

/* Part of the library; see golem_pawn_rook.h. Inline helpers the fragments use. */

/// Every third frame while `screamCharges` is clear, kicks a dust effect off the
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
    if (++work->dustTimer >= 3) {
        work->dustTimer = 0;
        head[-1].vx     = 0;
        rot->vz         = 0;
        rot->vy         = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & 0x1FF);
        Gp_SpawnEff(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], 0x100, rot);
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Takes a pending reaction: while `downedPose` is 0, bit 1 of the spawn
/// context's `reactionFlags` is cleared and the enemy switches to entry 0xA of
/// the `behavior` table with animation 0x14.
static inline void golemPawnRookApplyReaction(Task* actor)
{
    Enemy*             spawn;
    GolemPawnRookWork* work;
    u8                 flags;

    spawn = actor->spawnArg2.pointer;
    flags = spawn->reactionFlags;
    work  = actor->work;
    if ((flags & ENEMY_REACTION_BUILDUP) && (work->downedPose == 0)) {
        spawn->reactionFlags = flags & ENEMY_REACTION_BUILDUP_CLEAR;
        work->behavior       = GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP;
        work->anim           = 0x14;
        work->step           = 0;
        work->buildupActive  = 1;
    }
}

/// Saves the root coordinate's translation in `prevRootPos`, then
/// moves it `forwardSpeed` along its facing, raising it by 0x80 while `knockdownStage`
/// is below 2.
static inline void golemPawnRookStepRoot(Task* actor)
{
    GfxCoord*          coord;
    GolemPawnRookWork* work;

    coord                = actor->extra.tmd->coords;
    work                 = actor->work;
    work->prevRootPos.vx = coord->coord.t[0];
    work->prevRootPos.vy = coord->coord.t[1];
    work->prevRootPos.vz = coord->coord.t[2];
    coord->coord.t[0]   += (s32)(coord->coord.m[0][2] * work->forwardSpeed) >> 0xC;
    if (work->knockdownStage < 2) {
        coord->coord.t[1] += 0x80;
    }
    coord->coord.t[2] += (s32)(coord->coord.m[2][2] * work->forwardSpeed) >> 0xC;
}

/// Advances animation slots 1..0x12 by one frame, or, when `anim` names a
/// new animation, restarts the frame count and cross-fades every slot to it
/// over the animation's `gGolemPawnRookAnimBlendFrames` duration.
static inline void golemPawnRookTickAnim(Task* actor)
{
    GolemPawnRookWork* work;
    s16                duration;
    s32                i;

    work = actor->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        duration          = gGolemPawnRookAnimBlendFrames[work->anim];
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->anim, 0, duration);
        }
    } else {
        work->animFrame++;
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
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &pos, 0, 0);
    root   = actor->extra.tmd->coords;
    part   = root + 3;
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    effectDrawGroundShadow(&pos, 0x300, 0x80);
}
