/* Part of the effect sprite library; see effect_sprite.h. */

/// Per-frame driver of a rising sprite effect, a room-effect task.
///
/// On its first frame it seeds the rise speed (`move.vy`, 0x10-0x4F from
/// the LCG, negated when bit 16 of `Task::spawnArg1` is set), a random spin
/// angle (`scale`) and the sprite radius (`angle`, the low 12 bits of
/// `Task::spawnArg1`). While no event runs, every frame moves the coordinate
/// frame along Y by the speed and advances the animation frame (`index`)
/// every fourth tick, releasing the effect after frame 7. During an event of
/// state 1-3 it keeps drawing without moving or animating; state 4 or above
/// releases it. Each draw picks one of six sprite CLUTs at random.
void effectSpriteRiseTask(Task* task)
{
    EffectWork* work;
    GfxCoord*   coord;

    work  = task->spawnArg2.pointer;
    coord = task->extra.coordBody->coord;
    if (gRoomEffectState->effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
        if (gRoomEffectState->effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
            effectKillTask(work, task);
            return;
        }
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectDrawSpinningBillboard(coord, work->index, work->angle, work->scale | (((gRandomLcgState >> 16) % 6) << 12));
        return;
    }
    work->age++;
    if (task->state == 0) {
        work->move.vx   = 0;
        work->move.vz   = 0;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->move.vy   = ((gRandomLcgState >> 16) & 0x3F) + 0x10;
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        work->scale     = (gRandomLcgState >> 16) & 0xFFF;
        work->angle     = task->spawnArg1.value & 0xFFF;
        if (task->spawnArg1.value & 0x10000) {
            work->move.vy = -work->move.vy;
        }
        task->state = 1;
    }
    coord->coord.t[1]  += work->move.vy;
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    if (!(work->age & 3)) {
        work->index++;
    }
    if (work->index < 8) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        effectDrawSpinningBillboard(coord, work->index, work->angle, work->scale | (((gRandomLcgState >> 16) % 6) << 12));
        return;
    }
    effectKillTask(work, task);
}
