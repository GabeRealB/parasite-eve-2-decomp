#ifndef INCLUDE_WEAPONS_M4A1_PYKE_H
#define INCLUDE_WEAPONS_M4A1_PYKE_H

#include "gameplay/animation.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Native player animation bank for the M4A1 rifle with the Pyke attachment.
///
/// The primary character selects bank index 29 for weapon 28. Base slots
/// 1..44 contain 38 distinct player clips; slots 0, 45 and 46 are NULL.
/// Slots 2/3/4 are forward walk, backward walk and run; the other base slots
/// cover idle/turn, aim, attacks, reload, hit/death, Parasite Energy, scripted
/// presentation, stairs and item use. Slots 20, 21/22/41 and 34/35 alias
/// primary reload, idle and the paired presentation clips respectively.
/// Slot 15 serves flame release and the secondary-reload request.
///
/// The 32 extension words (47..78) start cleared and remain writable for
/// scripted copies, including payloads that are not clip pointers. A copy
/// replaces only its requested prefix; later words retain their previous values.
/// Play extension slots only when they hold loaded clips. Keep this weapon
/// package and all borrowed clip data loaded while the bank or playback retains
/// them; replacing the package invalidates the bank and its extension.
extern AnimationBank gM4a1PykePlayerAnimationBank;

/// Equipped M4A1 rifle with the Pyke flamethrower attachment.
///
/// A single model part is attached beneath the player's weapon-hand task.
/// Model instances borrow this overlay's geometry and stream; keep the package
/// loaded until they are released. Creation resolves stream handlers in place.
extern TmdSource gM4a1PykeWeaponModel;

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
