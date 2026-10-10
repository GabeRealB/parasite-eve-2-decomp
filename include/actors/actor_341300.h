#ifndef ACTORS_ACTOR_341300_H
#define ACTORS_ACTOR_341300_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Scene task recipes: 0 textured quads, 1 player facing, 2 placed-resource exit.
///
/// All run at priority 192. Only the quads own a coordinate body; the exit
/// task releases the Enemy passed in spawnArg2 by the walkway resource loader.
/// Keep actor_341300 loaded while any callback runs. Indices are unchecked.
extern TaskDesc gActor341300ScriptTaskDescriptors[3];

/// Normal event script for the default-variant B2 north maintenance walkway scene.
///
/// Coordinates the player, placed Brain Stinger, CAP cues, scene stream and
/// temporary effects, then restores control, view and area music. The actor
/// package, placed actor and equipped player animation bank must stay loaded
/// through the event; appended animation sets remain borrowed during playback.
/// Start with `gActor341300WalkwaySceneSkipScript` as the alternate skip path.
extern EvsCommand gActor341300WalkwaySceneScript[52];

/// Skip path placing the player at the walkway scene's end and releasing its effects.
///
/// Used with `gActor341300WalkwaySceneScript`; requires the same loaded resources
/// and live message recipients. Cancels streaming and restores view 4, music
/// and scene event 12 under a fade. The saved temporary-task handle must be
/// NULL or still live when cleanup requests its exit.
extern EvsCommand gActor341300WalkwaySceneSkipScript[21];

/// Clears the scene's saved temporary-task handle without stopping its task.
///
/// The walkway calls this after loading actor_341300 for its scene variant.
/// Player-facing and textured-quad tasks share the slot; this reset neither
/// validates nor releases a previously saved task. The actor overlay must be loaded.
void actor341300ResetScriptTaskHandle(void);

#endif // ACTORS_ACTOR_341300_H
