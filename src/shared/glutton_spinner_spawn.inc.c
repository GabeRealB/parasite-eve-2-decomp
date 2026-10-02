#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Spawn state of the enemy dispatched through `D_actor_444000_80131F30`:
/// allocate its `GluttonSpinnerWork`, parent the model object to the world
/// coordinate, give it a random orientation off `gRandomLcgState`, point it at its
/// own light and colour matrices and step the task on. Bails to
/// `Gp_DestroyEnemy` when the overlay is shutting down or the allocation fails.
void gluttonSpinnerSpawn(Enemy* enemy, Task* task)
{
    GluttonSpinnerWork* work;

    if (gGluttonEnded == 1) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    work       = memCalloc(0xA0, 0);
    task->work = work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    switch ((u16)task->spawnArg1.value) {
        case 0:
            work->spin = 0x14;
            break;
        case 1:
            work->spin = 0x28;
            break;
        case 2:
            work->spin = 0x50;
            break;
        default:
            work->spin = 0x50;
            break;
    }

    task->msgTable = NULL;
    work->field_98 = 0;
    work->field_96 = 0;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & 0x4FF, 0);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & 0x4FF, GRAPHICS_ROTATION_COMPOSE);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & 0x4FF, GRAPHICS_ROTATION_COMPOSE);

    task->extra.tmd->lightMtx             = &work->lightMtx;
    task->extra.tmd->colorMtx             = &work->colorMtx;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(task->extra.tmd->coords);
    func_800D7A9C(task->extra.tmd, (VECTOR*)task->extra.tmd->coords->workm.t, 0, 3);
    task->state++;
}
