#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Area placement entries the plaza's two police officers are spawned from: the rookie at the tower's entrance and the
/// sergeant at the barricade. The same IDs select each one's placement record in the plaza's nested layout.
enum {
    POLICE_OFFICERS_PLACEMENT_ROOKIE   = 0x6C,
    POLICE_OFFICERS_PLACEMENT_SERGEANT = 0x6D,
};

/// Messages an officer answers besides the common actor messages.
enum {
    POLICE_OFFICERS_MESSAGE_STOP_CONTROLLER = 2002,
    POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY = 2007, // Argument: `POLICE_OFFICERS_CULLED_BODY_*`
};

/// What an `ACTOR_MESSAGE_PLAY_ANIMATION` request's `source.index` selects: 0 plays a clip of the paired player's bank;
/// any other plays one of the receiving officer's own bank and chains through that follow-up table - the rookie's or
/// the sergeant's, whoever receives it (the sergeant's later talks send the rookie's).
enum {
    POLICE_OFFICERS_ANIMATION_PLAYER   = 0,
    POLICE_OFFICERS_FOLLOW_UP_ROOKIE   = 1,
    POLICE_OFFICERS_FOLLOW_UP_SERGEANT = 2,
};

/// What `POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY` does: swap in the sergeant's or the rookie's culled body, or freeze the
/// current model.
enum {
    POLICE_OFFICERS_CULLED_BODY_SERGEANT = 0,
    POLICE_OFFICERS_CULLED_BODY_ROOKIE   = 1,
    POLICE_OFFICERS_CULLED_BODY_FREEZE   = 3,
};

/// Aya's clips in the officers' scenes (rooms/akropolis/plaza/police_scene_player_bank), indexed by
/// `POLICE_SCENE_PLAYER_ANIMATION_*`; slots 10 and 12 are empty. The officers install it on the player for their
/// requests, and the plaza for the helicopter wreck; the clips are the plaza's, so both overlays stay loaded.
extern AnimationSet* gPoliceOfficersPlayerAnimSets[13];

/// The rookie's spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// All at `TASK_PRIORITY_CONTROL`. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep police_officers callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gPoliceOfficersRookieTaskDescriptors[3];

/// The sergeant's spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// All at `TASK_PRIORITY_CONTROL`. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep police_officers callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gPoliceOfficersSergeantTaskDescriptors[3];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H
