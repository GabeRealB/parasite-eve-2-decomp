#ifndef SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/message.h"
#include "gameplay/room.h"
#include "gameplay/sprites.h"

#include "main/task_types.h"

/// Three bodyless priority-192 recipes for the warehouse cutscene and its fades.
///
/// Entry 0 runs the cutscene with zero payloads; entries 1/2 fade out/in with
/// the intensity step in spawnArg1 and zero spawnArg2. Direct spawns index
/// these entries without a terminator. Keep the room loaded through completion.
extern TaskDesc gDryfieldWarehouseCutsceneTaskDescs[3];

/// Four logical-view boundaries shared by warehouse rooms 1..3.
///
/// The quads pair transitions 2/3 and 3/4; the fourth includes
/// `WORLD_COLLISION_TRIGGER_LAST`. Positions/radii use game coordinates and
/// normals use 4096 per unit. Room setup binds and links these mutable records;
/// keep the overlay loaded until they are unlinked.
extern WorldCollisionTrigger gDryfieldWarehouseViewBoundaryTriggers[4];

/// Thirteen action quads for warehouse room 1 and the third configured room.
///
/// Two warps, eight CAP interactions and three Monkey Wrench event regions;
/// the final event region includes `WORLD_COLLISION_TRIGGER_LAST`. The quads
/// use game coordinates and 4096-unit normals. Room setup borrows and mutates
/// their links, transforms, flags and hit latches while the overlay is loaded.
extern WorldCollisionTrigger gDryfieldWarehouseBeforeEventActionTriggers[13];

/// Ten action quads for warehouse room 2 after the Monkey Wrench event.
///
/// Two warps and eight CAP interactions replace the earlier list without its
/// event regions; the final CAP quad includes `WORLD_COLLISION_TRIGGER_LAST`.
/// Geometry uses game coordinates and 4096-unit normals. Keep these mutable
/// borrowed records loaded until room setup unlinks them.
extern WorldCollisionTrigger gDryfieldWarehouseAfterEventActionTriggers[10];

/// Single point-light collection shared by warehouse rooms 1..3.
///
/// Contains six lights active in every logical view, with no directional or
/// cone lights. Room lighting records borrow this collection and its mutable
/// point-light array while the room overlay is loaded.
extern WorldCoordRoomLights gDryfieldWarehouseLights;

/// Published warehouse cutscene task whose work is borrowed by script callbacks.
///
/// Set after successful cutscene-work allocation and initialization. Command
/// posting, player restoration and skip callbacks require that task/work to
/// remain live. The event gate releases the task on its requested exit; this
/// pointer is retained afterwards and is invalid outside the active cutscene.
extern Task* gDryfieldWarehouseCutsceneTask;

/// Borrowed handle to the warehouse cutscene's outstanding fade-out task.
///
/// Reset at cutscene initialization, set when a fade-out is spawned, and
/// cleared when that fade completes normally. The skip callback kills a
/// non-NULL handle without clearing it; fade-work allocation failure also leaves
/// the recorded handle unchanged. Use only within the cutscene's script lifetime.
extern Task* gDryfieldWarehouseFadeOutTask;

/// Whether skip restoration changed the saved warehouse room to room 2 (0/1).
///
/// Phase 0 overwrites this latch on every skip; phase 1 uses it one script tick
/// later to refresh the view and room objects. It is not the session room ID.
/// Storage remains a signed halfword.
extern s16 gDryfieldWarehouseSkipRoomChanged;

extern TaskMessageEntry D_dryfield_warehouse_8017F554[3];

extern TaskDesc D_dryfield_warehouse_8017F56C[2];

/// Empty sprite list for mapped view 1, retaining decoded background strips.
///
/// A zero-count leading batch is followed by `SPRITE_BATCH_END`; the view
/// retains this list's address in its empty source slot without reading sprites.
extern SpriteBatch gDryfieldWarehouseView1SpriteBatches[2];

/// Foreground sprite rectangles and sorting depths for mapped view 2.
///
/// All 21 elements are selected by `gDryfieldWarehouseView2SpriteBatches`.
/// Rectangles use centre-relative screen pixels and texture-page texels;
/// depths are unscaled ordering values. The room owns this mutable storage.
extern SpriteSource gDryfieldWarehouseView2SpriteSources[21];

/// Foreground ranges for mapped view 2, with the decoded background retained.
///
/// Four ranges partition its 21 sources as 2, 2, 13 and 4 elements.
/// The leading zero-count batch selects background strips; the last record
/// is `SPRITE_BATCH_END`. All ranges initially participate and are visible.
extern SpriteBatch gDryfieldWarehouseView2SpriteBatches[6];

