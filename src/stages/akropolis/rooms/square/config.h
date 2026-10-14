#ifndef STAGES_AKROPOLIS_ROOMS_SQUARE_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_SQUARE_CONFIG_H

/* Configuration of the Akropolis square: the values that tie the room to the
 * rest of the game (doors, commands, CAP scenes, tasks, messages, views and
 * sounds) and the values that tune how it behaves.
 *
 * State-machine states and the layout of the room's own data stay with the
 * code that uses them.
 */

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the square's area bank.
#define AKROPOLIS_SQUARE_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, GAME_AREA_AKROPOLIS_SQUARE, (cue))

/// Entries of the square's area bank.
enum {
    AKROPOLIS_SQUARE_SOUND_CALL_START         = 1, // The call cutscene starts; also room sound cue 1
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_DEPARTURE = 2, // Leaving through an elevator door
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_ARRIVAL   = 3, // Arriving through an elevator door
    AKROPOLIS_SQUARE_SOUND_ELEVATOR_BLOCKED   = 4, // An elevator door refuses the player
    AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_5       = 5, // Requested by a CAP script as room sound cue 5
    AKROPOLIS_SQUARE_SOUND_CALL_SCENE         = 6, // The call cutscene's scene sound; also room sound cue 6
    AKROPOLIS_SQUARE_SOUND_CALL_END           = 7, // The call cutscene ends; also room sound cue 7
    AKROPOLIS_SQUARE_SOUND_SCRIPT_CUE_8       = 8, // Requested by a CAP script as room sound cue 8
    /// The telephone ringing: a positional one-shot from the phone, every
    /// `AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES` until the call is answered or left.
    AKROPOLIS_SQUARE_SOUND_TELEPHONE_RING  = 9,
    AKROPOLIS_SQUARE_SOUND_TELEPHONE_SCENE = 10, // The telephone scene's cue, as the stranger runs off
    AKROPOLIS_SQUARE_SOUND_CALL_AFTER      = 11, // After the call cutscene's scene
};

/// Footstep requests of the floor's surface class 4, type-1 bank. Each is the
/// request for cue 2; cue 1 uses the following entry.
enum {
    AKROPOLIS_SQUARE_FOOTSTEP_WALK = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x11,
    AKROPOLIS_SQUARE_FOOTSTEP_RUN  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x13,
};

// ---------------------------------------------------------------------------
// Doors and arrivals

/// Descending stair action and the packed departure/arrival warp slots used here.
enum {
    AKROPOLIS_SQUARE_TRIGGER_STAIR_DESCEND       = 0x100,
    AKROPOLIS_SQUARE_TRIGGER_WEST_ELEVATOR_WARPS = (1 << 4) | 2,
    AKROPOLIS_SQUARE_TRIGGER_EAST_ELEVATOR_WARPS = (2 << 4) | 1,
    AKROPOLIS_SQUARE_TRIGGER_PATIO_WARPS         = (3 << 4) | 4,
    AKROPOLIS_SQUARE_TRIGGER_FORKED_ROAD_WARPS   = (4 << 4) | 4,
};

/// The arrival warp of the square's arrival scene; the call cutscene saves it as warp 1.
enum {
    AKROPOLIS_SQUARE_CUTSCENE_ARRIVAL_WARP = 7,
    AKROPOLIS_SQUARE_CUTSCENE_RESUME_WARP  = 1,
};

/// Placement layouts and the map-mark policy applied after the square story cutscene.
enum {
    AKROPOLIS_SQUARE_POST_CUTSCENE_PATIO_VARIANT     = 3,
    AKROPOLIS_SQUARE_POST_CUTSCENE_CAFETERIA_VARIANT = 3,
    AKROPOLIS_SQUARE_POST_CUTSCENE_PARKING_VARIANT   = 2,
    AKROPOLIS_SQUARE_POST_CUTSCENE_MAP_MARK          = 1,
};

/// Placement variant the telephone scene sets: empty, once the grinning stranger has gone.
enum { AKROPOLIS_SQUARE_TELEPHONE_PLACEMENT_VARIANT = 2 };

// ---------------------------------------------------------------------------
// Commands, actions, CAP scenes and story dialogue

/// Room-message commands, sent by the room's triggers and by CAP scripts.
enum {
    AKROPOLIS_SQUARE_ROOM_COMMAND_CUTSCENE      = 2,  // Answer the telephone: the call cutscene
    AKROPOLIS_SQUARE_ROOM_COMMAND_HINT_14       = 14, // The ammo box hint, CAP 14, once
    AKROPOLIS_SQUARE_ROOM_COMMAND_CONTROLS_HINT = 16, // The stairs controls hint, CAP 16, once
};

/// The square's one direction action, and the CAP event it starts.
enum {
    AKROPOLIS_SQUARE_ACTION_START_EVENT = 0,
    AKROPOLIS_SQUARE_ACTION_CAP_EVENT   = 5,
};

/// Scene music event the arrival scene arms (`state.sceneEvent`).
enum { AKROPOLIS_SQUARE_ARRIVAL_SCENE_EVENT = 2 };

/// CAP commands and the call cutscene's CAP file and slot.
enum {
    AKROPOLIS_SQUARE_ELEVATOR_BLOCKED_CAP_COMMAND    = 1,  // Caption of a refused elevator door
    AKROPOLIS_SQUARE_CAP_VIEW_COMMAND                = 5,  // The CAP view sequence
    AKROPOLIS_SQUARE_ARRIVAL_CAP_COMMAND             = 10, // The arrival scene
    AKROPOLIS_SQUARE_CONTROLS_HINT_ALTERNATE_COMMAND = 17, // The stairs hint for the other button layout
    AKROPOLIS_SQUARE_CUTSCENE_CAP_SLOT               = 1,
    AKROPOLIS_SQUARE_CUTSCENE_CAP_FILE               = 1,
};

