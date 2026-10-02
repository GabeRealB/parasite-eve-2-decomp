#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F1C`:
/// allocate its work block and pick the point it will be dropped on.
///
/// A spawn with `spawnArg1` 0 rerolls the drop-point group in
/// `gGluttonRainGroup`, mapping the two low bits of the LCG onto group
/// 1, 1, 2 and 0. `work->target` is then the host model's position pushed out
/// by 0x1B58, 0x2710 or 0x32C8 -- whichever ring the host is on, measured
/// against the player -- plus the `[group][spawnArg1]` entry of
/// `gGluttonRainPoints`, with a 0..0x7F jitter on z. `spawnArg1` 4 drops
/// on the player instead. The model itself is stood up beside the host at the
/// `gGluttonRainLaunchOffsets` offset, its work coordinate is parented to
/// `gGfxViewCoord` with an identity rotation and carries the single display
/// node, and the spawn cue is enqueued at the model's own pan and depth with
/// the owner's id in its high half. The trailing `Gp_SpawnEff` effect becomes
/// this task's parent so it dies with it.
///
/// Bails out -- destroying the enemy -- when the overlay is shutting down or
/// the work block cannot be allocated.
void gluttonRainSpawn(Enemy* enemy, Task* task)
{
    GluttonDropWork* work;
    Enemy*           owner;
    Task*            parent;
    Task*            player;
    GfxMatrix*       mtx;
    SVECTOR          vec;
    s32              dist;
    s32              rnd;
    s32              snd;
    s32              pan;

    player = gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    owner  = task->parent->spawnArg2.pointer;
    parent = task->parent;

    if (gGluttonEnded == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    work       = memCalloc(sizeof(GluttonDropWork), false);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    work->field_1AA                 = 0;

    gluttonGapToCamera(task->extra.tmd->coords, &vec);

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

    gluttonGapToCamera(parent->extra.tmd->coords, &vec);
    dist  = vec.vx * vec.vx;
    dist += vec.vy * vec.vy;
    dist += vec.vz * vec.vz;
    dist  = SquareRoot0(dist);

    if (dist < 0x1F40) {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x1B58;
    } else if (dist < 0x2AF8) {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x2710;
    } else {
        work->target.vx = parent->extra.tmd->coords->coord.t[0] + 0x32C8;
    }

    work->target.vx +=
        gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                 [(u16)task->spawnArg1.value]]
            .vz;
    work->target.vy = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->target.vz = gGluttonRainPoints[gGluttonRainPointIndex[gGluttonRainGroup]
                                                               [(u16)task->spawnArg1.value]]
                          .vx -
                      0x189C;
    work->target.vz = ((gRandomLcgState >> 16) & 0x7F) + work->target.vz;

    if ((u16)task->spawnArg1.value == 4) {
        work->target.vx = player->extra.tmd->coords->coord.t[0];
        work->target.vy = 0;
        work->target.vz = player->extra.tmd->coords->coord.t[2];
    }

    work->timer     = 0;
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    work->field_1AE = (gRandomLcgState >> 16) & 8;

    vec.vx = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vx;
    vec.vy = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vy;
    vec.vz = gGluttonRainLaunchOffsets[(u16)task->spawnArg1.value].vz;

    task->extra.tmd->coords->coord.t[0] = vec.vx + parent->extra.tmd->coords->coord.t[0];
    task->extra.tmd->coords->coord.t[1] = vec.vy;
    task->extra.tmd->coords->coord.t[2] = vec.vz + parent->extra.tmd->coords->coord.t[2];

    work->obj.key = Gp_PackObjPair(owner, 1);

    vec.vx = 0;
    vec.vy = 0;
    vec.vz = 0;

    work->coord.parent        = &gGfxViewCoord;
    mtx                       = (GfxMatrix*)&work->coord.coord;
    mtx->rotationWords.m00M01 = ONE;
    mtx->rotationWords.m02M10 = 0;
    mtx->rotationWords.m11M12 = ONE;
    mtx->rotationWords.m20M21 = 0;
    mtx->rotationWords.m22    = ONE;
    gfxRotMatrixY(&mtx->mat, 0, 1);

    actorLinkWorkObj(&work->coord, &work->obj, &work->rec, &vec, 0x100, 3, 1);
    work->obj.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);

    snd = ((owner->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x4020000B;
    pan = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));

    work->eff = Gp_SpawnEff(EFFECT_GLUTTON_RAIN_BLOB, task->extra.tmd->coords, 0, NULL);
    if (work->eff != NULL) {
        taskReparent(task, work->eff->task);
    }
    task->state++;
}