/// Foreground sprite rectangles and sorting depths shared by mapped views 3 and 6.
///
/// Both variants of logical view 3 select the same 25 sources and batches;
/// only their loaded view resources differ. Rectangles use centre-relative
/// screen pixels and texture-page texels; depths are unscaled ordering values.
/// The room owns this mutable storage.
extern SpriteSource gDryfieldWarehouseView3And6SpriteSources[25];

/// Foreground ranges shared by mapped views 3 and 6, retaining background strips.
///
/// Four ranges partition their 25 shared sources as 2, 1, 17 and 5 elements.
/// The leading zero-count batch selects background strips; the last record
/// is `SPRITE_BATCH_END`. All ranges initially participate and are visible.
extern SpriteBatch gDryfieldWarehouseView3And6SpriteBatches[6];

/// Foreground sprite rectangles and sorting depths shared by mapped views 4 and 7.
///
/// Both variants of logical view 4 select the same two sources and batches;
/// only their loaded view resources differ. Rectangles use centre-relative
/// screen pixels and texture-page texels; depths are unscaled ordering values.
/// The room owns this mutable storage.
extern SpriteSource gDryfieldWarehouseView4And7SpriteSources[2];

/// One two-sprite range shared by mapped views 4 and 7, retaining background strips.
///
/// The leading zero-count batch selects background strips; the last record
/// is `SPRITE_BATCH_END`. The source range initially participates and is visible.
extern SpriteBatch gDryfieldWarehouseView4And7SpriteBatches[3];

/// Empty sprite list for mapped view 5, retaining decoded background strips.
///
/// A zero-count leading batch is followed by `SPRITE_BATCH_END`; the view
/// retains this list's address in its empty source slot without reading sprites.
extern SpriteBatch gDryfieldWarehouseView5SpriteBatches[2];

// Callbacks referenced by the overlay's shared data tables.
/// Adjusts warehouse ambience to the current view while room events are idle.
///
/// State 0 clears the requested-volume cache and enters state 1; other states
/// do nothing. State 1 requests 50%, 60% or 100% for views 2, 3 or 4, and silence
/// elsewhere or during an event. Changes queue a start, mix or 30-audio-update
/// stop; the cache records the request even when sound admission fails.
/// Requires a live task and loaded warehouse sound resources while playback runs.
void dryfieldWarehouseAmbienceTask(Task* task);

/// Starts the warehouse event when the Monkey Wrench is used at its active trigger.
///
/// `itemId` is a collected-item ID from `ROOM_MESSAGE_USE_KEY_ITEM`; the second
/// payload and other callback arguments are ignored. Scans the live trigger
/// list for a hit room-action trigger with the room-event sentinel. A match
/// sets the seen flag and session event state, queues the event task and returns
/// `ROOM_KEY_ITEM_USE_SHOW_USED_NOTICE`, even if spawning fails. Other requests
/// return `ROOM_KEY_ITEM_USE_REFUSED`. No inventory item is removed here.
s32 dryfieldWarehouseUseKeyItem(Task* task, s32 messageId, s32 itemId, s32 unusedSecondArg);

/// Resolves warehouse departures, blocking the house route until the event is seen.
///
/// Borrows a readable eight-byte `request` and writable eight-byte `reply` for
/// synchronous `ROOM_EVENT_MESSAGE_RESOLVE`; they may alias. Copies the whole
/// record before checking it. The dilapidated-house route returns 1 once the
/// event is seen; before that it returns 0, and execution additionally starts
/// CAP command 3 and sets the optional map flag to 2. Other destinations return
/// 1 and execution requests a 15-audio-update ambience stop. Queries suppress
/// these effects. Neither pointer is retained and no destination is changed.
s32 dryfieldWarehouseResolveRoomEvent(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

/// Starts the warehouse event cutscene and waits for its requested exit.
///
/// States 0..2 start, wait and exit. State 0 disables display and publishes the
/// new cutscene task; state 1 polls its requested exit and ignores its result;
/// state 2 releases this gate. The cutscene owns display restoration and its
/// work. Requires a successful child spawn before polling and the room overlay
/// to remain loaded. This gate does not allocate work or adopt the child task.
void dryfieldWarehouseEventTask(Task* task);

#endif // SRC_ROOMS_DRYFIELD_WAREHOUSE_DRYFIELD_WAREHOUSE_PRIVATE_H
