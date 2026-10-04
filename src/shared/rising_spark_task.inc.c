/* Part of the rising spark library; see rising_spark.h. */

/// State 0 seeds the spark: a rise of 4 per frame, a random spin in `scale`,
/// the spawn argument's low twelve bits as its angle and full brightness in
/// `period`. State 1 lifts the coordinate, steps the frame every other tick
/// and on the odd ones draws `Gp_DrawFxQuad`; past frame 7 it releases the
/// effect.
static inline void risingSparkTask(Task* task)
{
    EffectWork* mem;
    GfxCoord*   coord;
    s32         y;
    s32         state;
    s16         step;
    u16         spawn;

    mem      = task->spawnArg2.pointer;
    coord    = task->extra.coordBody->coord;
    mem->age = mem->age + 1;
    state    = task->state;
    switch (state) {
        case 0:
            mem->move.vy    = 4;
            mem->move.vx    = 0;
            mem->move.vz    = 0;
            task->state     = 1;
            gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            mem->scale      = (gRandomLcgState >> 16) & 0xFFF;
            spawn           = (u16)task->spawnArg1.value;
            mem->period     = 0x1000;
            mem->angle      = spawn & 0xFFF;
            return;
        case 1:
            step                = mem->move.vy;
            y                   = coord->coord.t[1] + step;
            coord->composeStamp = GRAPHICS_COORD_DIRTY;
            coord->coord.t[1]   = y;
            actorRenderComposeCoord(coord);
            if (!(mem->age & 1)) {
                mem->index = mem->index + 1;
            }
            if (mem->index < 8) {
                if (mem->age & 1) {
                    Gp_DrawFxQuad(coord, mem->index, mem->angle,
                                  mem->scale | mem->period);
                    return;
                }
            } else {
                effectKillTask(mem, task);
                return;
            }
            break;
    }
}
