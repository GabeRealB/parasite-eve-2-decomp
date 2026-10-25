#ifndef INCLUDE_ACTORS_ACTOR_461800_H
#define INCLUDE_ACTORS_ACTOR_461800_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Neo Ark room-31 scene with the player, Kyle walker and Aya walker.
///
/// The 52 commands include END. The script selects scene/audio key 6/18/11,
/// starts CAP command 1, installs the player's extension clip and sequences
/// model visibility, animation, fade and framebuffer shift at CAP cues.
/// Outside attract demo 9, completion awards progress-dependent rescue bonuses
/// and requests the ending or Shelter R36 transition. Start with HUD
/// hide/restoration and the skip script below. Actor, room, CAP and borrowed
/// clip resources must remain loaded
/// throughout interpretation; singleton scene/walker state permits one instance.
extern EvsCommand gActor461800NeoArkSceneScript[52];

/// Skip path for the Neo Ark room-31 scene, including its END command.
///
/// Eight commands fade out, clean up scene playback and request the same
/// completion path. A million-update wait lets the ending or room load take
/// over before the retained fade-return commands. It borrows the same loaded
/// actor/room resources as the ordinary script.
extern EvsCommand gActor461800NeoArkSceneSkipScript[8];

/// Kyle scripted-walker recipes: body at index 0, left/right hand models at 1/2.
///
/// Three TMD entries run at priority `TASK_PRIORITY_CONTROL` with automatic primitive buffers and
/// no terminator. Spawn entry 0 with its live Enemy in spawn argument 2.
/// It spawns entries 1/2 with body-coordinate indices 8/12 in spawn argument 1.
/// Attachments borrow the twenty-part body until its teardown kills them.
/// Singleton handlers require one live walker; keep this package's models,
/// clips and callbacks loaded while any of its tasks use them.
extern TaskDesc gActor461800ScriptedWalkerTasks[3];

/// Aya footstep-walker TMD recipe for scene placement 132, at priority 192.
///
/// Spawn at index 0 with its live Enemy in spawn argument 2. The nineteen-part
/// model receives automatic primitive buffers. Its message/animation handlers
/// use a published singleton work block, so keep one instance and retain this
/// package's model, clips and callbacks throughout the task's lifetime.
extern TaskDesc gActor461800FootstepWalkerTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_461800_H
