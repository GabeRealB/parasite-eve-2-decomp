#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Restarts a changed corpse clip with blending, or advances its existing pose.
///
/// `actor` owns an initialized nineteen-slot body rig and `coord` is its model
/// root. The requested clip indexes the carrier's 32-entry blend-frame table
/// and must be loaded in the animation bank. A changed request resets the
/// halfword frame counter and seeks slots 1..18 to frame 0 with that whole-frame
/// blend duration; an unchanged request increments the counter and ticks them.
/// Slot 0 is untouched. Marks the root dirty after either path, without composing
/// it. Work, clip and model storage remain borrowed for their task lifetime;
/// calls require initialized animation/scratch/GTE state and retain no new pointer.
static inline void _golemPawnRookAdvanceCorpsePose(Task* actor, GfxCoord* coord)
{
    GolemPawnRookWork* animWork;
    s16                blendFrames;
    s32                slotIndex;

    animWork = actor->work;
    if (animWork->anim != animWork->playingAnim) {
        animWork->playingAnim = animWork->anim;
        animWork->animFrame   = 0U;
        blendFrames           = gGolemPawnRookAnimBlendFrames[animWork->anim];
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(animWork->rig.slots); slotIndex++) {
            animationSeekSlotWithBlend(&animWork->rig.anim, slotIndex, animWork->anim, 0, blendFrames);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        animWork->animFrame++;
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(animWork->rig.slots); slotIndex++) {
            animationTickSlot(&animWork->rig.anim, slotIndex);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
}

/// Saves a dead GOLEM's pose and keeps its corpse animated, lit and shadowed.
///
/// enemy and actor remain live throughout this persistent body state. Step 0
/// unlinks combat bodies and lock-on, awards the battle reference, and saves
/// the downed pose; step 1 emits a spark every fourth animation frame. Restored
/// corpses start at step 2 and skip those effects. Actor control pauses updates
/// or hides the corpse. Its eight scratch bytes are released on the running
/// path; paused and hidden returns leave them reserved until the next main-loop
/// frame resets the shared cursor.
static void _golemPawnRookDeadState(Enemy* enemy, Task* actor)
{
    enum {
        GOLEM_PAWN_ROOK_CORPSE_RELEASE_COMBAT   = 0,
        GOLEM_PAWN_ROOK_CORPSE_SPARKS           = 1,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_FRAME_MASK = 3,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_Y_MASK     = 511,
        GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM      = 0x19,
        GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM       = 0x1D,
        GOLEM_PAWN_ROOK_FIRST_LAUNCHER_ID       = 0x38,
        GOLEM_PAWN_ROOK_CORPSE_SPARK_SIZE       = 1024,
        GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE = 768,
        GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE     = 128,
    };

    GolemPawnRookWork* work;
    GfxCoord*          coord;
    GfxCoord*          root;
    GfxCoord*          part;
    SVECTOR*           sparkOffset;
    VECTOR3            worldPos;
    s16                corpseAnim;
    u32                randomDraw;

    work        = actor->work;
    coord       = actor->extra.tmd->coords;
    sparkOffset = SCRATCH_STACK_RESERVE_BLOCK(SVECTOR);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            actor->extra.tmd->flags       = 0;
            enemy->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            coord->composeStamp                      = GRAPHICS_COORD_DIRTY;
            actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            root        = actor->extra.tmd->coords;
            worldPos.vx = root->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = root->workm.t[2];
            worldCoordUpdateActorColor(actor->spawnArg2.pointer, &worldPos, 0, 0);
            root        = actor->extra.tmd->coords;
            part        = &root[3];
            worldPos.vx = part->workm.t[0];
            worldPos.vy = root->workm.t[1];
            worldPos.vz = part->workm.t[2];
            effectDrawGroundShadow(&worldPos, GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE, GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            actor->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    // Release combat participation once, while retaining the corpse model and work.
    switch (work->step) {
        case GOLEM_PAWN_ROOK_CORPSE_RELEASE_COMBAT:
            enemy->recs = NULL;
            worldTargetUnlinkNode(&enemy->node);
            worldCollisionUnlinkBody(&work->sightBody);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            if ((u32)((u16)work->actorId - GOLEM_PAWN_ROOK_FIRST_LAUNCHER_ID) < 2U) {
                worldCollisionUnlinkBody(&work->laserBody);
            }
            sceneReleaseBattleRefWithRewards(actor, work->actorId);
            corpseAnim = GOLEM_PAWN_ROOK_CORPSE_FRONT_ANIM;
            if (work->downedPose == GOLEM_PAWN_ROOK_DOWNED_BEHIND) {
                corpseAnim = GOLEM_PAWN_ROOK_CORPSE_BEHIND_ANIM;
            }
            work->anim        = corpseAnim;
            work->step        = GOLEM_PAWN_ROOK_CORPSE_SPARKS;
            enemy->spawnState = (u8)work->downedPose;
            areaSaveEnemyPose(enemy);
            gSceneCombatState.golemPawnRookDeathAlert = 1;
            break;
        case GOLEM_PAWN_ROOK_CORPSE_SPARKS:
            if (!(work->animFrame & GOLEM_PAWN_ROOK_CORPSE_SPARK_FRAME_MASK)) {
                sparkOffset->vx = 0;
                sparkOffset->vz = 0;
                randomDraw      = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                sparkOffset->vy = -((randomDraw >> 0x10) & GOLEM_PAWN_ROOK_CORPSE_SPARK_Y_MASK);
                gRandomLcgState = randomDraw;
                effectSpawn(EFFECT_FLASH_BURST, &actor->extra.tmd->coords[3], GOLEM_PAWN_ROOK_CORPSE_SPARK_SIZE, sparkOffset);
            }
            break;
    }
    // Keep the saved downed pose animated and visible after combat has ended.
    _golemPawnRookAdvanceCorpsePose(actor, coord);
    actor->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    root        = actor->extra.tmd->coords;
    worldPos.vx = root->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(actor->spawnArg2.pointer, &worldPos, 0, 0);
    root        = actor->extra.tmd->coords;
    part        = &root[3];
    worldPos.vx = part->workm.t[0];
    worldPos.vy = root->workm.t[1];
    worldPos.vz = part->workm.t[2];
    effectDrawGroundShadow(&worldPos, GOLEM_PAWN_ROOK_CORPSE_SHADOW_HALF_SIZE, GOLEM_PAWN_ROOK_CORPSE_SHADOW_SHADE);
    SCRATCH_STACK_RELEASE_BLOCK(SVECTOR);
}
