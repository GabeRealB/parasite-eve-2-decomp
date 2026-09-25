#ifndef ROOMS_ROOM_H
#define ROOMS_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

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

/// The colour ramp of a room's screen-fade task: an 8-byte block the task
/// allocates for itself and keeps at `Task::work`. The task steps the three
/// channels together every frame, up to fade out or down to fade in, and
/// draws them as a full-screen overlay; its end test watches `r`.
typedef struct RoomFadeWork {
    byte pad_0[0x2];
    s16  r;
    s16  g;
    s16  b;
} RoomFadeWork;
STATIC_ASSERT_SIZEOF(RoomFadeWork, 0x8);

/// A room's command to its actors, the payload of message 0x7DB. A room sends
/// it to one actor directly, or as message 0x7DA to the actor manager in
/// pointer slot 4, which passes it on to its actors as 0x7DB. `from` says who
/// the command is from, usually the stage and area of the room sending it; a
/// receiver tests the two bytes together as one halfword before it acts on
/// `command`.
typedef struct RoomActorMsg {
    union {
        struct {
            u8 stage;
            u8 area;
        } loc;
        u16 key; // `loc` read as one halfword, area in the high byte
    } from;
    u16 command; // What the receiver is to do: a state to enter or a request number
} RoomActorMsg;
STATIC_ASSERT_SIZEOF(RoomActorMsg, 0x4);

/// What a room's cutscene runner plays: the record a room hands the runner
/// task as `Task::spawnArg2`. The runner forces the scene's view into the save
/// location, loads the capture file and starts the capture slot with the
/// scene's sound task beside it, lets the player cut it short, and restores
/// everything when it ends.
typedef struct RoomCutsceneRec {
    s8  field_0;  // Positive: the view forced into the save location for the scene; otherwise its negation is the view restored after
    s8  field_1;  // Capture slot the scene starts, which also picks the command run after it
    s8  field_2;  // Non-zero skips straight to the abort path
    s8  field_3;  // Capture file to load first; 0 for none
    s32 field_4;  // Sound event at the start
    s32 field_8;  // Sound event at the end
    s32 field_C;  // Sound event after a scene that was not skipped
    s32 field_10; // Sound the scene's sound task plays, and that task's spawn argument
    s16 field_14; // First of the pair handed on once the capture file is loaded; 0 selects 0x3C0 with a second of 0
    s16 field_16; // Second of that pair
} RoomCutsceneRec;
STATIC_ASSERT_SIZEOF(RoomCutsceneRec, 0x18);

/// One row of a shop's price ladder, a table of thirteen in the room's data.
/// The row's three items join the shop's stock once the row's bit is set in
/// `Mc_SaveData.shopTiers`. The rooms read only `items`; the leading word grows
/// row by row up to `S32_MAX` in the last, which reads as the spend that
/// unlocks the row, but nothing here confirms it.
typedef struct RoomShopTier {
    s32  spendThreshold;
    s16  items[3];
    byte pad_A[0x2];
} RoomShopTier;
STATIC_ASSERT_SIZEOF(RoomShopTier, 0xC);

/// Where a room sends the player when it moves them on, staged by the room
/// before it spawns its departure task. The room first runs `area`, `warp`
/// and `room` through its message handler, which may rewrite them. The task
/// then turns the player to `facing`, plays `sndEvent` and waits for it, and
/// finally commits the four location bytes as the save location and restarts
/// the player task.
typedef struct RoomDeparture {
    u8   stage;
    u8   area;
    u8   warp;
    u8   room;
    s16  facing;   // Heading sent to the player as message 0x3EE; -1 sends nothing
    byte pad_6[0x2];
    s32  sndEvent; // Sound event played before leaving; 0 for none
} RoomDeparture;
STATIC_ASSERT_SIZEOF(RoomDeparture, 0xC);

/// The scratchpad block a room's screen-ripple drawer takes from
/// `G_SCRATCH_HEAD` for one call. The drawer rotates each screen row's vector
/// `row` through the transposed view rotation `mtx` into `rowView`, and divides
/// `depth` by its height to find the row's ordering-table depth; `origin` is
/// the view translation brought into the same frame. Nothing reads the tail;
/// the block's size is how far the drawer moves the scratch head.
typedef struct RoomRippleScratch {
    MATRIX  mtx;
    SVECTOR row;
    SVECTOR rowView;
    SVECTOR origin;
    s32     depth;
    u8      _pad[0x10];
} RoomRippleScratch;
STATIC_ASSERT_SIZEOF(RoomRippleScratch, 0x4C);

#endif /* ROOMS_ROOM_H */
