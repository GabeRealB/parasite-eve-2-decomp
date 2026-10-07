#ifndef INCLUDE_WEAPONS_M4A1_PYKE_H
#define INCLUDE_WEAPONS_M4A1_PYKE_H

#include "main/task_types.h"

void func_m4a1_pyke_8011D1F8(Task* task);

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

#endif // INCLUDE_WEAPONS_M4A1_PYKE_H
