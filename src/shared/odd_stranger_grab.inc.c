#include "gameplay/room_effects.h"

/* Part of the Odd Stranger library; see odd_stranger.h. */

/// State 12: clip 5 cross-fade; on entry places the actor 0x3E8 from the player along their XZ offset and sends the player a 0x3E9 transform (position and facing). Each frame pitches coordinates 2 and 3 by -0x80; at the clip-5 boundary spawns the 0x1001 effect and moves to state 0xD.
void oddStrangerGrab(Task* arg0)
{
    SVECTOR          dir;
    OddStrangerWork* work;
    Enemy*           enemy;
    Task*            player;
    SVECTOR*         pdir;
    ActorTransform*  msg;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->stateEntered != 0) {
        player                                  = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
        work->hitBody.radius                    = ODD_STRANGER_BODY_RADIUS;
        work->attackBody.flags                 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
        work->gridBody.flags                   |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags           = 0;
        work->animRequest                       = ODD_STRANGER_ANIM_REQUEST_BLEND;
        work->animRate                          = 0x10;
        work->animId                            = 5;
        player->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(player->extra.tmd->coords);
        msg         = &gOddStrangerGrabTransform.placement;
        msg->pos.vx = player->extra.tmd->coords->coord.t[0];
        msg->pos.vy = player->extra.tmd->coords->coord.t[1];
        msg->pos.vz = player->extra.tmd->coords->coord.t[2];
        pdir        = &dir;
        dir.vx      = (u16)arg0->extra.tmd->coords->coord.t[0] - (u16)player->extra.tmd->coords->coord.t[0];
        dir.vy      = 0;
        dir.vz      = (u16)arg0->extra.tmd->coords->coord.t[2] - (u16)player->extra.tmd->coords->coord.t[2];
        VectorNormalSS(pdir, pdir);
        gte_lddp(0x3E8);
        gte_ldsv(pdir);
        gte_gpf12();
        gte_stsv(pdir);
        arg0->extra.tmd->coords->coord.t[0]   = player->extra.tmd->coords->coord.t[0] + dir.vx;
        arg0->extra.tmd->coords->coord.t[2]   = player->extra.tmd->coords->coord.t[2] + dir.vz;
        arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        msg->rot.vx                           = 0;
        msg->rot.vy                           = ratan2(dir.vx, dir.vz);
        msg->rot.vz                           = 0;
        TASK_MESSAGE_DISPATCH_POINTER(player, 0x3E9, msg, 0);
        Gp_SpawnPadLerp(0xC, 8, 0x8F);
    }
    _oddStrangerDriveAnimation(arg0);
    gfxRotMatrixX(&arg0->extra.tmd->coords[2].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[2]);
    gfxRotMatrixX(&arg0->extra.tmd->coords[3].coord, -0x80, GRAPHICS_ROTATION_COMPOSE);
    arg0->extra.tmd->coords[5].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&arg0->extra.tmd->coords[3]);
    if (work->animId == 5 && (work->rig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY)) {
#if ODD_STRANGER_VARIANT == 2
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
        work->effectArg.coord      = arg0->extra.tmd->coords + ODD_STRANGER_GRAB_FX_PART;
        work->effectArg.spawnArgLo = 0x200;
        work->effectArg.spawnArgHi = 2;
        effectSpawnHit(damageGetPlayerAttackEffectId(0x1001), arg0->extra.tmd->coords + 5, NULL, &work->effectArg);
#if ODD_STRANGER_VARIANT == 1
        work->state = ODD_STRANGER_STATE_GRAB_STRIKE;
#endif
    }
}
