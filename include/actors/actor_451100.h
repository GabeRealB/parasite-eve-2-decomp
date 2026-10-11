#ifndef ACTORS_ACTOR_451100_H
#define ACTORS_ACTOR_451100_H

#include "gameplay/evs.h"

/// Submarine-tunnel story scene, synchronized with CAP command 2 and scene audio.
///
/// Run with `evsStartScriptWithSkip` and
/// `gActor451100SubmarineTunnelSceneSkipScript` after the room advances its
/// progress from 1 to 2 in variant 3. Requires this actor package, the room's
/// CAP resources, the player and placed scene actors 0 and 1 to remain loaded.
/// Installs borrowed player clips 47..50 and sequences the player and walkers.
/// Completion hides the walkers, returns to view 4 and releases scripted control.
/// The interpreter borrows this stream and its targets until it ends; installed
/// animation data must remain loaded until the player returns to its native bank.
extern EvsCommand gActor451100SubmarineTunnelSceneScript[];

/// Cancellation stream restoring play after the submarine-tunnel story scene.
///
/// Used as the skip target of `gActor451100SubmarineTunnelSceneScript`, with
/// the same loaded-resource requirements. Fades out, aborts CAP presentation,
/// restores view 4 and the player's placement/idle, hides both walkers, then
/// returns the fade and area music before releasing scripted control.
extern EvsCommand gActor451100SubmarineTunnelSceneSkipScript[];

/// Submarine-tunnel arrival event for warp 2 in variant 3 before story progress.
///
/// Starts CAP command 3 and an arrival sound, holds the player in weapon-bank
/// idle through the CAP cue and three further interpreter frames, then releases
/// scripted control. Requires this actor package, the room's CAP/sound resources
/// and the player task to remain loaded until the borrowed stream ends.
extern EvsCommand gActor451100SubmarineTunnelEntryScript[];

#endif // ACTORS_ACTOR_451100_H
