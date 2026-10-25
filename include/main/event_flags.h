#ifndef MAIN_EVENT_FLAGS_H
#define MAIN_EVENT_FLAGS_H

/* Event flags: IDs of the persisted four-bit values read and written with
 * `gameFlagGetNibble` / `gameFlagSetNibble`.
 *
 * The game keeps 504 of them, packed two per byte in the checksummed bank saved
 * with the game (`GameFlagNibbleBank`): ID n is the high half of byte n / 2 for
 * even n, the low half for odd n. Two neighbouring IDs can also be accessed as
 * one byte (`gameFlagSetPackedByte`). The bank has no types and no stage; how a
 * flag is used - marker, state, counter, set of bits - is up to its owner.
 *
 * This file is the allocation map: every ID, in ID order, each with the place
 * that owns it. An ID is listed only once its meaning has been checked against
 * the code that reads and writes it.
 *
 * A flag's values are named by its owner, all in one place, at the smallest
 * scope that covers every reader and writer of the ID:
 *   - one room:                 `src/stages/<stage>/rooms/<room>/event_flags.h`
 *   - several rooms of a stage: `src/stages/<stage>/event_flags.h`
 *   - read outside the stage's rooms (an actor, the map, a later stage):
 *                               `include/stages/<stage>/event_flags.h`, owned by
 *                               the stage that writes it
 *   - a game-wide mechanism:    here, next to the ID
 * Plain markers (0 not yet, 1 happened) need no value names.
 *
 * Flags not checked yet are still in `main/gameflag_ids.h`.
 */

/// Values of a map door marker, shared by every `EVENT_FLAG_MAP_DOOR_*` ID.
///
/// The load screen writes PASSED when the player arrives through the door; a
/// room writes LOCKED when the door refuses them. The map menu shows a door by
/// its marker.
enum {
    EVENT_FLAG_MAP_DOOR_UNMARKED = 0, // Not passed and not refused yet
    EVENT_FLAG_MAP_DOOR_PASSED   = 1, // Arrived through the door (`areaStartRoomRuntime`)
    EVENT_FLAG_MAP_DOOR_LOCKED   = 2, // The door refused the player
};

/// Values of `EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER`. Playing the story call
/// increments it like any CAP counter; at story dialogue 0xE, `_roomCutsceneTask`
/// uses 1 and 2 for its looping choice instead.
enum {
    EVENT_FLAG_TELEPHONE_DIALOGUE_UNHEARD     = 0, // The current story call has not been played; reset at every story beat
    EVENT_FLAG_TELEPHONE_DIALOGUE_LOOP_ACTIVE = 1, // Dialogue 0xE: the looping choice is running
    EVENT_FLAG_TELEPHONE_DIALOGUE_LOOP_DONE   = 2, // Dialogue 0xE: the looping choice has been left
};

/// Values of `EVENT_FLAG_STORY_CHAPTER`, in story order.
enum {
    EVENT_FLAG_STORY_CHAPTER_MIST_ARRIVAL       = 0, // The start: the MIST arrival caption
    EVENT_FLAG_STORY_CHAPTER_AKROPOLIS          = 1, // From the Akropolis west elevator hall's scene
    EVENT_FLAG_STORY_CHAPTER_DRYFIELD           = 2, // From the Dryfield gas station's arrival
    EVENT_FLAG_STORY_CHAPTER_DRYFIELD_NIGHT     = 3, // From the night dilapidated house
    EVENT_FLAG_STORY_CHAPTER_DRYFIELD_NIGHT_END = 4, // The end of the Dryfield night (actor_136300's event)
    EVENT_FLAG_STORY_CHAPTER_SHELTER            = 5, // From the shelter B2 main corridor
    EVENT_FLAG_STORY_CHAPTER_SHELTER_1F_BULWARK = 6, // From the shelter 1F bulwark
};

