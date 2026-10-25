#ifndef ACTORS_ACTOR_142600_H
#define ACTORS_ACTOR_142600_H

#include "gameplay/evs.h"

#include "main/task_types.h"

/// Unused bodyless descriptor retained by the incinerator arrival scene package.
///
/// The control room loads the package through this one-entry resource table,
/// with no terminator. Its empty placement list spawns nothing. Entry 0 has
/// priority `TASK_PRIORITY_CONTROL` and retains a two-argument enemy destructor address, which is
/// incompatible with task dispatch; it must not be spawned.
extern TaskDesc gActor142600TaskDescriptors[1];

/// CAP-synchronized player/companion scene after arrival from the incinerator.
///
/// The control room starts this 76-command stream on arrival warp 4 with
/// `gActor142600IncineratorArrivalSkipScript` as its input skip target. It
/// starts CAP command 2, installs the package's scene clips, and restores both
/// actors at the view-5 exit placements before releasing scripted control.
/// Keep actor_142600, both receiver banks and the room's CAP resources loaded
/// until either stream ends and no installed scene clip remains in playback.
extern EvsCommand gActor142600IncineratorArrivalScript[76];

/// Fifteen-command skip path that restores the incinerator arrival scene's exit state.
///
/// Fades through scene cleanup, shows the companion, restores view 5 and both
/// placements, then releases scripted control. Used only as the skip target of
/// `gActor142600IncineratorArrivalScript`, with the same resource lifetime.
extern EvsCommand gActor142600IncineratorArrivalSkipScript[15];

#endif // ACTORS_ACTOR_142600_H
