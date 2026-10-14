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

// The IDs come in blocks. The boundaries below are the ones seen so far; the
// gaps between listed IDs are still in `main/gameflag_ids.h`.
enum {
    // ---- Akropolis, from 0x000 ---------------------------------------------

    /// Akropolis square. Counter of the square's CAP command 4, the ammo box
    /// ("A SWAT ammunition box..."): the CAP interpreter keeps the command's
    /// counter here (doc/CAP_FORMAT.md §6), 0 before the box is examined, 1 after.
    EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_DIALOG_SEEN = 0x004,

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

    // ---- Not reserved for flags ---------------------------------------------

    /// The bank stores the captured play time (minutes, a halfword) in these four
    /// positions (`GameFlagNibbleBank::payload.state.playTimeMark`).
    EVENT_FLAG_RESERVED_PLAY_TIME_FIRST = 0x068,
    EVENT_FLAG_RESERVED_PLAY_TIME_LAST  = 0x06B,

    // ---- From 0x100: not divided yet -----------------------------------------

    /// Akropolis square. Marker: the hint by the ammo box has been shown.
    EVENT_FLAG_AKROPOLIS_SQUARE_AMMO_BOX_HINT_SEEN = 0x124,
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
