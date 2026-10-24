#ifndef STAGES_AKROPOLIS_ROOMS_CAFETERIA_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_CAFETERIA_CONFIG_H

// Configuration of the Akropolis cafeteria.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the cafeteria's area bank.
#define AKROPOLIS_CAFETERIA_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_CAFETERIA, (cue))

/// Entries of the cafeteria's area bank that room sound cues play.
enum {
    AKROPOLIS_CAFETERIA_SOUND_CUE_10 = 10,
    AKROPOLIS_CAFETERIA_SOUND_CUE_11 = 11,
};

// ---------------------------------------------------------------------------
// Doors

/// The hallway door the cafeteria holds until MIST has been contacted.
enum { AKROPOLIS_CAFETERIA_HALLWAY_EXIT_WARP = 4 };

/// Where the follow-up leaves the player: the patio's warp 3.
enum { AKROPOLIS_CAFETERIA_FOLLOW_UP_PATIO_WARP = 3 };

// ---------------------------------------------------------------------------
// Commands, actions and CAP

/// Room actions.
enum {
    AKROPOLIS_CAFETERIA_ACTION_FOLLOW_UP    = 0, // Starts the follow-up, once the battle is over
    AKROPOLIS_CAFETERIA_ACTION_CAP_COMMAND  = 2, // Runs CAP command 9
    AKROPOLIS_CAFETERIA_ACTION_START_BATTLE = 3, // Starts the battle, once
};

/// Room commands.
enum { AKROPOLIS_CAFETERIA_ROOM_COMMAND_CAP_7 = 7 }; // Plays CAP sequence 7 after the battle

/// CAP commands and sequences.
enum {
    AKROPOLIS_CAFETERIA_FOLLOW_UP_CAP_COMMAND   = 3, // The conversation before the follow-up
    AKROPOLIS_CAFETERIA_ACTION_CAP_COMMAND_ID   = 9,
    AKROPOLIS_CAFETERIA_HALLWAY_EXIT_SEQUENCE   = 5, // The hallway door, held
    AKROPOLIS_CAFETERIA_PATIO_EXIT_SEQUENCE     = 6, // The patio door, held after the battle
    AKROPOLIS_CAFETERIA_ROOM_COMMAND_SEQUENCE_7 = 7,
};

/// Placed objects whose state the scenes test: the follow-up's (3) and CAP sequence 7's (4).
enum {
    AKROPOLIS_CAFETERIA_FOLLOW_UP_OBJECT = 3,
    AKROPOLIS_CAFETERIA_CAP_7_OBJECT     = 4,
};

/// The action trigger that plays CAP 20, disabled once the battle is over.
enum { AKROPOLIS_CAFETERIA_CAP_20_TRIGGER = 9 };

/// The follow-up's scene event.
enum { AKROPOLIS_CAFETERIA_FOLLOW_UP_SCENE_EVENT = 4 };

// ---------------------------------------------------------------------------
// Tasks, messages and actors

/// Entries of the scene task table, and the scene and movie tasks' scheduler priorities.
enum {
    AKROPOLIS_CAFETERIA_BATTLE_TASK_ENTRY    = 0,
    AKROPOLIS_CAFETERIA_FOLLOW_UP_TASK_ENTRY = 1,
};

/// Placed actors: the Stranger (the boss), its scripted form, and Rupert.
enum {
    AKROPOLIS_CAFETERIA_STRANGER_ACTOR      = 0,
    AKROPOLIS_CAFETERIA_SCRIPTED_FORM_ACTOR = 1,
    AKROPOLIS_CAFETERIA_RUPERT_ACTOR        = 2,
};

// ---------------------------------------------------------------------------
// Tuning

/// Where the debug display prints the player's label.
enum {
    AKROPOLIS_CAFETERIA_PLAYER_DEBUG_X = -140,
    AKROPOLIS_CAFETERIA_PLAYER_DEBUG_Y = -50,
};

#endif
