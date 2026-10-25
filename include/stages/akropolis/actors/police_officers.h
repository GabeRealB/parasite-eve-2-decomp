#ifndef INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H
#define INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Area placement entries the plaza's two police officers are spawned from: the rookie at the tower's entrance and the
/// sergeant at the barricade.
enum {
    POLICE_OFFICERS_PLACEMENT_ROOKIE   = 0x6C,
    POLICE_OFFICERS_PLACEMENT_SERGEANT = 0x6D,
};

/// Messages an officer answers besides the common actor messages.
enum {
    POLICE_OFFICERS_MESSAGE_STOP_CONTROLLER = 2002,
    POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY = 2007, // Argument: `POLICE_OFFICERS_CULLED_BODY_*`
};

/// What an `ACTOR_MESSAGE_PLAY_ANIMATION` request's `source.index` selects: 0 plays a clip of the paired player's
/// bank; any other plays one of the receiving officer's own bank and chains through that follow-up table - the
/// rookie's or the sergeant's, whoever receives it (the sergeant's later talks send the rookie's).
enum {
    POLICE_OFFICERS_ANIMATION_PLAYER   = 0,
    POLICE_OFFICERS_FOLLOW_UP_ROOKIE   = 1,
    POLICE_OFFICERS_FOLLOW_UP_SERGEANT = 2,
};

/// What `POLICE_OFFICERS_MESSAGE_SET_CULLED_BODY` does: swap in the sergeant's or the rookie's culled body, or freeze
/// the current model.
enum {
    POLICE_OFFICERS_CULLED_BODY_SERGEANT = 0,
    POLICE_OFFICERS_CULLED_BODY_ROOKIE   = 1,
    POLICE_OFFICERS_CULLED_BODY_FREEZE   = 3,
};

/// Aya's clips in the officers' scenes (rooms/akropolis/plaza/police_scene_player_bank), indexed by
/// `POLICE_SCENE_PLAYER_ANIMATION_*`; slots 10 and 12 are empty.
extern AnimationSet* gPoliceOfficersPlayerAnimSets[13];

/// The rookie's tasks: his controller, the actor the plaza's placements spawn (its `Task::spawnArg2` the live
/// `Enemy`), and his body and culled body, spawned on the clip in `Task::spawnArg1`.
extern TaskDesc gPoliceOfficersRookieTaskDescriptors[3];

/// The sergeant's tasks: his controller, the actor the plaza's placements spawn (its `Task::spawnArg2` the live
/// `Enemy`), and his body and culled body, spawned on the clip in `Task::spawnArg1`.
extern TaskDesc gPoliceOfficersSergeantTaskDescriptors[3];

#endif // INCLUDE_STAGES_AKROPOLIS_ACTORS_POLICE_OFFICERS_H
