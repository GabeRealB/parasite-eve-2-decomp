#include "main/random.h"

/* Part of the library; see golem_pawn_rook.h. Inline helpers the fragments use. */

/// Emits one damaged-body spark every third call at a random local Y offset.
///
/// actor must have a live GOLEM body model and work. The caller selects when
/// sparks are enabled; this helper only advances dustTimer. Its eight-byte
/// offset is borrowed from the scratch stack and released before returning.
static __inline__ void _golemPawnRookSpawnDamageSparks(Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_DAMAGE_SPARK_FRAMES = 3,
        GOLEM_PAWN_ROOK_DAMAGE_SPARK_SIZE   = 256,
        GOLEM_PAWN_ROOK_DAMAGE_SPARK_Y_MASK = 511,
    };
    GolemPawnRookWork* work;
    SVECTOR*           sparkOffset;

    work        = actor->work;
    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    if (++work->dustTimer >= GOLEM_PAWN_ROOK_DAMAGE_SPARK_FRAMES) {
        work->dustTimer = 0;
        sparkOffset->vx = 0;
        sparkOffset->vz = 0;
        sparkOffset->vy = -(((gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT) >> 16) & GOLEM_PAWN_ROOK_DAMAGE_SPARK_Y_MASK);
        effectSpawn(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_DAMAGE_SPARK_SIZE, sparkOffset);
    }
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
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

/// Samples the body's colour and draws its ground shadow under model part 3.
///
/// bodyRoot and part 3 must have composed matrices in the same frame. Uses
/// bodyRoot's XYZ for colour, then part 3's X/Z with the model root's Y for
/// the shadow. This helper draws the shadow only; the task renderer draws
/// the TMD model. actor retains its model, lighting and enemy record.
static inline void _golemPawnRookUpdateColorAndDrawShadow(Task* actor, GfxCoord* bodyRoot)
{
    enum {
        GOLEM_PAWN_ROOK_SHADOW_HALF_SIZE = 768,
        GOLEM_PAWN_ROOK_SHADOW_SHADE     = 128,
    };
    VECTOR3   worldPos;
    GfxCoord* root;
    GfxCoord* part;

    worldPos.vx = bodyRoot->workm.t[0];
    worldPos.vy = bodyRoot->workm.t[1];
    worldPos.vz = bodyRoot->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &worldPos, 0, 0);
    root        = actor->extra.tmd->coords;
    part        = root + 3;
    worldPos.vx = part->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = part->workm.t[2];
    effectDrawGroundShadow(&worldPos, GOLEM_PAWN_ROOK_SHADOW_HALF_SIZE, GOLEM_PAWN_ROOK_SHADOW_SHADE);
}
