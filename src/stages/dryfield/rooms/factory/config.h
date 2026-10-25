#ifndef STAGES_DRYFIELD_ROOMS_FACTORY_CONFIG_H
#define STAGES_DRYFIELD_ROOMS_FACTORY_CONFIG_H

/* Configuration of the Dryfield factory: the values that tie the room to the
 * rest of the game (doors, commands, CAP scenes, game flags, tasks, views and
 * sounds) and the values that tune how it behaves. One set serves the day and
 * the night package; where they differ, `DRYFIELD_TIME` selects.
 *
 * State-machine states and the layout of the room's own data stay with the
 * code that uses them.
 */

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"

#include "../../dryfield_time.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the factory's area bank, by day or by night.
///
/// Code that runs in both packages picks one by the stage at run time; the
/// package's own tables use `FACTORY_SOUND`, its own bank.
#define FACTORY_DAY_SOUND(cue)   SOUND_AREA(GAME_STAGE_DRYFIELD, GAME_AREA_DRYFIELD_FACTORY, (cue))
#define FACTORY_NIGHT_SOUND(cue) SOUND_AREA(GAME_STAGE_DRYFIELD_NIGHT, GAME_AREA_DRYFIELD_NIGHT_FACTORY, (cue))
#if DRYFIELD_TIME == DRYFIELD_DAY
#define FACTORY_SOUND(cue) FACTORY_DAY_SOUND(cue)
#else
#define FACTORY_SOUND(cue) FACTORY_NIGHT_SOUND(cue)
#endif

/// Entries of the factory's area bank; both packages' banks use the same numbering.
enum {
    FACTORY_SOUND_DRIVEWAY_DEPARTURE  = 1,
    FACTORY_SOUND_DRIVEWAY_ARRIVAL    = 2,
    FACTORY_SOUND_BREEZEWAY_DEPARTURE = 3,
    FACTORY_SOUND_BREEZEWAY_ARRIVAL   = 4,
    FACTORY_SOUND_BREEZEWAY_BLOCKED   = 5,
    FACTORY_SOUND_GARAGE_DEPARTURE    = 6,
    FACTORY_SOUND_SCRIPT_CUE_7        = 7, // Requested by a CAP script through ROOM_MESSAGE_SOUND
    /// Lift platform raise/lower movement, started as it moves and stopped at the end.
    FACTORY_SOUND_LIFT_MOVE = 8,
    /// Operator-panel button press, added to the bank's base request.
    FACTORY_SOUND_PANEL_BUTTON = 9,
    /// The examine action that starts the factory's first scene.
    FACTORY_SOUND_START_SCENE = 0x0A,
    /// Played (day only) as the white-out scene flashes and reloads the room as variant 2.
    FACTORY_SOUND_WHITEOUT = 0x0B,
    /// The barrier-transition scene accepted, day only.
    FACTORY_SOUND_BARRIER_TRANSITION_ACCEPTED = 0x0C,
    FACTORY_SOUND_HATCH_OPEN                  = 0x0D, // The lift's hatch swinging open
    FACTORY_SOUND_HATCH_CLOSE                 = 0x0E, // The lift's hatch swinging shut
    FACTORY_SOUND_LIFT_TURN                   = 0x0F, // Quarter-turn movement, stopped when the turn ends
    FACTORY_SOUND_LIFT_MOVE_STOP              = 0x10, // The lift finishes raising or lowering
    FACTORY_SOUND_LIFT_TURN_STOP              = 0x11, // The lift finishes a turn
    /// Bang when a turn is tried with the lift lowered and it jams, with a screen jolt.
    FACTORY_SOUND_LIFT_JAM = 0x12,
    /// First and second sounds of the driveway door's event; the second is the
    /// same entry as the breezeway door's departure.
    FACTORY_SOUND_DRIVEWAY_EVENT_FIRST  = 0x13,
    FACTORY_SOUND_DRIVEWAY_EVENT_SECOND = 3,
    FACTORY_SOUND_GARAGE_ARRIVAL        = 0x14,
    /// Requested by a CAP script; also moves the lamp to its second position.
    FACTORY_SOUND_LAMP_SECOND_POSITION = 0x15,
};

