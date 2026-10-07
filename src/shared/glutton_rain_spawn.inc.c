#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `gGluttonRainGroup`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->aim.landing` is then the host model's position
/// pushed out by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on,
/// measured against the player -- plus the `[group][spawnArg1]` entry of
/// `gGluttonRainPoints`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `gGluttonRainLaunchOffsets` offset, `bodyCoord` is parented to
/// `gGfxViewCoord` with an identity rotation and carries `attackBody`, linked
/// with pair tests off, and the spawn cue is enqueued at the model's own pan
/// and depth with the owner's id in its high half. `rainEffect` is spawned on
/// the model's coordinate and made this task's child so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void gluttonRainSpawn(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;
    Enemy*                 owner;
    Task*                  parent;
    Task*                  player;
    SVECTOR                vec;
    s32                    dist;
    s32                    rnd;
    s32                    snd;
    s32                    pan;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    owner  = task->parent->spawnArg2.pointer;
    parent = task->parent;

    if (gGluttonEnded == 1) {
        enemyDestroy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(GluttonProjectileWork), false);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work->fallStep                  = 0;

    _gluttonGetPlayerOffset(task->extra.tmd->coords, &vec);

    if ((u16)task->spawnArg1.value == 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        rnd             = (gRandomLcgState >> 16) & 3;
        switch (rnd) {
            case 0:
            case 1:
                gGluttonRainGroup = 1;
                break;
            case 2:
                gGluttonRainGroup = rnd;
                break;
            case 3:
                gGluttonRainGroup = 0;
                break;
            default:
                gGluttonRainGroup = 0;
                break;
        }
    }

    _gluttonGetPlayerOffset(parent->extra.tmd->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->aim.landing.vx = parent->extra.tmd->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->aim.landing.vx = parent->extra.tmd->coords->coord.t[0] + 0x2710;
    } else {
        work->aim.landing.vx = parent->extra.tmd->coords->coord.t[0] + 0x32C8;
    }

    work->aim.landing.vx +=
        gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                 [(u16)task->spawnArg1.value]]
            .vz;
    work->aim.landing.vy = 0;
    gRandomLcgState      = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->aim.landing.vz = gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                                    [(u16)task->spawnArg1.value]]
                               .vx -
                           0x189C;
    work->aim.landing.vz = ((gRandomLcgState >> 16) & 0x7F) + work->aim.landing.vz;

    if ((u16)task->spawnArg1.value == 4) {
        work->aim.landing.vx = player->extra.tmd->coords->coord.t[0];
        work->aim.landing.vy = 0;
        work->aim.landing.vz = player->extra.tmd->coords->coord.t[2];
    }

    work->stateTicks  = 0;
    gRandomLcgState   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->speedJitter = (gRandomLcgState >> 16) & 8;

    vec.vx = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vx;
    vec.vy = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vy;
    vec.vz = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vz;

    task->extra.tmd->coords->coord.t[0] = vec.vx + parent->extra.tmd->coords->coord.t[0];
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz + parent->extra.tmd->coords->coord.t[2];

    work->attackBody.key = damagePackEnemyAttackKey(owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    // The attack body rides a node of its own, which the later states keep on
    // the blob.
    work->bodyCoord.parent = &gGfxViewCoord;
    gfxSetRotIdentity(&work->bodyCoord.coord);
    gfxRotMatrixY(&work->bodyCoord.coord, 0, 1);

    _worldCollisionLinkSphereBody(&work->bodyCoord, &work->attackBody, work->attackContacts, &vec, 0x100, WORLD_COLLISION_LIST_ENEMY_ATTACKS,
                                  ARRAY_SIZE(work->attackContacts));
    work->attackBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    snd = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000B;
    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(snd, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));

    work->rainEffect = effectSpawn(EFFECT_GLUTTON_RAIN_BLOB, task->extra.tmd->coords, 0, NULL);
    if (work->rainEffect != NULL) {
        taskReparent(task, work->rainEffect->task);
    }
    task->state++;
}
