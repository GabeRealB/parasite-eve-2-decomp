#ifndef INCLUDE_ACTORS_ACTOR_800100_H
#define INCLUDE_ACTORS_ACTOR_800100_H

#include "main/task_types.h"

/// Runs the armed companion task's initialization, frame update or teardown.
///
/// Task state must be 0..3. State 0 initializes an actor/model supplied by the
/// spawner; 1 updates it, 2 defers removal and 3 releases attached tasks and
/// collision links. Actor work and model resources must remain live until
/// teardown; the state-3 call kills the task and ends that lifetime.
void actor800100Task(Task* task);

/// Requests for the companion's persistent Pyke emitter in `Task::spawnArg1.value`.
enum {
    ACTOR_800100_PYKE_OFF        = 0,
    ACTOR_800100_PYKE_IDLE       = 1,
    ACTOR_800100_PYKE_FIRE       = 2,
    ACTOR_800100_PYKE_RESET_IDLE = 3,
    ACTOR_800100_PYKE_RESET_OFF  = 4,
    ACTOR_800100_PYKE_RELEASE    = 5,
};

/// Draws the companion's Pyke nozzle, lights it and emits child flames.
///
/// Requires an effect task with owned `EffectWork` in `spawnArg2.pointer`,
/// a coordinate body, initial state 0 and a live borrowed muzzle parent.
/// State 1 consumes `ACTOR_800100_PYKE_*` requests. Idle arms launch speed at
/// 64 game units per running tick; fire raises it by 64 to 384 and emits one
/// flame per tick. Children are reparented to the emitter for teardown.
/// Refreshes transient point-light slot 3 for four frames while running.
/// Hidden companion/effects suspend all processing, including release requests;
/// paused idle redraws the nozzle and paused fire retains its launch speed.
/// Release frees the effect work and kills the task and its coordinate body.
void actor800100PykeEmitterTask(Task* task);

/// Updates one flame emitted by the companion's Pyke attachment.
///
/// Requires a counted effect task with an owned coordinate body and `EffectWork`
/// in `spawnArg2.pointer`, initially age/state zero and work NULL. The low u16
/// of `spawnArg1.value` supplies launch speed in game units per running tick
/// (0x40..0x180) and initial billboard size before a 0x180 bias.
/// Launch follows the coordinate's positive Y axis; a geometry hit starts drift
/// and an enemy contact ends flight. Checks expiry after drawing at age 21 or
/// later, except on the tick entering drift.
/// Owns collision work after initialization and releases the effect on expiry
/// or cancellation. Paused updates refresh the composed coordinate and redraw.
void actor800100PykeFlameTask(Task* task);

#endif // INCLUDE_ACTORS_ACTOR_800100_H
