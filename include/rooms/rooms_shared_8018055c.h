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

/// The ten course lists the SELECT menu can offer, indexed by
/// `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.gameMode` (the difficulty the save runs at) plus 5 outside a
/// debug attach. The row handler stack-copies the whole table like the other
/// gallery tables, but draws its row through the same 0x28-byte block: the
/// text request overlays the first four list pointers, which is safe because
/// the row is looked up before the request is filled in.
typedef union RoomsShared8018055cMenu {
    /* 0x00 */ JukeboxTrack* lists[10];
    /* 0x00 */ TextDrawReq   req;
} RoomsShared8018055cMenu;
STATIC_ASSERT_SIZEOF(RoomsShared8018055cMenu, 0x28);

#endif // INCLUDE_ROOMS_ROOMS_SHARED_8018055C_H