/// Story dialogue indices (`GAME_FLAG_STORY_DIALOGUE_INDEX`) the square selects.
enum {
    /// Ringing ended without the call being answered - its views left, or the
    /// square left by the forked road or the east elevator.
    AKROPOLIS_SQUARE_TELEPHONE_MISSED_DIALOGUE = 2,
    /// Follow-up the call cutscene hands to the room cutscene task.
    AKROPOLIS_SQUARE_CUTSCENE_FOLLOWUP = 2,
};

// ---------------------------------------------------------------------------
// Tasks and messages

/// Entries of the room's task tables.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_TASK_INDEX = 0, // In the sequence table: the ring task
    AKROPOLIS_SQUARE_BACKDROP_TASK_INDEX  = 2, // In the sequence table: the scrolling backdrop
    AKROPOLIS_SQUARE_CUTSCENE_TASK_ENTRY  = 0, // In the room cutscene table
};

/// Scheduler priorities of the square's tasks.
enum {
    AKROPOLIS_SQUARE_REFLECTION_TASK_PRIORITY = 112,
    AKROPOLIS_SQUARE_SEQUENCE_TASK_PRIORITY   = 32,
    AKROPOLIS_SQUARE_BACKDROP_TASK_PRIORITY   = 192,
};

/// Player reflections: the task bank slot that draws them and its two modes.
enum {
    AKROPOLIS_SQUARE_REFLECTION_TASK_BANK = 1,
    AKROPOLIS_SQUARE_REFLECTION_TASK_SLOT = 37,
    AKROPOLIS_SQUARE_REFLECTION_FLOOR     = 0,
    AKROPOLIS_SQUARE_REFLECTION_PLANE     = 1,
};

/// Messages the square's tasks receive.
enum {
    AKROPOLIS_SQUARE_MESSAGE_USE_KEY_ITEM = 5105,
    /// Selects the room effect's telephone-light mode with an integer first payload.
    AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE = 3103,
};

/// Payloads of `AKROPOLIS_SQUARE_MESSAGE_SET_TELEPHONE_LIGHT_MODE`.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_IDLE    = 0,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_RINGING = 1,
};

/// Player weapon-bank clip of the arrival presentation.
enum { AKROPOLIS_SQUARE_ARRIVAL_ANIMATION_ID = 1 };

// ---------------------------------------------------------------------------
// Views

/// Views of the telephone scene and the call.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_EVENT_VIEW          = 14, // The scene's camera, as the stranger runs off
    AKROPOLIS_SQUARE_TELEPHONE_FIRST_FOLLOWUP_VIEW = 5,  // Views 5..7 keep the phone ringing
    AKROPOLIS_SQUARE_TELEPHONE_FOLLOWUP_VIEW_COUNT = 3,
    AKROPOLIS_SQUARE_TELEPHONE_SCENE_VIEW          = 9,  // The telephone's view, which also keeps it ringing
    AKROPOLIS_SQUARE_CUTSCENE_VIEW                 = 9,  // The call cutscene
};

/// Views of the arrival scene and the CAP view sequence.
enum {
    AKROPOLIS_SQUARE_ARRIVAL_EVENT_VIEW   = 15,
    AKROPOLIS_SQUARE_ARRIVAL_SKIP_VIEW    = 13,
    AKROPOLIS_SQUARE_BACKDROP_FINISH_VIEW = 13,
    AKROPOLIS_SQUARE_CAP_VIEW_FIRST       = 7,
    AKROPOLIS_SQUARE_CAP_VIEW_SECOND      = 8,
};

/// Views that show the telephone's light: front views by mask (bit n is view n + 1),
/// and the side and round presets in their own views.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_VIEW_MASK  = 0x268,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_VIEW  = 14,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_VIEW = 9,
};

// ---------------------------------------------------------------------------
// Tuning

/// The telephone, in whole room-coordinate units: the ring sound and the front light.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_X = 0x19AA,
    AKROPOLIS_SQUARE_TELEPHONE_Y = -0xF96,
    AKROPOLIS_SQUARE_TELEPHONE_Z = 0x8DE,
};

/// Frames between rings, and the length of the telephone scene's camera.
enum {
    AKROPOLIS_SQUARE_TELEPHONE_RING_FRAMES  = 121,
    AKROPOLIS_SQUARE_TELEPHONE_EVENT_FRAMES = 148,
};

/// The telephone light's presets, packed spawn words: pulse rate in bits 0..7,
/// radius in 8..15, cyan in bit 16, streaks in bit 28, round fan in bit 31.
/// Ringing adds `MODE_DELTA` once.
enum {
    // Rate 8, radius 14, cyan diamond; ringing adds rate 24, radius 2 and streaks.
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_BASE       = 0x00010E08,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_FRONT_MODE_DELTA = 0x10000218,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_BASE        = 0x10010608,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_SIDE_MODE_DELTA  = 0x218,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_BASE       = 0x80010308,
    AKROPOLIS_SQUARE_TELEPHONE_LIGHT_ROUND_MODE_DELTA = 0x118,
};

/// Lengths of the arrival scene and the scrolling backdrop.
enum {
    AKROPOLIS_SQUARE_ARRIVAL_EVENT_FRAMES  = 395,
    AKROPOLIS_SQUARE_SKIP_FADE_FRAMES      = 8,
    AKROPOLIS_SQUARE_BACKDROP_DELAY_FRAMES = 46,
    AKROPOLIS_SQUARE_BACKDROP_HOLD_FRAMES  = 31,
};

#endif
