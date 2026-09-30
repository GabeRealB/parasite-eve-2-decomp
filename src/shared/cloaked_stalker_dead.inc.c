#include "main/random.h"

/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Frame handler for the scene's `Gp_StateF0.field_4` mode. Mode 1 only refreshes the
/// coordinates, tint and shadow and mode 2 hides the model, both returning
/// without giving back the 8-byte scratch stack block. Otherwise the
/// `field_6CE` sequence runs: state 0 unlinks the actor and saves its pose,
/// state 1 sprays a randomly angled effect every fourth frame, and state 2
/// projects the actor before moving on to 3.
void stalkerDeadState(Enemy* arg0, Task* arg1)
{
    u8*              head;
    SVECTOR*         sc;
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              mode;
    u32              random;
    s16              anim;

    work                     = arg1->work;
    coord                    = &arg1->extra.tmd->coords[0];
    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(SVECTOR);
    sc                       = (SVECTOR*)(head - sizeof(SVECTOR));
    mode                     = Gp_StateF0.field_4;
    switch (mode) {
        case 0:
            arg1->extra.tmd->flags = 0;
            break;
        case 1:
            coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
            arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
            Gp_UpdateCoord(coord);
            actor402200UpdateTint(arg1);
            stalkerDrawShadowInline(arg1);
            return;
        case 2:
            arg1->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
            return;
    }
    switch (work->field_6CE) {
        case 0:
            arg0->recs = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_4E4);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_47C);
            Gp_UnlinkObj((WorldCollisionBody*)work->field_564);
            Gp_ReleaseStateF0Add(arg1, work->field_716);
            anim = 0x14;
            if (work->field_6F0 == 1) {
                anim = 0x10;
            }
            work->field_6C0  = anim;
            work->field_6CE  = 1;
            arg0->spawnState = work->field_6F0;
            Gp_SaveEnemyPose(arg0);
            break;
        case 1:
            if (!(work->field_6C4 & 3)) {
                sc->vx          = 0;
                sc->vz          = 0;
                random          = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                sc->vy          = -((random >> 16) & 0x1FF);
                gRandomLcgState = random;
                Gp_SpawnEff(0x600E0, &arg1->extra.tmd->coords[3], 0x400, sc);
            }
            break;
        case 2:
            stalkerQueueFrameCapture(&arg1->extra.tmd->coords[3], 0xC);
            stalkerCloakFade(arg1);
            func_8009EA50(work->field_6D8);
            work->field_6CE = 3;
            break;
    }
    stalkerHoldCueTimer(arg1);
    stalkerTickAnimInline(arg1);
    coord->composeStamp                     = GRAPHICS_COORD_DIRTY;
    arg1->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(coord);
    actor402200UpdateTint(arg1);
    stalkerDrawShadowInline(arg1);
    SCRATCH_STACK_RELEASE_BYTES(sizeof(SVECTOR));
}