/// Footstep requests for the floor's surface classes 2 and 3, type-1 bank.
/// Each is the request for cue 2; cue 1 uses the following entry.
enum {
    FACTORY_SURFACE_2_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x51,
    FACTORY_SURFACE_2_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x53,
    FACTORY_SURFACE_3_WALK_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x15,
    FACTORY_SURFACE_3_RUN_SOUND  = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x17,
    FACTORY_SURFACE_3_JUMP_SOUND = SOUND_SCRIPT_REQUEST_TYPE_1 | 0x19,
};

// ---------------------------------------------------------------------------
// Doors

/// Map-marker nibbles shared with the opposite endpoint of each factory door.
enum {
    FACTORY_GARAGE_MAP_FLAG    = 473,
    FACTORY_DRIVEWAY_MAP_FLAG  = 474,
    FACTORY_BREEZEWAY_MAP_FLAG = 475,
};

/// Packed 1-based departure/arrival slots of the three doors, departure in the high nibble.
enum {
    FACTORY_DRIVEWAY_WARP_SLOTS  = (1 << 4) | 1,
    FACTORY_BREEZEWAY_WARP_SLOTS = (2 << 4) | 2,
    FACTORY_GARAGE_WARP_SLOTS    = (3 << 4) | 1,
};

/// Rules for leaving through the doors, applied by `roomVariantFactoryMsg`.
enum {
    FACTORY_DRIVEWAY_PROGRESS_OPEN     = 2, // Driveway progress from which the day driveway keeps its variant
    FACTORY_DRIVEWAY_DEFAULT_ROOM      = 1, // Driveway variant before that
    FACTORY_GARAGE_FINAL_CHAPTER       = 4, // Story chapter from which the garage is its late variant
    FACTORY_GARAGE_EARLY_ROOM          = 1,
    FACTORY_GARAGE_LATE_ROOM           = 2,
    FACTORY_DEPARTURE_REFUSAL_MAP_MARK = 2, // Map-marker value of a door that refused the player
};

// ---------------------------------------------------------------------------
// Room commands, actions and CAP scenes

/// Room-message commands, sent by the room's triggers and by CAP scripts.
enum {
    FACTORY_COMMAND_CAP_SCENE          = 1,
    FACTORY_COMMAND_BARRIER_TRANSITION = 2,
    FACTORY_COMMAND_LAMP_SCENE         = 3,
    FACTORY_COMMAND_POWER_SCENE        = 5,
    FACTORY_COMMAND_PANEL_SESSION      = 6,
    FACTORY_COMMAND_HATCH_SCENE        = 12,
};

/// The examine action that starts the factory's first scene, and what it starts.
enum {
    FACTORY_ACTION_START_SCENE    = 1,
    FACTORY_START_SCENE_CAP       = 11, // CAP command played
    FACTORY_START_SCENE_OBJECTIVE = 10, // Objective set
};

/// CAP commands, sequence slots and choices.
enum {
    FACTORY_GARAGE_REFUSAL_CAP_SLOT         = 4,  // Garage door refused: the lamp is not ready
    FACTORY_BREEZEWAY_REFUSAL_CAP_COMMAND   = 13, // Breezeway door refused: still locked
    FACTORY_DRIVEWAY_CAP_COMMAND            = 14, // Driveway door event
    FACTORY_POWER_CAP_ACCEPTED              = 3,  // Choice that switches the power on
    FACTORY_POWER_FOLLOWUP_VARIANT_KEY      = 2,  // Variant key of the scene that follows it
    FACTORY_BARRIER_TRANSITION_CAP_ACCEPTED = 1,  // Choice that starts the barrier transition
    FACTORY_BARRIER_TRANSITION_ROOM         = 2,  // Room variant the barrier transition reloads
};

/// Operator-panel captions.
enum {
    FACTORY_PANEL_CAPTION6             = 6,
    FACTORY_PANEL_CAPTION7             = 7,
    FACTORY_PANEL_CAPTION_CANNOT_RAISE = 8,
    FACTORY_PANEL_CAPTION_CANNOT_LOWER = 9,
    FACTORY_PANEL_CAPTION_CANNOT_TURN  = 10,
};

