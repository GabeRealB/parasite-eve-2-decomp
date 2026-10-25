#ifndef INCLUDE_ACTORS_ACTOR_560800_H
#define INCLUDE_ACTORS_ACTOR_560800_H

#include "types.h"

#include "main/task_types.h"

/// Descriptor index used by the gantry to start the Aya/Eve/Kyle/No. 9 scene.
enum { ACTOR_560800_TASK_CUTSCENE = 0 };

/// Cast and presentation task recipes for the pod service gantry cutscene.
///
/// Fourteen live entries, without a terminator, at priority `TASK_PRIORITY_CONTROL`: 0 controller,
/// 1 discard task, 2 reveal fade, 3 darkening fade, 4 masked Eve body, 5 Kyle
/// body, 6 No. 9 body, 7 Kyle gun hand, 8 Kyle free hand, 9 handgun,
/// 10 No. 9 gunblade, 11 replacement Eve body, 12 Eve replacement/display
/// handoff, 13 playback/display wait. Entry 0 takes unused spawn arguments.
/// Fades use argument 1's low unsigned halfword as intensity units per update.
/// Body 11 uses argument 1 = 1 to place and relight the replacement; ordinary
/// bodies start with 0. Attachments borrow their live body in argument 2 and
/// select free hand (0), gun hand (1), gun (2) or gunblade (3) in argument 1.
/// Models skip automatic primitive buffers. Bodies, attachments, fades and
/// props join the controller's teardown tree; display-handoff tasks end
/// independently. Keep this package and its models, animation, CAP and scene
/// resources loaded through playback and teardown of every spawned task.
extern TaskDesc gActor560800CutsceneTaskDescriptors[14];

/// Movie launcher index used by the gantry after the scripted scene ends.
enum { ACTOR_560800_MOVIE_TASK_LAUNCH = 0 };

/// Task recipes for handing display control to movie playback and restoring play.
///
/// Two entries without a terminator, both with no body at priority `TASK_PRIORITY_CONTROL`:
/// 0 launches the display task and immediately dies; 1 owns playback,
/// cancellation and game-resource restoration. Spawn argument 1 selects
/// movie 100 when zero, 101 otherwise, in the current location; argument 2
/// is unused. Keep this actor package loaded until playback and restoration
/// finish. The launcher's returned task pointer cannot be polled after its
/// first update.
extern TaskDesc gActor560800MovieTaskDescriptors[2];

/// Carrier update counter supplying the gantry chains' alternating glow phase.
///
/// Starts at 1 and advances once per carrier update after initialization,
/// including hidden updates. Placing chains under the carrier resets it to 1.
/// The glow drawer adds its per-chain phase byte and tests parity. Storage and
/// arithmetic remain signed 32-bit; the room may read it only while this actor
/// package is loaded. It counts carrier updates rather than display frames.
extern s32 gActor560800ChainGlowTick;

#endif // INCLUDE_ACTORS_ACTOR_560800_H
