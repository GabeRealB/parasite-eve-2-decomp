#ifndef INCLUDE_ACTORS_ACTOR_535700_H
#define INCLUDE_ACTORS_ACTOR_535700_H

#include "gameplay/evs.h"

/// Aya's encounter with three Pawn GOLEMs on entry to the night Dryfield R08 room.
///
/// Presents the scene actors while hiding the player, then drives their walks
/// and animation cues. A 300-update blackout precedes scene completion; input
/// skipping is disabled for the final 60-frame wait. Use the paired skip script
/// with `EVENT_SCRIPT_HUD_HIDE_RESTORE`. All 99 commands and their actor, CAP,
/// audio and callback resources are borrowed until the interpreter ends.
/// Completion returns to the night motel balcony. Keep this actor overlay and
/// the room's scene resources loaded throughout.
extern EvsCommand gActor535700PawnGolemEncounterScript[99];

/// Skip path that cleans up the Pawn GOLEM encounter and returns to the motel.
///
/// Fades across cleanup, clears the blackout, finishes the scene, waits sixty
/// frames and reveals play. All nine commands and their callbacks are borrowed
/// as the initial skip target of `gActor535700PawnGolemEncounterScript`.
extern EvsCommand gActor535700PawnGolemEncounterSkipScript[9];

#endif // INCLUDE_ACTORS_ACTOR_535700_H
