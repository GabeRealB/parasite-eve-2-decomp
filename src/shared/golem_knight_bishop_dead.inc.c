#include "main/random.h"

/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Frame handler for the scene's `gSceneCombatState.actorControl` mode. Mode 1 only refreshes the
/// coordinates, tint and shadow and mode 2 hides the model, both returning
/// without giving back the 8-byte scratch stack block. Otherwise `step`
/// runs: step 0 unlinks the enemy and its hurt, ground and strike bodies,
/// files `downedPose` as the saved pose and takes the lying animation, step 1
/// then sprays a randomly angled effect every fourth frame, and step 2, where
/// a restored corpse starts, queues the frame capture and the fade once
/// before moving on to 3.
void golemKnightBishopDeadState(Enemy* arg0, Task* arg1)
{
    u8*                    head;
    SVECTOR*               sc;
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    mode;
    u32                    random;
    s16                    anim;

    work                     = arg1->work;
    coord                    = &arg1->extra.tmd->coords[0];
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(SVECTOR);
    sc                       = (SVECTOR*)(head - sizeof(SVECTOR));
    mode                     = gSceneCombatState.actorControl;
    switch (mode) {
        case 0:
            arg1->extra.tmd->flags = 0;
            break;
        case 1:
            coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            actorRenderComposeCoord(coord);
            golemKnightBishopUpdateTintInline(arg1);
            golemKnightBishopDrawShadowInline(arg1);
            return;
        case 2:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    switch (work->step) {
        case 0:
            arg0->recs = 0;
            worldTargetUnlinkNode(&arg0->node);
            worldCollisionUnlinkBody(&work->groundBody);
            worldCollisionUnlinkBody(&work->hurtBody);
            worldCollisionUnlinkBody(&work->strikeBody);
            sceneReleaseBattleRefWithRewards(arg1, work->actorId);
            anim = 0x14;
            if (work->downedPose == 1) {
                anim = 0x10;
            }
            work->anim       = anim;
            work->step       = 1;
            arg0->spawnState = work->downedPose;
            areaSaveEnemyPose(arg0);
            break;
        case 1:
            if (!(work->animFrame & 3)) {
                sc->vx          = 0;
                sc->vz          = 0;
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                sc->vy          = -((random >> 16) & 0x1FF);
                gRandomLcgState = random;
                effectSpawn(EFFECT_FLASH_BURST, &arg1->extra.tmd->coords[3], 0x400, sc);
            }
            break;
        case 2:
            golemKnightBishopQueueFrameCapture(&arg1->extra.tmd->coords[3], 0xC);
            golemKnightBishopTranslucencyFade(arg1);
            modelLightingSetLayerMaterials(work->translucency);
            work->step = 3;
            break;
    }
    golemKnightBishopHoldCueTimer(arg1);
    golemKnightBishopTickAnimInline(arg1);
    coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(coord);
    golemKnightBishopUpdateTintInline(arg1);
    golemKnightBishopDrawShadowInline(arg1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}
