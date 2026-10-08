#ifndef INCLUDE_WEAPONS_GUNBLADE_H
#define INCLUDE_WEAPONS_GUNBLADE_H

#include "main/task_types.h"

/// Records a Gunblade slash and draws its blue-white then yellow-white ribbon.
///
/// Bank-6 effect 0x186 requires a live coordinate body and owned `EffectWork`
/// in `spawnArg2.pointer`. Its borrowed parent is the weapon muzzle. State 0
/// seeds eight poses per endpoint; state 1 records age modulo eight and draws
/// seven segments. Ages nine through twelve can post one deferred ammunition-
/// graded flash when `index` is one. Age thirteen or cancellation releases
/// the work and task and clears the published work pointer; paused effects
/// retain them without drawing. Histories and published handles are shared
/// by all instances in the loaded overlay. Requires a current view/GTE frame,
/// frame-arena room and initialized scratch storage for transform/draw helpers.
void gunbladeTrailTask(Task* task);

/// Expands and fades the Gunblade's ammunition-graded disc, bands and screen tint.
///
/// Bank-6 effect 0x29A requires a live coordinate body, owned `EffectWork` in
/// `spawnArg2.pointer`, and the Gunblade overlay loaded. `spawnArg1.value`
/// selects Buckshot (13), Firefly (14) or R. Slug (15); other values perform
/// no draw or teardown. `scale`/`period` are disc/band brightness, while twice
/// `angle` is the disc radius and 3/2 of `step` is the band inner radius, in
/// game units. Initialization spawns independent flashes, bands and sparks.
/// Each active tick grows the radii and fades the bands/screen tint, then the
/// disc; final teardown releases the effect work and task. Pause retains them;
/// cancellation releases them. Requires current view/GTE state, frame-arena
/// room and initialized scratch storage for the drawing helpers.
void gunbladeChargeFlashTask(Task* task);

/// Runs the Gunblade shot or spinning slash and its ammunition-powered charge window.
///
/// Primary input starts an 18-tick slash windup and seeds a 57-tick spin
/// counter. Only slash phases consume spin ticks, yawing by 12/4096 turn and
/// advancing along the root's local forward axis divided by 136. The eight-tick contact window accepts secondary input
/// to spend a round and request the ammunition-graded flash. Other entry input
/// fires immediately. Reserves 104 scratch bytes for impact and advance data.
///
/// Requires live player `GameActor` work, its model and initialized animation
/// slots, equipped weapon/contact storage, and the matching weapon overlay
/// loaded throughout dispatch and owned effects. Phase 0 enters normal mode
/// state 4; later calls advance `GameActor::statePhase`. Counts are dispatch
/// ticks. Scratch reservations are released before returning.
void gunbladeAttackState(Task* playerTask);

#endif // INCLUDE_WEAPONS_GUNBLADE_H