// ---------------------------------------------------------------------------
// Game-flag values

/// Bits of the lift position nibble, game flag
/// `GAME_FLAG_FACTORY_LIFT_POSITION`, and of the copy of it in
/// `FactoryLiftWork::position`. The operator panel requests a move by changing
/// a bit; the lift then moves until it stands where the nibble says.
enum {
    FACTORY_LIFT_POSITION_TURNED = 1 << 0, // Turned a quarter turn about Y
    FACTORY_LIFT_POSITION_RAISED = 1 << 1, // Raised
};

/// Values of `GAME_FLAG_FACTORY_LAMP_PROGRESS`.
enum {
    FACTORY_LAMP_PROGRESS_FIRST  = 1, // After the power scene
    FACTORY_LAMP_PROGRESS_SECOND = 2, // After the lamp scene or its sound command; the garage opens
};

/// Values of `GAME_FLAG_FACTORY_HATCH_OPEN`.
enum {
    FACTORY_HATCH_FLAG_CLOSED = 0,
    FACTORY_HATCH_FLAG_OPEN   = 1,
};

// ---------------------------------------------------------------------------
// Tasks

/// Slots of the room's spawn table (`gFactorySpawnTable`).
enum {
    FACTORY_SPAWN_POWER_SCENE              = 0,
    FACTORY_SPAWN_LAMP_SCENE               = 1,
    FACTORY_SPAWN_CAP_SCENE                = 2,
    FACTORY_SPAWN_BARRIER_TRANSITION_SCENE = 3,
    FACTORY_SPAWN_LIFT                     = 4,
    FACTORY_SPAWN_BARRIER_COLLISION        = 5,
    FACTORY_SPAWN_HATCH_SCENE              = 6,
    FACTORY_SPAWN_HATCH                    = 7,
};

/// Messages the operator panel task receives.
enum {
    /// The lift has finished the movement the panel asked for, whether it ran
    /// to its end or was skipped. Both argument words are zero and no result
    /// is defined.
    FACTORY_PANEL_MESSAGE_MOVE_SETTLED = 0x13F3,
};

// ---------------------------------------------------------------------------
// Views

/// Logical entries searched in each room map, independent of its storage capacity.
enum { FACTORY_LOGICAL_VIEW_COUNT = 19 };

/// Saved views used while scanning the operator panel and after leaving it.
enum {
    FACTORY_PANEL_VIEW_RETURN    = 3,
    FACTORY_PANEL_VIEW_POWERED   = 5,
    FACTORY_PANEL_VIEW_UNPOWERED = 12
};

/// Views the panel shows while the lift moves, with the barrier present or cleared.
enum {
    FACTORY_PANEL_MOVE_VIEW_BARRIER_PRESENT = 18,
    FACTORY_PANEL_MOVE_VIEW_BARRIER_CLEARED = 19,
};

/// Sprite batches shown once the power is on: the lit panel and the power scene's view.
enum {
    FACTORY_POWER_SPRITE_VIEW        = 9,
    FACTORY_POWER_SPRITE_BATCH       = 1,
    FACTORY_POWER_SCENE_SPRITE_VIEW  = 11,
    FACTORY_POWER_SCENE_SPRITE_BATCH = 1,
};

/// Logical views (bit n is view n, before camera/image remapping) that draw each glow.
enum {
    FACTORY_GLOW_POWER_VIEW_MASK = (1 << 3) | (1 << 5) | (1 << 6) | (1 << 12) | (1 << 14) | (1 << 16),
    FACTORY_GLOW_LAMP_VIEW_MASK  = (1 << 2) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10) | (1 << 13) | (1 << 16) | (1 << 17) | (1 << 18) | (1 << 19),
};

// ---------------------------------------------------------------------------
// Tuning

/// Lift position in the room, in world units.
enum {
    FACTORY_LIFT_ROOM_X = 3660,
    FACTORY_LIFT_ROOM_Z = 6830,
};

/// Raised lift endpoint as a signed 16.16 room-coordinate Y word.
///
/// The lowered endpoint is zero. Negative Y rises, so the raised height is
/// -570 units. Store/compare the full word in `FactoryLiftWork::y`; model
/// translation takes its signed integer half. Natural settling retains the
/// computed endpoint, while setup and START skip assign this exact value.
enum { FACTORY_LIFT_Y_RAISED_Q16 = -570 * 0x10000 };

