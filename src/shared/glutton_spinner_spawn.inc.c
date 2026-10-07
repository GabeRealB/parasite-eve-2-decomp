#include "main/random.h"

/* Part of the Glutton library; see glutton.h. */

/// Creates a visible floor spinner with its own lighting and chase delay.
///
/// Requires an enemy task with a live TMD model and root coordinate. The low
/// spawn-argument half selects a 20-tick delay for 0, 40 for 1, and 80 otherwise.
/// The model is parented to the view and receives three masked random rotations.
/// The task owns the zeroed work block and its lighting matrices until teardown.
/// Allocation failure or fight end destroys the enemy; success enters waiting.
static void _gluttonSpinnerSpawn(Enemy* enemy, Task* task)
{
    enum { GLUTTON_SPINNER_DELAY_CLASS_SHORT  = 0,
           GLUTTON_SPINNER_DELAY_CLASS_MEDIUM = 1,
           GLUTTON_SPINNER_DELAY_CLASS_LONG   = 2,
           GLUTTON_SPINNER_DELAY_SHORT        = 20,
           GLUTTON_SPINNER_DELAY_MEDIUM       = 40,
           GLUTTON_SPINNER_DELAY_LONG         = 80,
           GLUTTON_SPINNER_ORIENTATION_MASK   = 0x4FF };
    GluttonSpinnerWork* work;

    if (gGluttonEnded == 1) {
        enemyDestroy(enemy, task);
        return;
    }

    work       = memCalloc(sizeof(GluttonSpinnerWork), 0);
    task->work = work;
    if (work == NULL) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->coords->parent = &gGfxViewCoord;
    task->extra.tmd->flags          = 0;

    switch ((u16)task->spawnArg1.value) {
        case GLUTTON_SPINNER_DELAY_CLASS_SHORT:
            work->chaseDelay = GLUTTON_SPINNER_DELAY_SHORT;
            break;
        case GLUTTON_SPINNER_DELAY_CLASS_MEDIUM:
            work->chaseDelay = GLUTTON_SPINNER_DELAY_MEDIUM;
            break;
        case GLUTTON_SPINNER_DELAY_CLASS_LONG:
            work->chaseDelay = GLUTTON_SPINNER_DELAY_LONG;
            break;
        default:
            work->chaseDelay = GLUTTON_SPINNER_DELAY_LONG;
            break;
    }

    task->msgTable   = NULL;
    work->chaseSpeed = 0;
    work->chaseTicks = 0;

    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixY(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
    gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
    gfxRotMatrixX(&task->extra.tmd->coords->coord, (gRandomLcgState >> 16) & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);

    task->extra.tmd->lightMtx             = &work->lightMtx;
    task->extra.tmd->colorMtx             = &work->colorMtx;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCoordSetModelLighting(task->extra.tmd, task->extra.tmd->coords->workm.t, 0, 3);
    task->state++;
}
