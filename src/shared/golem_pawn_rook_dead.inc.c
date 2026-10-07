#include "main/random.h"

/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Teardown state of the enemy (entry 2 of the package's task-state table).
/// `gSceneCombatState.actorControl` gates it: 1 only redraws and 2 hides the model and its lock-on
/// node, both returning; 0 shows them and runs the states. State 0 unlinks the enemy's lock-on
/// node and collision bodies, releases its state-F0 slot, settles on the idle
/// `downedPose` selects, files the pose with `Gp_SaveEnemyPose` so the enemy is
/// restored in that pose, and raises `gSceneCombatState.golemPawnRookDeathAlert`. State 1 spawns a spark
/// every fourth frame. Either way the animation slots advance or are reseeded
/// and the model is drawn with its ground shadow.
void golemPawnRookDeadState(Enemy* arg0, Task* arg1)
{
    GolemPawnRookWork* work;
    GolemPawnRookWork* animWork;
    GfxCoord*          coord;
    GfxCoord*          root;
    GfxCoord*          part;
    SVECTOR*           scratch;
    VECTOR3            pos;
    s16                anim;
    s16                duration;
    s32                i;
    u32                random;

    work    = arg1->work;
    coord   = arg1->extra.tmd->coords;
    scratch = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    switch (gSceneCombatState.actorControl) {
        case SCENE_COMBAT_ACTORS_RUNNING:
            arg1->extra.tmd->flags       = 0;
            arg0->node.state.parts.flags = 0;
            break;
        case SCENE_COMBAT_ACTORS_PAUSED:
            coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            root   = arg1->extra.tmd->coords;
            pos.vx = root->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = root->workm.t[2];
            worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
            root   = arg1->extra.tmd->coords;
            part   = &root[3];
            pos.vx = part->workm.t[0];
            pos.vy = root->workm.t[1];
            pos.vz = part->workm.t[2];
            effectDrawGroundShadow(&pos, 0x300, 0x80);
            return;
        case SCENE_COMBAT_ACTORS_HIDDEN:
            arg1->extra.tmd->flags       = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            arg0->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
            return;
    }
    switch (work->step) {
        case 0:
            arg0->recs = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->sightBody);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            if ((u32)((u16)work->actorId - 0x38) < 2U) {
                worldCollisionUnlinkBody(&work->laserBody);
            }
            sceneReleaseBattleRefWithRewards(arg1, work->actorId);
            anim = 0x1D;
            if (work->downedPose == 1) {
                anim = 0x19;
            }
            work->anim       = anim;
            work->step       = 1;
            arg0->spawnState = (u8)work->downedPose;
            Gp_SaveEnemyPose(arg0);
            gSceneCombatState.golemPawnRookDeathAlert = 1;
            break;
        case 1:
            if (!(work->animFrame & 3)) {
                scratch->vx     = 0;
                scratch->vz     = 0;
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                scratch->vy     = -((random >> 0x10) & 0x1FF);
                gRandomLcgState = random;
                Gp_SpawnEff(EFFECT_FLASH_BURST, &arg1->extra.tmd->coords[3], 0x400, scratch);
            }
            break;
    }
    animWork = arg1->work;
    if (animWork->anim != animWork->playingAnim) {
        animWork->playingAnim = (s16)(u16)animWork->anim;
        animWork->animFrame   = 0U;
        duration              = gGolemPawnRookAnimBlendFrames[animWork->anim];
        for (i = 1; i < 0x13; i++) {
            animationSeekSlotWithBlend(&animWork->rig.anim, i, animWork->anim, 0, duration);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    } else {
        animWork->animFrame++;
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&animWork->rig.anim, i);
        }
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
    }
    arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    root   = arg1->extra.tmd->coords;
    pos.vx = root->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = root->workm.t[2];
    worldCoordUpdateActorColor(arg1->spawnArg2.pointer, &pos, 0, 0);
    root   = arg1->extra.tmd->coords;
    part   = &root[3];
    pos.vx = part->workm.t[0];
    pos.vy = root->workm.t[1];
    pos.vz = part->workm.t[2];
    effectDrawGroundShadow(&pos, 0x300, 0x80);
    SCRATCH_STACK_RELEASE_BYTES(8);
}