/// Values of `EVENT_FLAG_CURRENT_OBJECTIVE`, in story order.
enum {
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_ENTERED                 = 0x01, // "Check the roof"; set as the west elevator hall's arrival starts
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_KEY_RECEIVED  = 0x02, // "Civilian evac."; set as the east elevator hall scene starts
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_BOSS_KILLED   = 0x03, // "I've never seen this kind of NMC."; after the cafeteria battle
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_CAFETERIA_BOSS_EXAMINED = 0x04, // "I have to contact MIST!"; after the cafeteria's follow-up
    EVENT_FLAG_OBJECTIVE_MIST_CONTACTED                    = 0x05, // "Warn Rupert"; set by the square's call
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_SANCTUARY_ENTERED       = 0x06, // "Find No. 9"; set as the sanctuary's arrival starts
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_ROOF_GARDEN_BOMB_SEEN   = 0x07, // "Bomb, how to escape"; the roof garden's and fire escape's dialogue
    EVENT_FLAG_OBJECTIVE_AKROPOLIS_NO9_DEFEATED            = 0x08, // "Follow No. 9"; after the helicopter landing pad's encounter
    EVENT_FLAG_OBJECTIVE_DRYFIELD_ARRIVED                  = 0x09, // "Fact finding"; set at the Dryfield gas station's arrival
    EVENT_FLAG_OBJECTIVE_MIST_SHOOTING_GALLERY_START       = 0x3A, // The first: the shooting gallery's introduction
    EVENT_FLAG_OBJECTIVE_MIST_SHOOTING_GALLERY_BRIEFED     = 0x3B, // "Akropolis"; after the gallery's control room scene
    EVENT_FLAG_OBJECTIVE_MIST_RETURNED                     = 0x3C, // "Go to the desert"; after returning to MIST
};

// The IDs come in blocks. The boundaries below are the ones seen so far; the
// gaps between listed IDs are still in `main/gameflag_ids.h`.
enum {
    // ---- Akropolis, from 0x000 ---------------------------------------------

