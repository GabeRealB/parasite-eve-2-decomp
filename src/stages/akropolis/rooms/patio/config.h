#ifndef STAGES_AKROPOLIS_ROOMS_PATIO_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_PATIO_CONFIG_H

// Configuration of the Akropolis patio.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the patio's area bank.
#define AKROPOLIS_PATIO_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_PATIO, (cue))

/// Entries of the patio's area bank.
enum {
    AKROPOLIS_PATIO_SOUND_FOUNTAIN_DOOR_DEPARTURE  = 1, // Leaving through the fountain door
    AKROPOLIS_PATIO_SOUND_FOUNTAIN_DOOR_ARRIVAL    = 2, // Arriving through the fountain door
    AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_UNLOCK    = 3, // The cafeteria key opens the door (CAP 3, room sound cue 3)
    AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_DEPARTURE = 4, // Leaving through the cafeteria door, also once it is opened
    AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_ARRIVAL   = 5, // Arriving through the cafeteria door
    AKROPOLIS_PATIO_SOUND_CAFETERIA_DOOR_BLOCKED   = 6, // The cafeteria door refuses the player
    AKROPOLIS_PATIO_SOUND_FOUNTAIN_DOOR_BLOCKED    = 7, // The fountain door refuses the player
};

/// Footsteps on surface class 4.
enum {
    AKROPOLIS_PATIO_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x01,
    AKROPOLIS_PATIO_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x03,
    AKROPOLIS_PATIO_FOOTSTEP_JUMP = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
};

// ---------------------------------------------------------------------------
// Doors

/// Trigger parameters: each door's departure and arrival warps, and the stairs (surface class 4 above the step
/// count; the control bit selects descent).
enum {
    AKROPOLIS_PATIO_TRIGGER_FOUNTAIN_WARPS  = (2 << 4) | 1,
    AKROPOLIS_PATIO_TRIGGER_CAFETERIA_WARPS = (3 << 4) | 2,
    AKROPOLIS_PATIO_TRIGGER_SQUARE_WARPS    = (1 << 4) | 5,
    AKROPOLIS_PATIO_TRIGGER_STAIR_ASCEND    = (4 << 4) | 3,
    AKROPOLIS_PATIO_TRIGGER_STAIR_DESCEND   = (4 << 4) | 2,
    AKROPOLIS_PATIO_TRIGGER_STAIR_CONTROL   = 0x100,
};

/// The rooms the patio's doors lead to in the cafeteria and the fountain.
enum {
    AKROPOLIS_PATIO_FOUNTAIN_AFTER_RED_KEY_ROOM = 2, // The fountain once the red key is in the security panel
    AKROPOLIS_PATIO_CAFETERIA_AFTER_BOSS_ROOM   = 3, // The cafeteria while its boss's report is pending
};

// ---------------------------------------------------------------------------
// Commands, actions and CAP

/// Room actions: the triggers that start the patio's scenes.
enum {
    AKROPOLIS_PATIO_ACTION_OPENING   = 0, // The SWAT scene
    AKROPOLIS_PATIO_ACTION_FOLLOW_UP = 1, // Its follow-up, once the Stranger has left
    AKROPOLIS_PATIO_ACTION_AMBUSH    = 2, // The ambush on the way back from the cafeteria
};

/// Room commands, and the CAP events they start.
enum {
    AKROPOLIS_PATIO_ROOM_COMMAND_GYRO     = 1, // "A gyro, left half-eaten." (CAP 1)
    AKROPOLIS_PATIO_ROOM_COMMAND_CIVILIAN = 2, // "There's a woman in there!" (CAP 2), only before the cafeteria boss
};

/// The scene event the opening sets.
enum { AKROPOLIS_PATIO_OPENING_SCENE_EVENT = 3 };

/// CAP commands.
enum {
    AKROPOLIS_PATIO_CAFETERIA_DOOR_CAP_COMMAND       = 3, // The cafeteria door: locked, or the choice to open it
    AKROPOLIS_PATIO_FOUNTAIN_BLOCKED_CAP_COMMAND     = 4, // The fountain door, before it is unlocked
    AKROPOLIS_PATIO_SCENE_CHOICE_CAP_COMMAND         = 6, // A choice whose key 1 makes the map available; the patio's file has none
    AKROPOLIS_PATIO_CAFETERIA_DOOR_AFTER_CAP_COMMAND = 8, // The cafeteria door after Rupert's scene
};

// ---------------------------------------------------------------------------
// Tasks, messages and actors

/// Entries of the scene task table, and the tasks' scheduler priorities.
enum {
    AKROPOLIS_PATIO_SCENE_CHOICE_TASK_ENTRY   = 0, // Never spawned
    AKROPOLIS_PATIO_CAFETERIA_DOOR_TASK_ENTRY = 1,
    AKROPOLIS_PATIO_HEAD_AIM_TASK_ENTRY       = 2,
    AKROPOLIS_PATIO_HEAD_AIM_TASK_PRIORITY    = 97,
};

/// Placed actors: the SWAT member, the Stranger, and the ambush's two Strangers.
enum {
    AKROPOLIS_PATIO_SWAT_ACTOR     = 0,
    AKROPOLIS_PATIO_STRANGER_ACTOR = 1,
    AKROPOLIS_PATIO_AMBUSH_ACTOR_1 = 2,
    AKROPOLIS_PATIO_AMBUSH_ACTOR_2 = 3,
};

/// Placement variants: the opening's, and the ambush's.
enum {
    AKROPOLIS_PATIO_OPENING_VARIANT = 1,
    AKROPOLIS_PATIO_AMBUSH_VARIANT  = 2,
};

// ---------------------------------------------------------------------------
// Tuning

/// The point the player's head turns to in the opening, how far it may turn, and its slide along Z.
enum {
    AKROPOLIS_PATIO_HEAD_AIM_X              = -8000,
    AKROPOLIS_PATIO_HEAD_AIM_Z              = 900,
    AKROPOLIS_PATIO_HEAD_AIM_SLIDE_STEP     = 50,
    AKROPOLIS_PATIO_HEAD_AIM_SLIDE_DISTANCE = 4096,
};

/// The player's scripted half turn: its length in ticks.
enum { AKROPOLIS_PATIO_PLAYER_TURN_TICKS = 16 };

/// The light glows: the spawn argument's fields (anchor, sheet cell, size), and the sheet (glow_draw.h's flare page).
enum {
    AKROPOLIS_PATIO_LIGHT_GLOW_ANCHOR_MASK   = 0xF,
    AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SHIFT    = 8,
    AKROPOLIS_PATIO_LIGHT_GLOW_CELL_MASK     = 3,
    AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_SHIFT    = 16,
    AKROPOLIS_PATIO_LIGHT_GLOW_SIZE_MASK     = 0xFFF,
    AKROPOLIS_PATIO_LIGHT_GLOW_DEFAULT_SIZE  = 640,
    AKROPOLIS_PATIO_LIGHT_GLOW_CLUT_Y        = 270,
    AKROPOLIS_PATIO_LIGHT_GLOW_CELL_SIZE     = 40,
    AKROPOLIS_PATIO_LIGHT_GLOW_FLICKER_SHIFT = 4,
};

#endif
