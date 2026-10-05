#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Room-effect task drawing a growing, fading quad through
/// `_waterDrawSplash`. The first frame sets the brightness to
/// 0x40, takes the size from the spawn parameter's low 12 bits and turns the
/// coordinate to a random Y rotation. Every frame then grows the size by
/// 0x20, draws, and dims by 2, releasing the effect once the brightness falls
/// under 2. The coordinate is never rebuilt, so it keeps the frame the spawner
/// left. Once `gRoomEffectState->effectControl` leaves running it only draws,
/// and releases at cancellation.
void waterRippleTaskFixedCoord(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        _waterDrawSplash(coord, work->angle, work->scale);
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
        }
    } else {
        work->age++;
        if (task->state == 0) {
            work->scale     = 0x40;
            work->angle     = task->spawnArg1.halves.low & 0xFFF;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gfxRotMatrixY(&coord->coord, (gRandomLcgState >> 16) & 0xFFF, 1);
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            task->state         = 1;
        }
        work->angle += 0x20;
        _waterDrawSplash(coord, work->angle, work->scale);
        work->scale -= 2;
        if (work->scale < 2) {
            effectKillTask(work, task);
        }
    }
}
