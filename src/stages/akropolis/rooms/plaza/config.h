#ifndef STAGES_AKROPOLIS_ROOMS_PLAZA_CONFIG_H
#define STAGES_AKROPOLIS_ROOMS_PLAZA_CONFIG_H

// Configuration of the Akropolis plaza.

#include "main/areas.h"
#include "main/sound_ids.h"
#include "main/sound_types.h"

#include "gameplay/message.h"
#include "../../room_ids.h"

// ---------------------------------------------------------------------------
// Sounds

/// A sound of the plaza's area bank.
#define AKROPOLIS_PLAZA_SOUND(cue) SOUND_AREA(GAME_STAGE_AKROPOLIS, AKROPOLIS_ROOM_ID_PLAZA, (cue))

/// Entries of the plaza's area bank.
enum {
    AKROPOLIS_PLAZA_SOUND_OFFICER_AMBIENCE   = 1, // Under the traversal's officer shots
    AKROPOLIS_PLAZA_SOUND_ENTRY_AMBIENCE     = 2, // The traversal's opening, faded out early
    AKROPOLIS_PLAZA_SOUND_ENTERING_TOWER_CUE = 3, // The final scene's cue
    AKROPOLIS_PLAZA_SOUND_APPROACH_AMBIENCE  = 4, // The traversal's approach
    AKROPOLIS_PLAZA_SOUND_WALK_AMBIENCE      = 5, // The whole traversal
    AKROPOLIS_PLAZA_SOUND_POLICE_STEP_1      = 6, // An officer's step, on his clip's second cue
    AKROPOLIS_PLAZA_SOUND_POLICE_STEP_2      = 7, // An officer's step, on his clip's first cue
    AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_1      = 8, // The rookie's steps in turn; unreachable (police_officers.c)
    AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_2      = 9,
    AKROPOLIS_PLAZA_SOUND_ROOKIE_STEP_3      = 10,
    AKROPOLIS_PLAZA_SOUND_OFFICERS_WALK_IN   = 11, // The officers walk in, in the final scene
};

/// Where in the traversal each ambience plays, in its frames: the walk ambience over the whole street, the entry's
/// fading out by frame 130 (more steeply after 84), the approach's and the officers' rising to a peak and falling away.
/// In the helicopter wreck's movie, the officers' ambience rises to frame 30, holds over 54 frames from 31, then falls
/// toward frame 115, over a 120-frame ramp.
enum {
    AKROPOLIS_PLAZA_AMBIENCE_TRAVERSAL_END_FRAME     = 800,
    AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_KNEE         = 84,
    AKROPOLIS_PLAZA_AMBIENCE_EARLY_FADE_END          = 130,
    AKROPOLIS_PLAZA_AMBIENCE_APPROACH_FIRST_FRAME    = 15,
    AKROPOLIS_PLAZA_AMBIENCE_APPROACH_PEAK_FRAME     = 80,
    AKROPOLIS_PLAZA_AMBIENCE_APPROACH_LAST_FRAME     = 142,
    AKROPOLIS_PLAZA_AMBIENCE_OFFICER_FIRST_FRAME     = 200,
    AKROPOLIS_PLAZA_AMBIENCE_OFFICER_PEAK_FRAME      = 320,
    AKROPOLIS_PLAZA_AMBIENCE_OFFICER_LAST_FRAME      = 370,
    AKROPOLIS_PLAZA_AMBIENCE_WRECK_FULL_FIRST_FRAME  = 31,
    AKROPOLIS_PLAZA_AMBIENCE_WRECK_FULL_FRAME_COUNT  = 54,
    AKROPOLIS_PLAZA_AMBIENCE_WRECK_RISE_END_FRAME    = 30,
    AKROPOLIS_PLAZA_AMBIENCE_WRECK_FALL_ANCHOR_FRAME = 115,
    AKROPOLIS_PLAZA_AMBIENCE_WRECK_RAMP_FRAMES       = 120,
};

// ---------------------------------------------------------------------------
// Movies

/// The plaza's movies, selected by `CdCmdQueue::plazaStreamSubId`: the backgrounds the player walks through. The
/// traversal plays one forward and one reverse, and seeks back and forth in them as the player walks; each scene has
/// its own.
enum {
    AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_FORWARD     = 0,
    AKROPOLIS_PLAZA_MOVIE_TRAVERSAL_REVERSE     = 1,
    AKROPOLIS_PLAZA_MOVIE_HELICOPTER_WRECK      = 2,
    AKROPOLIS_PLAZA_MOVIE_OFFICERS_TALK         = 3,
    AKROPOLIS_PLAZA_MOVIE_ENTRANCE_OFFICER_TALK = 4,
    AKROPOLIS_PLAZA_MOVIE_ENTERING_TOWER        = 5,
    AKROPOLIS_PLAZA_MOVIE_OPENING               = 100, // Selected as the location's view, not by sub-id
};

