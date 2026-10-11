#ifndef ACTORS_ACTOR_335800_H
#define ACTORS_ACTOR_335800_H

#include "gameplay/evs.h"

/// Night-balcony entry scene introducing the Burner, Douglas and Flint.
///
/// Requires stage 3/area 29 variant 2, the player, the balcony movie resources
/// and placed actors 0 Burner, 1 Douglas and 2 Flint. Starts stream 58, installs
/// player clips and returns the Burner to combat. Start with
/// `gActor335800BalconyEntrySkipScript` as the skip target. Keep actor_335800,
/// actor_403100 and the room loaded until playback and its borrowed clips end.
extern EvsCommand gActor335800BalconyEntryScript[72];

/// Delayed battle engagement after balcony entry or its skip recovery.
///
/// Waits thirty interpreter updates, engages the scene battle, yields once,
/// then ends player scripting while retaining the root offset. Requires the
/// live player and scene manager; keep this package loaded until END.
extern EvsCommand gActor335800BalconyBattleStartScript[5];

/// Skip recovery that restores the balcony entry scene's combat presentation.
///
/// Cancels streamed playback under a fade, restores the player's pose and
/// weapon animation, hides Douglas/Flint and returns the Burner to combat.
/// Requires the entry scene's loaded actors, room and player resources. Keep
/// this package and its borrowed payloads loaded until END.
extern EvsCommand gActor335800BalconyEntrySkipScript[19];

/// Stream-59 balcony scene for the Burner's timed retreat and departure.
///
/// Requires the live player, Burner at placement 0, night-balcony resources
/// and writable player weapon-bank extension. Calls the common opening and
/// retreat ending, which balance their returns before END. Keep this package,
/// actor_403100, the room and borrowed clip data loaded through playback.
extern EvsCommand gActor335800BurnerRetreatSceneScript[9];

/// Stream-60 balcony scene for the Burner's defeat after combat.
///
/// Requires the retreat script's actor/resource contract and the Burner's
/// loaded sound bank. Stops stage music, calls the common opening and defeat
/// ending, then ends the event. Keep this package and borrowed clips loaded
/// through playback; both subscript calls return before END.
extern EvsCommand gActor335800BurnerDefeatSceneScript[10];

#endif // ACTORS_ACTOR_335800_H
