#ifndef INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H
#define INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H

#include "common.h"

#include "main/text.h"

/// One row of a jukebox SELECT list: the sequence to play and the label drawn
/// for that row.
///
/// `sequenceId` is the MIDI sequence id. The menu task passes that id as the
/// file index when it loads the sequence from the global library (file group 4,
/// stage 0) and then starts it. `name` includes the row's place in its list
/// ("1. Crazy King"); the same sequence is labeled differently in other lists.
typedef struct {
    s32         sequenceId; // MIDI sequence id; file index used to load that sequence (file group 4)
    const char* name;       // Label drawn for the row, including its list number ("1. Crazy King")
} JukeboxTrack;
STATIC_ASSERT_SIZEOF(JukeboxTrack, 0x8);

/// The jukebox's ten SELECT track lists, and the row callback's stack copy of
/// them, whose storage then carries the row's text request.
///
/// Lists 0-4 are offered in the debug attach room and 5-9 everywhere else;
/// within each half the save's `gameMode` picks the list, and list 4 of the
/// half stands in before the first clear. The row callback copies the whole
/// table to the stack, looks its row up, and only then fills in `req` over the
/// first four list pointers.
typedef union {
    JukeboxTrack* lists[10]; // Track list per game mode: 0-4 debug attach room, 5-9 otherwise
    TextDrawReq   req;       // Row label request, built after the row is looked up
} JukeboxTrackLists;
STATIC_ASSERT_SIZEOF(JukeboxTrackLists, 0x28);

#endif // INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H
