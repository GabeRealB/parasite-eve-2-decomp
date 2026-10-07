#ifndef INCLUDE_WEAPONS_M4A1_PYKE_H
#define INCLUDE_WEAPONS_M4A1_PYKE_H

#include "main/task_types.h"

/// Mutable nozzle requests in `Task::spawnArg1.value`.
enum {
    M4A1_PYKE_NOZZLE_OFF        = 0,
    M4A1_PYKE_NOZZLE_IDLE       = 1,
    M4A1_PYKE_NOZZLE_FIRE       = 2,
    M4A1_PYKE_NOZZLE_RESET_IDLE = 3,
    M4A1_PYKE_NOZZLE_RESET_OFF  = 4,
    M4A1_PYKE_NOZZLE_RELEASE    = 5,
};

/// Runs the Pyke's persistent nozzle glow, flame emission and transient point light.
///
/// Bank-6 effect 0x2A owns a coordinate body and `EffectWork` in `spawnArg2.pointer`;
/// its borrowed muzzle parent must remain live. Requests use `M4A1_PYKE_NOZZLE_*`.
/// Idle draws the nozzle; fire emits a child flame each running tick, increasing
/// launch speed by 64 to a maximum of 384 game units per tick. Reset requests
/// select idle/off on the next dispatch; release ends the task and owned work.
/// A hidden player model or hidden room effects suspend all dispatch. Other
/// pauses redraw the idle nozzle without aging, but suspend flame emission.
/// Point-light refreshes replace transient slot 1 for four ticks. Keep the Pyke
/// overlay, gameplay and the parent alive through effect dispatch and teardown.
void m4a1PykeNozzleTask(Task* task);

/// Updates one flying flame from the M4A1 Pyke flamethrower attachment.
///
/// Callback for `EFFECT_M4A1_PYKE_FLAME`, spawned through `effectSpawn` with
/// a launch coordinate. Requires a live counted task with an owned coordinate
/// body and an owned `EffectWork` in `spawnArg2.pointer`; state and age start
/// at zero, and `work` starts NULL. The unsigned low halfword of
/// `spawnArg1.value` supplies launch speed in game units per running tick
/// (0x40..0x180), reduced by 0..63 units of jitter along the rotated local
/// positive Y axis. It also seeds the billboard's size numerator, plus 0x180.
///
/// Flight takes enemy-body contacts; a room-geometry hit starts noncolliding
/// spreading drift. Age 21 ends the effect after drawing, except on the tick
/// entering drift. Paused effects redraw without aging. Cancellation or an
/// enemy-body contact also ends the effect. Initialization owns a separate
/// collision block in `work`; teardown releases both work blocks and the
/// coordinate body. Keep this weapon overlay and gameplay loaded until release.
void m4a1PykeFlameTask(Task* task);

/// Runs the M4A1 Pyke player attack, choosing rifle bursts or sustained flame.
///
/// Primary input fires up to three rounds at four-tick intervals. Secondary
/// input drives the persistent nozzle and consumes another fuel load after
/// each twenty-tick countdown while secondary input remains held.
/// Stops flame emission before returning to attack recovery.
///
/// Requires live player `GameActor` work, its model and initialized native
/// animation slots, equipped weapon/contact storage and the matching weapon
/// overlay loaded throughout dispatch and owned effects. Phase 0 enters
/// normal mode state 4; later calls advance `GameActor::statePhase`. Frame counts
/// are dispatch ticks. Releases its scratch reservation before returning.
/// Reserves 80 scratch bytes for a temporary impact node, in addition to
/// the called helpers' reservations.
void m4a1PykeAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_M4A1_PYKE_H
