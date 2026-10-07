#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F0C`:
/// allocate its work block, drop the model onto the floor of the view
/// coordinate and hang the two display nodes off it.
///
/// The model is reparented to `gGfxViewCoord` and its translation replaced by
/// the world position of part 3 of the owning enemy's model, so the body starts
/// where that part is. `fallStep` is a ninth of that height -- the bounce the
/// descent state adds back -- and `aim.travel` the horizontal gap to the player, which
/// the later states spend a fifteenth at a time. The landing cue is enqueued at
/// the model's own pan and depth with the owner's id in its high half, the
/// model is spun to a random yaw, and the two nodes are linked with their
/// collision-record tables before the task's colour and light matrices are
/// pointed into the work block.
void gluttonChunkSpawn(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    Enemy*                 owner;
    Task*                  player;
    SVECTOR                vec;
    s32                    sfx;
    s32                    pan;

    owner  = task->parent->spawnArg2.pointer;
    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);

    if (gGluttonEnded == 1 ||
        (work = memCalloc(sizeof(GluttonProjectileWork), false), task->work = work, work == NULL)) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    vec.vx = vec.vy = vec.vz = 0;
    _actorRenderTransformLocalPointToWorld(&owner->task->extra.tmd->coords[3], &vec);

    task->extra.tmd->coords->coord.t[0]   = vec.vx;
    task->extra.tmd->coords->coord.t[1]   = vec.vy;
    task->extra.tmd->coords->coord.t[2]   = vec.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    work->fallStep = task->extra.tmd->coords->coord.t[1] / 9;
    work->aim.travel.vx =
        player->extra.tmd->coords->coord.t[0] - task->extra.tmd->coords->coord.t[0];
    work->aim.travel.vy = 0;
    work->aim.travel.vz =
        player->extra.tmd->coords->coord.t[2] - task->extra.tmd->coords->coord.t[2];
    work->stateTicks = 0;
    task->state++;

    sfx = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000B;
    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(sfx, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (gRandomLcgState >> 0x10) & 0x1FF, 1);

    vec.vx = vec.vy = vec.vz = 0;

    actorLinkWorkObj(task->extra.tmd->coords, &work->attackBody, work->attackContacts, &vec, 0x100, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                     ARRAY_SIZE(work->attackContacts));

    work->gridBody.coord            = task->extra.tmd->coords;
    work->gridBody.context.contacts = work->gridContacts;
    work->gridBody.pos.vx           = 0;
    work->gridBody.pos.vy           = 0;
    work->gridBody.pos.vz           = 0;
    work->gridBody.key              = 0x3000A;
    work->gridBody.radius           = 0x100;
    work->gridBody.flags            = WORLD_COLLISION_BODY_SPHERE;
    worldCollisionLinkBody(WORLD_COLLISION_LIST_ENEMY_BODIES, &work->gridBody);

    work->attackBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    worldCollisionInitContacts(work->gridBody.context.contacts, ARRAY_SIZE(work->gridContacts), 0);
    work->gridBody.flags |= WORLD_COLLISION_BODY_GRID_ENABLED;
    work->attackBody.key  = damagePackEnemyAttackKey(owner, 5);

    task->extra.tmd->lightMtx = &work->lightMtx;
    task->extra.tmd->colorMtx = &work->colorMtx;
}
