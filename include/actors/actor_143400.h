#ifndef INCLUDE_ACTORS_ACTOR_143400_H
#define INCLUDE_ACTORS_ACTOR_143400_H

#include "gameplay/evs.h"

/// Companion-entry scene for Shelter R47, including its END command.
///
/// Installs this package's clips in the player and companion bank extensions,
/// then coordinates their placement and playback with CAP command 3. Completion
/// restores actor control, captions, ambient lighting and the saved view, and
/// applies the room's saved-area updates. Requires both actors and their banks,
/// this package's animation/CAP resources and the R47 overlay to remain loaded
/// through playback. Start with `gActor143400CompanionEntrySkipScript` as the
/// skip target.
extern EvsCommand gActor143400CompanionEntryScript[97];

/// Skip path that commits the companion-entry scene's final placement and state.
///
/// Twenty commands, including END, show and place both actors, release scripted
/// control, restore the view/music, set scene event 15 and apply the saved-area
/// updates before fading back in. Does not install or play the scene's clips.
/// Keep this package and the R47 overlay loaded until the interpreter finishes.
extern EvsCommand gActor143400CompanionEntrySkipScript[20];

#endif // INCLUDE_ACTORS_ACTOR_143400_H