// ---------------------------------------------------------------------------
// Triggers and scenes

/// Event kinds of the plaza sequence: `WorldCollisionTrigger::parameter0` of the room's action triggers, latched in
/// the sequence work's `eventKind` (plaza.c).
enum {
    AKROPOLIS_PLAZA_EVENT_HELICOPTER_WRECK = 0, // Once
    AKROPOLIS_PLAZA_EVENT_SERGEANT         = 1, // Once, and before every other scene
    AKROPOLIS_PLAZA_EVENT_ENTRANCE         = 2, // Ends the sequence: the player enters the tower
    AKROPOLIS_PLAZA_EVENT_SERGEANT_AGAIN   = 3, // Every visit to the sergeant after the first
    AKROPOLIS_PLAZA_EVENT_FIRST_CAPTION    = 6  // This kind and every higher one is the CAP to run
};

/// Where the scenes walk the player to, in world units, and which way it turns there: the sergeant's and the
/// helicopter's scenes face 5/6 of a turn, and the player leaves the wreck facing 11/12.
enum {
    AKROPOLIS_PLAZA_SERGEANT_SPOT_X         = 14340,
    AKROPOLIS_PLAZA_SERGEANT_SPOT_Z         = 4040,
    AKROPOLIS_PLAZA_HELICOPTER_SPOT_X       = 3950,
    AKROPOLIS_PLAZA_HELICOPTER_SPOT_Z       = 9000,
    AKROPOLIS_PLAZA_ENTRANCE_SPOT_X         = 990, // Before the officer at the entrance, and then inside
    AKROPOLIS_PLAZA_ENTRANCE_SPOT_Z         = 13310,
    AKROPOLIS_PLAZA_INSIDE_SPOT_Z           = 17310,
    AKROPOLIS_PLAZA_SCENE_FACING            = ACTOR_TRANSFORM_ANGLE_TURN * 5 / 6,
    AKROPOLIS_PLAZA_FACING_AFTER_HELICOPTER = ACTOR_TRANSFORM_ANGLE_TURN * 11 / 12,
};

/// The scenes' timing against their movies, in frames: the frame the player rejoins the street after the helicopter
/// wreck; entering the tower's cue and fade out, and the officers' talk's fade in.
enum {
    AKROPOLIS_PLAZA_HELICOPTER_WRECK_REJOIN_FRAME = 96,
    AKROPOLIS_PLAZA_ENTRANCE_CUE_FRAME            = 30,
    AKROPOLIS_PLAZA_ENTRANCE_FADE_OUT_FRAME       = 61,
    AKROPOLIS_PLAZA_ENTRANCE_FADE_IN_FRAME        = 11,
};

// ---------------------------------------------------------------------------
// Departure

/// Where the reload goes after the player enters the tower: the west elevator hall, through the warp its arrival
/// scene comes in by (the hall's `AKROPOLIS_WEST_ELEVATOR_HALL_ARRIVAL_WARP`), in room state 1, with sprite variant 1.
enum {
    AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_WARP   = 1,
    AKROPOLIS_PLAZA_ELEVATOR_ARRIVAL_ROOM   = 1,
    AKROPOLIS_PLAZA_ELEVATOR_SPRITE_VARIANT = 1,
};

// ---------------------------------------------------------------------------
// Lights

/// The traversal's ambient light, on every channel.
enum { AKROPOLIS_PLAZA_TRAVERSAL_AMBIENT_LEVEL = 0x370 };

/// Which markers of `_gAkropolisPlazaAmbientLightPlacements` carry which light: the siren beams, the rotating
/// flares and the glows, each blue (yellow, for the glows) up to its first red one. Markers 0 and 11 carry none.
enum {
    AKROPOLIS_PLAZA_LIGHT_FIRST_SIREN     = 1,
    AKROPOLIS_PLAZA_LIGHT_FIRST_RED_SIREN = 5,
    AKROPOLIS_PLAZA_LIGHT_SIREN_END       = 7,
    AKROPOLIS_PLAZA_LIGHT_FIRST_FLARE     = 7,
    AKROPOLIS_PLAZA_LIGHT_FIRST_RED_FLARE = 9,
    AKROPOLIS_PLAZA_LIGHT_FLARE_END       = 11,
    AKROPOLIS_PLAZA_LIGHT_FIRST_GLOW      = 12,
    AKROPOLIS_PLAZA_LIGHT_FIRST_RED_GLOW  = 16,
};

#endif
