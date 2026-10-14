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
    /// Akropolis security-room panel. Bits: 0 the blue key (item 0x104) is inserted,
    /// which also sets `GAME_FLAG_OBSERVATORY_ROUTE_PROGRESS` to 2; 1 the red key
    /// (item 0x103). Doors that lead to a room's second variant once a key is in:
    /// bit 0 the fountain/observatory and the square's forked-road door, bit 1 the
    /// hallway, patio and forked road.
    EVENT_FLAG_AKROPOLIS_SECURITY_ROOM_PANEL_KEYS = 0x009,
    /// Akropolis, the report to MIST after the cafeteria boss. State: 1 the boss
    /// scene has played and queued the report (story dialogue 4, "A new species..."),
    /// 4 the report has been made from the square's telephone (`_roomCutsceneTask`).
    /// The cafeteria's patio door sets 2 when it holds 3, which nothing writes.
    EVENT_FLAG_AKROPOLIS_CAFETERIA_BOSS_REPORT = 0x00E,

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

    // ---- Not reserved for flags ---------------------------------------------

    /// The bank stores the captured play time (minutes, a halfword) in these four
    /// positions (`GameFlagNibbleBank::payload.state.playTimeMark`).
    EVENT_FLAG_RESERVED_PLAY_TIME_FIRST = 0x068,
    EVENT_FLAG_RESERVED_PLAY_TIME_LAST  = 0x06B,

    // ---- From 0x100: not divided yet -----------------------------------------

    /// Akropolis square. Marker: the hint by the ammo box has been shown.
    EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN = 0x124,
    /// Story dialogue. Index (0..0xF) of the telephone call to play next: the call is
    /// CAP command 16 + index in the call file (`_roomCutsceneTask`). Story beats in
    /// every stage raise it, each clearing `EVENT_FLAG_TELEPHONE_DIALOGUE_COUNTER`;
    /// a stage start resets it to 0; 0xE adds a looping choice. Each stage has its own
    /// call file, so its values are named per stage (`src/stages/akropolis/event_flags.h`).
    EVENT_FLAG_STORY_DIALOGUE_INDEX = 0x155,
    /// Akropolis square. Marker: the controls hint by the stairs has been shown.
    EVENT_FLAG_AKROPOLIS_SQUARE_STAIRS_HINT_SEEN = 0x156,

    // ---- Map door markers, about 0x1AC..0x1F7 --------------------------------
    // One sequential run per stage, the stages in reverse story order: Shelter and
    // Neo Ark from about 0x1AC, Dryfield from 0x1CE, Akropolis 0x1EA..0x1F7.
    // Values: `EVENT_FLAG_MAP_DOOR_*`.

    /// Door between the Akropolis square and the east elevator hall.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_EAST_ELEVATOR = 0x1F6,
    /// Door between the Akropolis square and the west elevator hall.
    EVENT_FLAG_MAP_DOOR_AKROPOLIS_SQUARE_WEST_ELEVATOR = 0x1F7,
};

#endif // MAIN_EVENT_FLAGS_H