/// Turned endpoint as a signed 16.16 yaw word, one quarter of a revolution.
///
/// The integer angle is 1024 in 4096 units per turn. Compare/store the full
/// word in FactoryLiftWork::yaw; frame updates pass only its signed integer
/// half to the SDK. Setup deliberately passes this full word to RotMatrixY,
/// where it masks to zero, retaining the binary's initial unturned frame.
enum { FACTORY_LIFT_YAW_TURNED_Q16 = 0x400 * 0x10000 };

/// Jam thresholds as 16.16 angles, with 4096 integer units per turn.
/// A lowered lift turns one thirty-second of a revolution before rebounding.
enum {
    FACTORY_LIFT_JAM_YAW_OUT  = 0x80 * 0x10000,
    FACTORY_LIFT_JAM_YAW_BACK = FACTORY_LIFT_YAW_TURNED_Q16 - FACTORY_LIFT_JAM_YAW_OUT
};

/// Inclusive elapsed-movement-frame threshold for the lift's START skip.
///
/// Port 0 must report a pressed START while the movement is in its sound,
/// outbound or settling phase. This gates both yaw and vertical handlers;
/// it is neither a remaining-time count nor the duration of the skip.
enum { FACTORY_LIFT_SKIP_FRAMES = 11 };

/// Hatch swing, as 16.16 angles about X: the open angle and the accelerations
/// and speed limits of the swings.
enum {
    FACTORY_HATCH_INITIAL_OPEN_ANGLE           = -768, // Integer angle the hatch starts at when open
    FACTORY_HATCH_OPEN_ANGLE_Q16               = -0x3000000,
    FACTORY_HATCH_OPEN_ACCELERATION_Q16        = -0x28000,
    FACTORY_HATCH_OPEN_MIN_VELOCITY_Q16        = -0x300000,
    FACTORY_HATCH_OPEN_SETTLE_ACCELERATION_Q16 = 0x40000,
    FACTORY_HATCH_OPEN_MAX_SETTLE_VELOCITY_Q16 = 0x100000,
    FACTORY_HATCH_CLOSE_ACCELERATION_Q16       = 0x20000,
    FACTORY_HATCH_CLOSE_MAX_VELOCITY_Q16       = 0x700000,
};

/// Frames the hatch scene waits with the hatch open and after it closes.
enum {
    FACTORY_HATCH_SCENE_OPEN_COUNTDOWN  = 60,
    FACTORY_HATCH_SCENE_CLOSE_COUNTDOWN = 30,
};

/// The barrier transition: fade length, and how far the cleared barrier's collision moves along X.
enum {
    FACTORY_BARRIER_TRANSITION_FADE_FRAMES = 30,
    FACTORY_BARRIER_CLEAR_X_OFFSET         = 2000,
};

/// Idle-task ticks of hotspot scanning delay after an operator-panel choice.
///
/// Armed after confirmation/cancellation and again after a lift move settles.
/// The idle state decrements before testing it, even while CAP is busy; CAP
/// playback independently keeps the cursor hidden when this count reaches zero.
enum { FACTORY_PANEL_SCAN_DELAY_FRAMES = 10 };

/// Updates before the room's interaction triggers re-arm after leaving the panel.
enum { FACTORY_PANEL_REENTRY_DELAY_UPDATES = 10 };

/// The glows over the power switch and the lamp.
enum {
    // Pixel radii are scale * 64 / (camera Z / 4 + 1).
    FACTORY_GLOW_POWER_RADIUS_SCALE = 0x100,
    FACTORY_GLOW_LAMP_RADIUS_SCALE  = 0x80,
    // High nibble is the odd-frame flicker shift; low three are RGB * 16.
    FACTORY_GLOW_POWER_COLOR       = (3 << 12) | (6 << 8) | (6 << 4),
    FACTORY_GLOW_LAMP_FIRST_COLOR  = (5 << 12) | (10 << 8),
    FACTORY_GLOW_LAMP_SECOND_COLOR = (5 << 12) | (10 << 4),
};

#endif
