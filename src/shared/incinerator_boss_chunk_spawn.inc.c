#include "main/random.h"

/* Part of the incinerator boss library; see incinerator_boss.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block, drop the model onto the floor of the view
/// coordinate and hang the two display nodes off it.
///
/// The model is reparented to `gGfxViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `field_1AA` is a ninth of that height -- the bounce the
/// descent state adds back -- and `vel` the horizontal gap to the player, which
/// the later states spend a fifteenth at a time. The landing cue is enqueued at
/// the model's own pan and depth with the owner's id in its high half, the
/// model is spun to a random yaw, and the two nodes are linked with their
/// collision-record tables before the task's colour and light matrices are
/// pointed into the work block.
void incinBossChunkSpawn(Enemy* enemy, Task* task)
{
    Actor403200GrabWork* work;
    Enemy*               owner;
    Task*                player;
    SVECTOR              vec;
    s32                  sfx;
    s32                  pan;

    owner  = task->parent->spawnArg2.pointer;
    player = gameGetPtrSlot(3);

    if (gIncinBossEnded == 1 ||
        (work = memCalloc(sizeof(Actor403200GrabWork), false), task->work = work, work == NULL)) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    vec.vx = vec.vy = vec.vz = 0;
    actorLocalToView(&owner->task->extra.tmd->coords[3], &vec);

    task->extra.tmd->coords->coord.t[0]   = vec.vx;
    task->extra.tmd->coords->coord.t[1]   = vec.vy;
    task->extra.tmd->coords->coord.t[2]   = vec.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    work->field_1AA = task->extra.tmd->coords->coord.t[1] / 9;
    work->vel.vx =
        player->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->vel.vy = 0;
    work->vel.vz =
        player->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->field_1AC = 0;
    task->state++;

    sfx = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000B;
    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(sfx, pan, (s8)gpGetObjDepth(task->extra.tmd->coords));

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (gRandomLcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    actorLinkWorkObj(task->extra.tmd->coords, &work->obj0, &work->rec0, &vec, 0x100, 3, 1);

    work->obj1.coord            = task->extra.tmd->coords;
    work->obj1.context.contacts = &work->rec1;
    work->obj1.pos.vx           = 0;
    work->obj1.pos.vy           = 0;
    work->obj1.pos.vz           = 0;
    work->obj1.key              = 0x3000A;
    work->obj1.radius           = 0x100;
    work->obj1.flags            = WORLD_COLLISION_BODY_SPHERE;
    Gp_LinkObj(2, &work->obj1);

    work->obj0.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    Gp_InitRec18Table(work->obj1.context.contacts, 3, 0);
    work->obj1.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->obj0.key    = Gp_PackObjPair(owner, 5);

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;
}
