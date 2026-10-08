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
        effectSpawn(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], 0x100, rot);
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}

/// Consumes a standing GOLEM's pending buildup reaction and enters its hold.
///
/// Leaves downed bodies and other reaction bits alone. The body task must have
/// live GOLEM work and its Enemy spawn record; the buildup handler owns recovery.
static inline void _golemPawnRookApplyBuildupReaction(Task* actor)
{
    enum { GOLEM_PAWN_ROOK_ANIM_BUILDUP = 0x14 };
    Enemy*             enemy;
    GolemPawnRookWork* work;
    u8                 reactionFlags;

    enemy         = actor->spawnArg2.pointer;
    reactionFlags = enemy->reactionFlags;
    work          = actor->work;
    if ((reactionFlags & ENEMY_REACTION_BUILDUP) && (work->downedPose == 0)) {
        enemy->reactionFlags = reactionFlags & ENEMY_REACTION_BUILDUP_CLEAR;
        work->behavior       = GOLEM_PAWN_ROOK_BEHAVIOR_BUILDUP;
        work->anim           = GOLEM_PAWN_ROOK_ANIM_BUILDUP;
        work->step           = GOLEM_PAWN_ROOK_BEHAVIOR_START_STEP;
        work->buildupActive  = 1;
    }
}

/// Saves the body root's position, then applies its horizontal movement and floor step.
///
/// forwardSpeed is in parent-coordinate units per frame, multiplied by the root's
/// 12-fractional-bit forward axis. Positive Y presses a standing or newly falling
/// body toward the floor; that step ends at knockdownStage 2. Collision resolution
/// uses the saved position to undo a rejected move on the next frame.
static inline void _golemPawnRookStepRoot(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_ROOT_ROTATION_FRACTION_BITS = 12,
        GOLEM_PAWN_ROOK_ROOT_FLOOR_STEP             = 128,
        GOLEM_PAWN_ROOK_ROOT_FALLEN_STAGE           = 2,
    };
    GfxCoord*          root;
    GolemPawnRookWork* work;

    root                 = actor->extra.tmd->coords;
    work                 = actor->work;
    work->prevRootPos.vx = root->coord.t[0];
    work->prevRootPos.vy = root->coord.t[1];
    work->prevRootPos.vz = root->coord.t[2];
    root->coord.t[0]    += (s32)(root->coord.m[0][2] * work->forwardSpeed) >> GOLEM_PAWN_ROOK_ROOT_ROTATION_FRACTION_BITS;
    if (work->knockdownStage < GOLEM_PAWN_ROOK_ROOT_FALLEN_STAGE) {
        root->coord.t[1] += GOLEM_PAWN_ROOK_ROOT_FLOOR_STEP;
    }
    root->coord.t[2] += (s32)(root->coord.m[2][2] * work->forwardSpeed) >> GOLEM_PAWN_ROOK_ROOT_ROTATION_FRACTION_BITS;
}

/// Advances the body's part animations or blends them into a newly requested clip.
///
/// The initialized nineteen-slot rig drives slots 1..18, leaving root slot 0 alone.
/// anim must index the carrier's animation and blend tables. A changed request
/// resets animFrame; an unchanged request increments it once per call.
static inline void _golemPawnRookTickAnim(Task* actor)
{
    GolemPawnRookWork* work;
    s16                blendFrames;
    s32                slotIndex;

    work = actor->work;
    if (work->anim != work->playingAnim) {
        work->playingAnim = work->anim;
        work->animFrame   = 0;
        blendFrames       = gGolemPawnRookAnimBlendFrames[work->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->anim, 0, blendFrames);
        }
    } else {
        work->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
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
