#ifndef INCLUDE_ACTORS_ACTOR_310100_H
#define INCLUDE_ACTORS_ACTOR_310100_H

#include "gameplay/animation.h"

#include "main/task_types.h"

/// Area placement entries the plaza's two police officers are spawned from: the rookie officer at the tower's entrance
/// and the sergeant at the barricade. The same IDs select each one's placement record in the plaza's nested layout.
enum {
    ACTOR_310100_PLACEMENT_OFFICER  = 0x6C,
    ACTOR_310100_PLACEMENT_SERGEANT = 0x6D,
};

/// Messages an officer answers besides the common actor messages.
enum {
    ACTOR_310100_MESSAGE_STOP_CONTROLLER = 2002,
    ACTOR_310100_MESSAGE_SET_CULLED_BODY = 2007, // Argument: `ACTOR_310100_CULLED_BODY_*`
};

/// What an `ACTOR_MESSAGE_PLAY_ANIMATION` request's `source.index` selects: 0 plays a clip of the paired player's bank;
/// any other plays one of the receiving officer's own bank and chains through that follow-up table - the officer's or
/// the sergeant's, whoever receives it (the sergeant's later talks send the officer's).
enum {
    ACTOR_310100_ANIMATION_PLAYER   = 0,
    ACTOR_310100_FOLLOW_UP_OFFICER  = 1,
    ACTOR_310100_FOLLOW_UP_SERGEANT = 2,
};

/// What `ACTOR_310100_MESSAGE_SET_CULLED_BODY` does: swap in the sergeant's or the officer's culled body, or freeze the
/// current model.
enum {
    ACTOR_310100_CULLED_BODY_SERGEANT = 0,
    ACTOR_310100_CULLED_BODY_OFFICER  = 1,
    ACTOR_310100_CULLED_BODY_FREEZE   = 3,
};

/// Aya's clips in the officers' scenes (rooms/akropolis/plaza/police_scene_player_bank), indexed by
/// `POLICE_SCENE_PLAYER_ANIMATION_*`; slots 10 and 12 are empty. The officers install it on the player for their
/// requests, and the plaza for the helicopter wreck; the clips are the plaza's, so both overlays stay loaded.
extern AnimationSet* gActor310100PlayerAnimationSets[13];

/// The officer's spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// Priority 192. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep actor_310100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor310100OfficerTaskDescriptors[3];

/// The sergeant's spawn recipes: 0 movie controller, 1 animated body, 2 culled body.
///
/// Priority 192. Indices 0..2 have no terminator.
/// Both TMD model entries skip automatic primitive buffers.
/// The controller has no body; its room spawn carries a live `Enemy` in
/// `Task::spawnArg2`. Model entries use `Task::spawnArg1` as a body animation id and ignore
/// `Task::spawnArg2`. Their work is owned by the model task; model swaps release
/// the previous model before starting another.
/// Keep actor_310100 callbacks and borrowed model/animation resources loaded
/// while the spawned tasks use them.
extern TaskDesc gActor310100SergeantTaskDescriptors[3];

#endif // INCLUDE_ACTORS_ACTOR_310100_H
