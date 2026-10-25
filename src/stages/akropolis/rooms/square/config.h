#ifndef STAGES_AKROPOLIS_ROOMS_SQUARE_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_SQUARE_CONFIG_H

// Configuration of the Akropolis square.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the square's area bank.
#define AKROPOLIS_SQUARE_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_SQUARE, (cue))

/// Entries of the square's area bank.
enum {
    AKROPOLIS_SQUARE_SOUND_CALL_START         = 1,  // The call cutscene starts; also room sound cue 1
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_DEPARTURE = 2,  // Leaving through an elevator door
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_ARRIVAL   = 3,  // Arriving through an elevator door
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_BLOCKED   = 4,  // An elevator door refuses the player
    AKROPOLIS_SQUARE_SOUND_AMMO_BOX_OPEN      = 5,  // The ammo box opens (CAP 4, room sound cue 5)
    AKROPOLIS_SQUARE_SOUND_CALL_SCENE         = 6,  // The call cutscene's scene sound; also room sound cue 6
    AKROPOLIS_SQUARE_SOUND_CALL_END           = 7,  // The call cutscene ends; also room sound cue 7
    AKROPOLIS_SQUARE_SOUND_AMMO_BOX_CLOSE     = 8,  // The ammo box closes (CAP 4, room sound cue 8)
    AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING     = 9,  // One ring of the telephone
    AKROPOLIS_SQUARE_SOUND_TELEPHONE_SCENE    = 10, // The telephone scene's cue, as the stranger runs off
    AKROPOLIS_SQUARE_SOUND_CALL_AFTER         = 11, // After the call cutscene's scene
};

/// Footsteps on surface class 4.
enum {
    AKROPOLIS_SQUARE_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    AKROPOLIS_SQUARE_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

// ---------------------------------------------------------------------------
// Doors and arrivals

/// Trigger parameters: the stair descent action and each door's departure and arrival warps.
enum {
    AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND       = 0x100,
    AKROPOLIS_SQUARE_TRIGGER_WEST_ELEVATOR_WARPS = (1 << 4) | 2,
    AKROPOLIS_SQUARE_TRIGGER_EAST_ELEVATOR_WARPS = (2 << 4) | 1,
    AKROPOLIS_SQUARE_TRIGGER_PATIO_WARPS         = (3 << 4) | 4,
    AKROPOLIS_SQUARE_TRIGGER_FORKED_ROAD_WARPS   = (4 << 4) | 4,
};

/// The arrival scene's warp, and the warp the call cutscene saves in its place.
enum {
    AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP = 7,
    AKROPOLIS_SQUARE_CUTSCENE_RESUME_WARP  = 1,
};

/// Placement variants set after the first call.
enum {
    AKROPOLIS_SQUARE_POST_CUTSCENE_PATIO_VARIANT     = 3,
    AKROPOLIS_SQUARE_POST_CUTSCENE_CAFETERIA_VARIANT = 3,
    AKROPOLIS_SQUARE_POST_CUTSCENE_PARKING_VARIANT   = 2,
    AKROPOLIS_SQUARE_POST_CUTSCENE_MAP_MARK          = 1,
};

/// Placement variant after the telephone scene: the grinning stranger is gone.
enum { AKROPOLIS_SQUARE_TELEPHONE_PLACEMENT_VARIANT = 2 };

// ---------------------------------------------------------------------------
// Commands, actions, CAP scenes and story dialogue

/// Room commands.
enum {
    AKROPOLIS_SQUARE_ROOM_COMMAND_CUTSCENE      = 2,  // Answer the telephone: the call cutscene
    AKROPOLIS_SQUARE_ROOM_COMMAND_AMMO_BOX_HINT = 14, // Near the ammo box the first time: "something on the bench" (CAP 14)
    AKROPOLIS_SQUARE_ROOM_COMMAND_STAIRS_HINT   = 16, // At the stairs the first time: their controls (CAP 16)
};

/// The room action: reading the map board, which shows the rooftop's map (CAP 5).
enum { AKROPOLIS_SQUARE_ACTION_MAP_BOARD = 0 };

/// The scene event the arrival scene sets.
enum { AKROPOLIS_SQUARE_ARRIVAL_SCENE_EVENT = 2 };

/// CAP commands, and the call cutscene's CAP file and slot.
enum {
    AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND  = 1,  // "It won't open!": a refused elevator door
    AKROPOLIS_SQUARE_MAP_BOARD_CAP_COMMAND         = 5,  // The map board
    AKROPOLIS_SQUARE_STAIRS_HINT_ALTERNATE_COMMAND = 17, // The stairs hint for the other button layout
    AKROPOLIS_SQUARE_CUTSCENE_CAP_SLOT             = 1,
    AKROPOLIS_SQUARE_CUTSCENE_CAP_FILE             = 1,
    AKROPOLIS_SQUARE_CUTSCENE_FOLLOWUP             = 2, // Call file command after the call: the save point
};

// ---------------------------------------------------------------------------
// Tasks and messages

/// Entries of the room's task tables.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_TASK_INDEX = 0, // In the sequence table: the ring task
    AKROPOLIS_SQUARE_BACKDROP_TASK_INDEX  = 2, // In the sequence table: the scrolling backdrop
    AKROPOLIS_SQUARE_CUTSCENE_TASK_ENTRY  = 0, // In the room cutscene table
};

