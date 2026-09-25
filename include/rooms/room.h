#ifndef ROOMS_ROOM_H
#define ROOMS_ROOM_H

#include "common.h"

/// A scripted event a room starts in answer to a message. The room's message
/// handler builds the record, and if the event has not happened yet it copies
/// the record into the room's own pending copy and spawns the room's event
/// task, which works through it: it runs the capture command, starts the fade
/// task if asked to once the capture has finished, plays the stage sound and
/// waits for it, and then moves the player on.
typedef struct RoomLatchedEvent {
    s32 capCmd;   // Capture command the event task runs first
    s32 stageSnd; // Packed stage sound played once the capture has finished; 0 for none
    s16 flagId;   // Game-flag nibble that records the event as done, set when it starts; 0 for none
    u8  fade;     // Non-zero starts the fade task (bank 1, type 0x31) over 30 frames once the capture has finished
} RoomLatchedEvent;
STATIC_ASSERT_SIZEOF(RoomLatchedEvent, 0xC);

#endif /* ROOMS_ROOM_H */
