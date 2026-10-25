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
    AKROPOLIS_CAFETERIA_SOUND_AMPOULE_CONTAINER_OPEN = 10, // "There's an ampoule inside." (CAP 10, room sound cue 10)
    AKROPOLIS_CAFETERIA_SOUND_CUE_11                 = 11, // No CAP line asks for it, and the bank names no sound 11
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
    AKROPOLIS_CAFETERIA_ACTION_FOLLOW_UP    = 0, // Examining the creature, once the battle is over
    AKROPOLIS_CAFETERIA_ACTION_DEAD_SWAT    = 2, // The dead SWAT member
    AKROPOLIS_CAFETERIA_ACTION_START_BATTLE = 3, // The civilian: the opening and the battle, once
};

/// Room commands.
enum { AKROPOLIS_CAFETERIA_ROOM_COMMAND_JOURNAL = 7 }; // The scientific journal, after the battle

/// CAP commands and sequences.
enum {
    AKROPOLIS_CAFETERIA_FOLLOW_UP_CAP_COMMAND = 3, // "I've never seen this type of creature before."
    AKROPOLIS_CAFETERIA_DEAD_SWAT_CAP_COMMAND = 9, // "Another SWAT..."
    AKROPOLIS_CAFETERIA_HALLWAY_EXIT_SEQUENCE = 5, // "It's locked."
    AKROPOLIS_CAFETERIA_PATIO_EXIT_SEQUENCE   = 6, // "That creature... It was different, somehow.", after the battle
    AKROPOLIS_CAFETERIA_JOURNAL_CAP_SEQUENCE  = 7, // "A scientific journal..."
};

/// Placed objects whose state the scenes test: the creature's body (3) and the journal (4).
enum {
    AKROPOLIS_CAFETERIA_FOLLOW_UP_OBJECT = 3,
    AKROPOLIS_CAFETERIA_JOURNAL_OBJECT   = 4,
};

/// The action trigger of the trays ("Trays are scattered everywhere.", CAP 20), disabled once the battle is over.
enum { AKROPOLIS_CAFETERIA_TRAYS_TRIGGER = 9 };

/// The placement variant the battle spawns.
enum { AKROPOLIS_CAFETERIA_BATTLE_VARIANT = 2 };

/// The follow-up's scene event.
enum { AKROPOLIS_CAFETERIA_FOLLOW_UP_SCENE_EVENT = 4 };

// ---------------------------------------------------------------------------
// Tasks, messages and actors

/// Entries of the scene task table.
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