    /// Akropolis route through the cafeteria. State: 1 the east elevator hall scene
    /// has played, 2 the cafeteria boss is dead and its scene has played, 3 Mist has
    /// been contacted (the square's telephone call, `_roomCutsceneTask`), 5 the
    /// fountain door is unlocked. The square refuses both elevators from 2; the
    /// patio, hallway, cafeteria, fountain and forked-road doors answer by it.
    EVENT_FLAG_AKROPOLIS_CAFETERIA_ROUTE_PROGRESS = 0x000,
    /// Akropolis route through the observatory to the sanctuary. State: 1 the
    /// observatory's lock has been found, 2 the security room's blue key has unlocked
    /// the escalator, 3 the forked road's scene to the observatory has played, 4 the
    /// promenade has been entered from the observatory, 5 the sanctuary has been
    /// reached. The security room's SWAT member (security_monitors) is gone from 3.
    EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS = 0x001,
    /// Akropolis bridge. State: 2 the sanctuary has been entered, 3 the bridge has
    /// been raised (the keypad movie). The promenade's bridge door is refused at 0;
    /// the promenade and the fire escape lead to the bridge's second state at 3.
    EVENT_FLAG_AKROPOLIS_BRIDGE_PROGRESS = 0x002,
    /// Telephone dialogue. Counter of the story call's CAP command (16 +
    /// `EVENT_FLAG_STORY_DIALOGUE_INDEX`), incremented each time the phone is used;
    /// it picks the call's variant, so a repeat plays a shorter one. Cleared whenever the story index changes - a
    /// missed call in the square, every story beat - so the next call starts over. At
    /// index 0xE, `_roomCutsceneTask` sets 1 while it loops the branching call and 2
    /// when the loop exits.
    EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER = 0x003,
    /// Akropolis square. Counter of the square's CAP command 4, the ammo box
    /// ("A SWAT ammunition box..."): the CAP interpreter keeps the command's
    /// counter here (doc/CAP_FORMAT.md §6), 0 before the box is examined, 1 after.
    EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_DIALOG_SEEN = 0x004,
    /// Akropolis sanctuary. Marker: Rupert has left - the roof garden has been entered,
    /// or the fire escape's door to the bridge used; the sanctuary then moves its cage
    /// out of the doorway and no longer arms Rupert's actor.
    EVENT_FLAG_AKROPOLIS_SANCTUARY_RUPERT_LEFT = 0x006,
    /// Akropolis sanctuary, the bomb scene. State: 1 it has played (room action),
    /// 2 it was passed by - the player left for the promenade, or reached the roof
    /// garden, first.
    EVENT_FLAG_AKROPOLIS_SANCTUARY_BOMB_SCENE_STATE = 0x007,
    /// Akropolis patio, its door to the cafeteria. State: 1 the player has found it
    /// locked (CAP 3), 2 the east elevator hall scene has unlocked it, 3 it has been
    /// opened and the warp to the cafeteria has started.
    EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_STATE = 0x008,
    /// Akropolis security-room panel. Bits: 0 the blue key (item 0x104) is inserted,
    /// which also sets `EVENT_FLAG_AKROPOLIS_SANCTUARY_ROUTE_PROGRESS` to 2; 1 the red key
    /// (item 0x103). Doors that lead to a room's second variant once a key is in:
    /// bit 0 the fountain/observatory and the square's forked-road door, bit 1 the
    /// hallway, patio and forked road.
    EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS = 0x009,
    /// Akropolis security-room monitor. Bits: 0 the SWAT member's scene has played
    /// (camera 0xB, CAP 0xD), 1 Rupert's (camera 0xA; security_monitors sets it).
    EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_SCENES_SEEN = 0x00A,
    /// Akropolis helicopter landing pad, the first of the three power transformers No. 9's
    /// fight can break: helicopter_landing_pad_no_9 writes its status each frame it is in
    /// view. State: `AKROPOLIS_HELIPAD_TRANSFORMER_*`.
    EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_1_STATE = 0x00B,
    /// Akropolis helicopter landing pad, the second transformer; as the first.
    EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_2_STATE = 0x00C,
    /// Akropolis helicopter landing pad, the third transformer and the blast source beside
    /// it, as one value written every frame of the fight. State:
    /// `AKROPOLIS_HELIPAD_TRANSFORMER_3_*`.
    EVENT_FLAG_AKROPOLIS_HELIPAD_TRANSFORMER_3_STATE = 0x00D,
    /// Akropolis, the report to MIST after the cafeteria boss. State: 1 the boss
    /// scene has played and queued the report (story dialogue 4, "A new species..."),
    /// 4 the report has been made from the square's telephone (`_roomCutsceneTask`).
    /// The cafeteria's patio door sets 2 when it holds 3, which nothing writes.
    EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT = 0x00E,
    /// Akropolis bridge. Marker: the scene at the fire escape's door has played - the
    /// first time the door is tried; it also means Rupert has left the sanctuary.
    EVENT_FLAG_AKROPOLIS_BRIDGE_FIRE_ESCAPE_DOOR_SCENE_SEEN = 0x010,
    /// Akropolis fountain. Marker: the hidden path has been found (room command 4,
    /// CAP 4): its climb trigger is enabled, and stays enabled on later entries.
    EVENT_FLAG_AKROPOLIS_FOUNTAIN_HIDDEN_PATH_FOUND = 0x012,
    /// Akropolis fountain. Marker: the door to the forked road has been opened once,
    /// from either side; the forked road plays CAP 1 step 3 the first time it is.
    EVENT_FLAG_AKROPOLIS_FOUNTAIN_FORKED_ROAD_DOOR_OPENED = 0x013,

    // ---- Map pages, 0x015..0x01A: one per map, read by the map menu ---------
    // 0x016 Dryfield, 0x017 Shelter mine and B1, 0x018 Shelter B2..B4,
    // 0x019 Neo Ark, 0x01A Shelter 1F.

    /// Marker: the Akropolis map can be shown. Written by the Akropolis square and
    /// the patio; read by the map menu.
    EVENT_FLAG_MAP_AKROPOLIS_AVAILABLE = 0x015,

    // ---- Akropolis, continued -----------------------------------------------

    /// Akropolis square. Marker: the telephone scene has played - the grinning
    /// stranger by the phone runs off and the phone rings.
    EVENT_FLAG_AKROPOLIS_SQUARE_TELEPHONE_SCENE_SEEN = 0x01F,

