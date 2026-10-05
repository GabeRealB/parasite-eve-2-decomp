#include "main/random.h"

/* Part of the water effects library; see water_effects.h. */

/// Per-frame driver of an expanding, fading flash effect. While the room's
/// event state is 0 it updates the task's coordinate, ticks the age counter
/// `age` and draws the flash through
/// `_waterDrawSplash` at size `angle` and brightness
/// `scale`. The first frame sets the brightness to 0x40, takes the size from
/// the spawn argument's low 12 bits and turns the coordinate about Y by a
/// random angle; every frame then grows the size by 0x20 and dims the
/// brightness by 2, releasing the work block once it falls under 2. Once the
/// event state is non-zero it only draws, releasing the block from event state
/// 4 on.
static inline void waterRippleTask(Task* task)
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
        actorRenderComposeCoord(coord);
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