/// Player reflections: their task and its two modes.
enum {
    AKROPOLIS_SQUARE_REFLECTION_TASK_BANK = 1,
    AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT = 37,
    AKROPOLIS_SQUARE_REFLECTION_FLOOR     = 0,
    AKROPOLIS_SQUARE_REFLECTION_PLANE     = 1,
};

/// Messages the square's tasks receive.
enum {
    AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE = 3103,
};

/// Telephone light modes.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE    = 0,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_RINGING = 1,
};

// ---------------------------------------------------------------------------
// Views

/// Views of the telephone and the call.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_FIRST_FOLLOWUP_VIEW = 5, // Views 5..7 keep the phone ringing
    AKROPOLIS_SQUARE_TELEPHONE_FOLLOWUP_VIEW_COUNT = 3,
    AKROPOLIS_SQUARE_TELEPHONE_SCENE_VIEW          = 9, // The telephone's view, which also keeps it ringing
    AKROPOLIS_SQUARE_CUTSCENE_VIEW                 = 9, // The call cutscene
};

/// The view the arrival's panorama cuts to, and the map board sequence's two views.
enum {
    AKROPOLIS_SQUARE_BACKDROP_FINISH_VIEW  = 13,
    AKROPOLIS_SQUARE_MAP_BOARD_FIRST_VIEW  = 7,
    AKROPOLIS_SQUARE_MAP_BOARD_SECOND_VIEW = 8,
};

/// Views that show the telephone's light (mask bit n is view n + 1).
enum {
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_VIEW_MASK  = 0x268,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_VIEW  = 14,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_VIEW = 9,
};

// ---------------------------------------------------------------------------
// Tuning

/// The telephone's position, and where its light shows in the side and close views, in world units.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_X             = 6570,
    AKROPOLIS_SQUARE_TELEPHONE_Y             = -3990,
    AKROPOLIS_SQUARE_TELEPHONE_Z             = 2270,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_X  = 6354,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_Y  = -4107,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_Z  = 2219,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_X = 6570,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_Y = -3990,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_Z = 2280,
};

/// Frames between rings.
enum { AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES = 121 };

/// The telephone light's presets: pulse rate in bits 0..7, radius in 8..15, cyan in
/// bit 16, streaks in bit 28, round fan in bit 31. Ringing adds the delta.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_BASE       = 0x00010E08,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_MODE_DELTA = 0x10000218,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_BASE        = 0x10010608,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_MODE_DELTA  = 0x218,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_BASE       = 0x80010308,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_MODE_DELTA = 0x118,
};

/// The arrival's panorama: its frames before and after it pans, and where its five pages sit in video memory (the
/// actor sheet; view 15's file loads them) with their palette (the first room sprite palette).
enum {
    AKROPOLIS_SQUARE_BACKDROP_DELAY_FRAMES = 46,
    AKROPOLIS_SQUARE_BACKDROP_HOLD_FRAMES  = 31,
    AKROPOLIS_SQUARE_PANORAMA_VRAM_X       = 0x1C0,
    AKROPOLIS_SQUARE_PANORAMA_VRAM_Y       = 0x100,
    AKROPOLIS_SQUARE_PANORAMA_CLUT_Y       = 0xFF,
};

#endif