    /// Akropolis patio cutscene progress. State: 1 the square has been entered (from
    /// the west elevator hall), 3 the patio SWAT scene has played, 4 the patio scene
    /// has finished. Below 2, the square, fountain and cafeteria doors lead to the
    /// patio's first variant.
    EVENT_FLAG_AKROPOLIS_PATIO_CUTSCENE_PROGRESS = 0x021,
    /// Akropolis patio. Marker: Rupert's scene at the cafeteria door has played - the
    /// first time the door is tried after the cafeteria boss, before MIST is contacted;
    /// afterwards the door runs CAP 8.
    EVENT_FLAG_AKROPOLIS_PATIO_CAFETERIA_DOOR_SCENE_SEEN = 0x023,
    /// Akropolis observatory. Marker: the ride down to the forked road has played -
    /// the first time the observatory is left that way; later departures arrive by
    /// the stairs. The sanctuary sets it too.
    EVENT_FLAG_AKROPOLIS_OBSERVATORY_FORKED_ROAD_RIDE_SEEN = 0x025,
    /// Akropolis patio. Marker: the ambush scene has played - on the way back from the
    /// cafeteria; while clear, entering the patio in that variant sends placed actors 2 and 3 their command 0.
    EVENT_FLAG_AKROPOLIS_PATIO_AMBUSH_SCENE_SEEN = 0x026,
    /// Akropolis security-room monitor. Counter: the brightness detent (0..4) it was
    /// left at, saved on leaving it; it reopens at that level.
    EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_BRIGHTNESS = 0x02A,

    // ---- Not reserved for flags ---------------------------------------------

    /// The bank stores the captured play time (minutes, a halfword) in these four
    /// positions (`GameFlagNibbleBank::payload.state.playTimeMark`).
    EVENT_FLAG_RESERVED_PLAY_TIME_FIRST = 0x068,
    EVENT_FLAG_RESERVED_PLAY_TIME_LAST  = 0x06B,

    // ---- The story chapter, 0x07A -------------------------------------------

    /// The story chapter, set as each stage begins: what the boot captions, the music,
    /// room variants and cutscene choices go by. Values: `EVENT_FLAG_STORY_CHAPTER_*`.
    EVENT_FLAG_STORY_CHAPTER = 0x07A,

    // ---- The current objective, 0x0A2..0x0A3 --------------------------------

    /// The current objective, a whole byte (`gameFlagSetPackedByte`): what the map
    /// marks and what its Help panel says (stage-zero file 20800 + the objective).
    /// Values: `EVENT_FLAG_OBJECTIVE_*`.
    EVENT_FLAG_CURRENT_OBJECTIVE = 0x0A2,

    // ---- Akropolis ambushes, from 0x0CA -------------------------------------

    /// Akropolis observatory. Marker: the bat ambush has played - entering from the
    /// forked road after the red key; while clear, the forked road leads to the
    /// observatory's second state.
    EVENT_FLAG_AKROPOLIS_OBSERVATORY_BAT_AMBUSH_SEEN = 0x0CA,
    /// Akropolis roof garden, the enemy drop (placement variants 1 and 7). State: 1
    /// armed by its first trigger, 2 dropped by its second; from 2 the
    /// maggot/caterpillar is in its other placement.
    EVENT_FLAG_AKROPOLIS_ROOF_GARDEN_ENEMY_DROP_STATE = 0x0CB,
    /// Akropolis forked road. Marker: the ambush has played - back on the road after
    /// the red key (room action 1, placement variants 4 and 8); from then on the
    /// maggot/caterpillar is in its other placement.
    EVENT_FLAG_AKROPOLIS_FORKED_ROAD_AMBUSH_SEEN = 0x0CC,

    // ---- MIST ---------------------------------------------------------------

