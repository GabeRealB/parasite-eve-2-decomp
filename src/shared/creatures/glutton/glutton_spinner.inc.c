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

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED) {
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

    RANDOM_LCG_ADVANCE();
    gfxRotMatrixY(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW() & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
    RANDOM_LCG_ADVANCE();
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW() & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);
    RANDOM_LCG_ADVANCE();
    gfxRotMatrixX(&task->extra.tmd->coords->coord, RANDOM_LCG_DRAW() & GLUTTON_SPINNER_ORIENTATION_MASK, GRAPHICS_ROTATION_COMPOSE);

    task->extra.tmd->lightMtx             = &work->lightMtx;
    task->extra.tmd->colorMtx             = &work->colorMtx;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCoordSetModelLighting(task->extra.tmd, task->extra.tmd->coords->workm.t, 0, 3);
    task->state++;
}

/// Tests whether a spinner's XZ target offset meets its pre-acceleration chase radius.
///
/// Ignores Y; offset and signed-halfword chaseSpeed share game-coordinate units.
/// Returns 1 at or beyond the squared radius, including equality and zero radius.
/// Requires live inputs, twelve free word-aligned scratch bytes and an XZ
/// squared sum representable as s32. Restores the exact cursor snapshot before
/// its final read, with no intervening reservation or call that can reuse it.
static inline s32 _gluttonSpinnerOutsideArrivalRadius(const SVECTOR* offset, const GluttonSpinnerWork* work)
{
    OverlayRangeScratch* rangeSquares;
    OverlayRangeScratch* scratchEnd;
    s16                  speed;

    scratchEnd   = SCRATCH_STACK_CURSOR(OverlayRangeScratch);
    rangeSquares = scratchEnd - 1;
    _scratchStackSetCursor(rangeSquares);
    speed                 = work->chaseSpeed;
    rangeSquares->dx      = offset->vx;
    rangeSquares->dz      = offset->vz;
    rangeSquares->radius  = speed;
    rangeSquares->dx     *= rangeSquares->dx;
    rangeSquares->dz     *= rangeSquares->dz;
    rangeSquares->radius *= rangeSquares->radius;
    _scratchStackSetCursor(scratchEnd);
    // No reservation can intervene between restore and the final read.
    return rangeSquares->dx + rangeSquares->dz >= rangeSquares->radius;
}

/// Delays a released Glutton spinner, then accelerates it toward its target.
///
/// Requires initialized spinner work, a live model with writable lighting
/// matrices and a target in the root's parent frame. Offsets narrow to s16;
/// distance and speed use game units, angles use 4096 units per turn. Needs a
/// free word-aligned twelve-byte scratch block and initialized GTE state.
/// Advances only when XZ distance is strictly below the pre-acceleration speed;
/// equality keeps chasing, and the arrival tick still accelerates and moves.
/// Encounter shutdown or a cleared release flag destroys the spinner.
static void _gluttonSpinnerChase(Enemy* enemy, Task* task)
{
    GluttonSpinnerWork* work;
    SVECTOR             targetOffset;
    SVECTOR*            movement;
    s32                 delay;
    s32                 phase;
    s32                 outsideArrivalRadius;

    work                                  = task->work;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(task->extra.tmd->coords);
    worldCoordSetModelLighting(task->extra.tmd, task->extra.tmd->coords->workm.t, 0, 3);

    if (_gGluttonEncounterEnded == GLUTTON_ENCOUNTER_ENDED || _gGluttonSpinnersReleased == GLUTTON_SPINNER_RELEASE_OFF) {
        enemyDestroy(enemy, task);
        return;
    }

    task->extra.tmd->flags = 0;

    delay = work->chaseDelay;
    if (delay != 0) {
        delay--;
        work->chaseDelay = delay;
        phase            = work->chaseDelay;
        if ((phase & 3) == 1) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, 0x40, GRAPHICS_ROTATION_COMPOSE);
        }
        if ((work->chaseDelay & 3) == 3) {
            gfxRotMatrixY(&task->extra.tmd->coords->coord, -0x3C, GRAPHICS_ROTATION_COMPOSE);
        }
        task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(task->extra.tmd->coords);
        return;
    }

    movement         = &targetOffset;
    *movement        = _gGluttonSpinnerTarget;
    targetOffset.vx -= task->extra.tmd->coords->coord.t[0];
    targetOffset.vy -= task->extra.tmd->coords->coord.t[1];
    targetOffset.vz -= task->extra.tmd->coords->coord.t[2];

    outsideArrivalRadius = _gluttonSpinnerOutsideArrivalRadius(movement, work);
    if (!outsideArrivalRadius) {
        task->state++;
    }

    // Arrival still completes this tick's acceleration, displacement and spin.
    work->chaseTicks++;
    work->chaseSpeed += work->chaseTicks / 8;
    VectorNormalSS(movement, movement);

    gte_lddp((u16)work->chaseSpeed);
    gte_ldsv(movement);
    gte_gpf12();
    gte_stsv(movement);

    task->extra.tmd->coords->coord.t[0]  += targetOffset.vx;
    task->extra.tmd->coords->coord.t[1]  += targetOffset.vy;
    task->extra.tmd->coords->coord.t[2]  += targetOffset.vz;
    task->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;

    gfxRotMatrixY(&task->extra.tmd->coords->coord, work->chaseSpeed / 2, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixZ(&task->extra.tmd->coords->coord, work->chaseSpeed * 2, GRAPHICS_ROTATION_COMPOSE);
    gfxRotMatrixX(&task->extra.tmd->coords->coord, work->chaseTicks, GRAPHICS_ROTATION_COMPOSE);
}
