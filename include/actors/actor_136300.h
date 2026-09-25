#ifndef ACTOR_136300_H
#define ACTOR_136300_H

#include "common.h"

#include "main/task.h"

/// Ramp context of the screen-wave task. The message handler seeds the span
/// and scale and spawns the task with the block as its argument; a later
/// positive message is written into the ramp state, which ends the wave. The
/// task advances the frame and reads the tint.
typedef struct Actor136300WaveCtx {
    s16 field_0; // span: frames the ramp takes to reach full scale
    s16 field_2; // scale: amplitude at the end of the ramp
    s16 field_4; // ramp state: 0 up, 1 down, 2 finished
    s16 field_6; // current ramp frame
    u8  field_8; // nonzero: tint the mesh with field_9..field_B
    u8  field_9;
    u8  field_A;
    u8  field_B;
} Actor136300WaveCtx;

/// Earlier name of the ramp context, still used by a room that carries the
/// same message handler.
typedef Actor136300WaveCtx Actor136300Spawn;

extern Actor136300WaveCtx D_actor_136300_8013C99C;

extern TaskDesc D_actor_136300_80132AC4;

#endif // ACTOR_136300_H