    /// MIST. Marker: Pierce has gone to the parking lot - set on leaving the firearms
    /// room (the parking lot's room action 2); the shooting gallery and Pierce's
    /// actors go by it.
    EVENT_FLAG_MIST_PIERCE_IN_PARKING_LOT = 0x0ED,
    /// MIST parking lot, the departure for Dryfield after Akropolis (placement variant
    /// 2). State: the arrival conversation and its follow-ups, step by step; values in
    /// the room's private header (`MIST_PARKING_CONVERSATION_*`). While nonzero, part of
    /// Pierce's model (parking_pierce) is hidden.
    EVENT_FLAG_MIST_PARKING_DRYFIELD_DEPARTURE_STATE = 0x0F1,
    /// MIST shooting gallery, Pierce's conversations (shooting_gallery_pierce). Counter:
    /// which talk plays next, `MIST_PIERCE_TALK_*`.
    EVENT_FLAG_MIST_PIERCE_TALK_PROGRESS = 0x0F5,

    // ---- From 0x100: not divided yet -----------------------------------------

    /// Akropolis security-room monitor. Marker: Rupert's scene on camera 0xA has
    /// played (security_monitors, after 0x5A frames); while clear, the camera starts CAP 0xC.
    EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_MONITOR_RUPERT_SCENE_SEEN = 0x102,
    /// MIST shooting gallery. Marker: the control room's scene has played (room
    /// action 4).
    EVENT_FLAG_MIST_SHOOTING_GALLERY_CONTROL_ROOM_SCENE_SEEN = 0x106,
    /// MIST shooting gallery. Marker: Pierce's introduction to the gallery has played
    /// (shooting_gallery_pierce sets it as the script starts, so skipping it counts too);
    /// afterwards talking to the gallery goes straight to its menu.
    EVENT_FLAG_MIST_SHOOTING_GALLERY_INTRO_SEEN = 0x121,
    /// Akropolis square. Marker: the hint by the ammo box has been shown.
    EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN = 0x124,
    /// MIST shooting gallery, a prize per course: levels 1..5 are FIRST..LAST, read as
    /// FIRST + the course. Values: `MIST_SHOOTING_GALLERY_PRIZE_*` (src/stages/akropolis/event_flags.h).
    EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_FIRST = 0x125,
    EVENT_FLAG_MIST_SHOOTING_GALLERY_PRIZE_LAST  = 0x129,
    /// Story dialogue. Index (0..0xF) of the telephone call to play next: the call is
    /// CAP command 16 + index in the call file (`_roomCutsceneTask`). Story beats in
    /// every stage raise it, each clearing `EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER`;
    /// a stage start resets it to 0; 0xE adds a looping choice. Each stage has its own
    /// call file, so its values are named per stage (`src/stages/akropolis/event_flags.h`).
    EVENT_FLAG_STORY_DIALOGUE_INDEX = 0x155,
    /// Akropolis square. Marker: the controls hint by the stairs has been shown.
    EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN = 0x156,
    /// Akropolis fire escape. Marker: the telephone has been inspected (room command
    /// 4, CAP 11); afterwards it plays the call's scene.
    EVENT_FLAG_AKROPOLIS_FIRE_ESCAPE_TELEPHONE_INSPECTED = 0x16A,

    // ---- Map door markers, about 0x1AC..0x1F7 --------------------------------
    // One sequential run per stage, the stages in reverse story order: Shelter and
    // Neo Ark from about 0x1AC, Dryfield from 0x1CE, Akropolis 0x1EA..0x1F7.
    // Values: `EVENT_FLAG_MAP_DOOR_*`.

    /// Door between the Akropolis forked road and the observatory. The security room
    /// unmarks it when the blue key unlocks the escalator.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_FORKED_ROAD_OBSERVATORY = 0x1EE,
    /// Door between the Akropolis forked road and the fountain.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_FORKED_ROAD_FOUNTAIN = 0x1EF,
    /// Door between the Akropolis patio and the fountain.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_PATIO_FOUNTAIN = 0x1F0,
    /// Door between the Akropolis hallway and the fountain.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_HALLWAY_FOUNTAIN = 0x1F1,
    /// Door between the Akropolis square and the east elevator hall.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_EAST_ELEVATOR = 0x1F6,
    /// Door between the Akropolis square and the west elevator hall.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_WEST_ELEVATOR = 0x1F7,
};

#endif // MAIN_EVENT_FLAGS_H
