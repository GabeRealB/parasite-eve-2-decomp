#ifndef SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H

#include "types.h"

#include "gameplay/collision.h"
#include "gameplay/direction.h"
#include "gameplay/message.h"
#include "gameplay/room.h"

#include "main/task_types.h"

/// The island's collision mesh and 4-by-5 XZ cell index.
///
/// Borrows this overlay's 13 normals, 127 vertices, 61 faces and 20 cell lists.
/// Cells are 4000 game-coordinate units wide, from origin (-5100, -7000)
/// on X/Z. Face-index lists end at WORLD_COLLISION_GRID_CELL_END.
/// Activation binds the mutable grid to the view; keep this overlay loaded while active.
extern WorldCollisionGrid gNeoArkIslandCollisionGrid;

/// Four directional lights shared by every island view, with no point or cone lights.
///
/// Borrows this overlay's mutable light array; coordinate setup and shading
/// update transforms and attenuation. Keep this overlay loaded while used.
extern WorldCoordRoomLights gNeoArkIslandRoomLights;

/// Directed view boundaries connecting island views 2 and 3, then 3 and 4.
///
/// Each direction has its own quad, and the fourth record carries LAST.
/// Room setup binds, links and enables these mutable nodes; they must stay
/// loaded until unlinked. Hit consumption checks the source view before
/// requesting the destination. Positions use room units and normals use 4096 per unit.
extern WorldCollisionTrigger gNeoArkIslandViewBoundaryTriggers[4];

/// Island interactions: pavilion warp, gallery warp and facing-gated CAP command 2.
///
/// Warp bytes select departure/arrival slots 1/3 for the pavilion and 2/1
/// for the gallery; the room defers gallery departure to its prompt. The last
/// record starts CAP command 2 and carries LAST. Room setup binds, links and
/// enables these mutable nodes; keep them loaded until unlinked. Positions
/// use room units and normals use 4096 per unit.
extern WorldCollisionTrigger gNeoArkIslandActionTriggers[3];

/// Island room-message dispatch: transitions, item refusals, actions, commands and sound.
///
/// Five id-selected callbacks followed by TASK_MESSAGE_TABLE_END. The room
/// controller borrows this table; keep the overlay loaded while it receives messages.
extern TaskMessageEntry gNeoArkIslandRoomMessages[6];

/// Bodyless gallery-departure prompt recipe, priority `TASK_PRIORITY_SCENE` and zero metadata.
///
/// One descriptor with no terminator. Spawn at index 0 with zero arguments
/// after staging a destination and holding player control. The prompt borrows
/// the shared destination and room CAP resources through completion.
extern TaskDesc gNeoArkIslandGalleryDepartureTaskDesc;

// Callbacks referenced by the overlay's shared data tables.

/// Prompts for departure to the Submarine Gallery and commits an accepted destination.
///
/// Start in state 0 after staging the resolved area/warp/room and holding player
/// control. CAP reply key 10 resumes actors, waits one additional update, stops
/// non-ambient sounds and requests a saved-state reload with frame capture.
/// Other replies release the task and resume player control. Requires the room
/// CAP resources and overlay throughout the sequence; commits only those three
/// location bytes, preserving stage and view. Only one staged departure may
/// be pending at a time.
void neoArkIslandGalleryDepartureTask(Task* task);

/// Refuses key-item use on the island without changing room state.
///
/// Installed for `ROOM_MESSAGE_USE_KEY_ITEM`. All arguments are ignored;
/// returns `ROOM_KEY_ITEM_USE_REFUSED` so the item menu shows its refusal.
s32 neoArkIslandRejectKeyItemMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedItemId, s32 unusedSecondArg);

/// Resolves a destination and defers gallery departure to the island's CAP prompt.
///
/// Borrows complete eight-byte request and writable reply records, which may
/// alias. Copies the request before consulting the loaded Neo Ark map overlay.
/// For the Submarine Gallery, execution stages the resolved area/warp/room,
/// holds player control and starts the departure task; queries do none of these.
/// Both return zero to suppress immediate transition, even on spawn failure.
/// Other destinations return 1. Retains no request/reply pointer. A later
/// execution overwrites the shared staged destination of an earlier departure.
s32 neoArkIslandResolveRoomTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Ignores island CAP room commands and returns zero.
///
/// Installed for `ROOM_MESSAGE_COMMAND`; `unusedCommandId` is the CAP command
/// selector. All arguments are ignored and no room state changes.
s32 neoArkIslandIgnoreCommandMessage(Task* unusedTask, s32 unusedMessageId, s32 unusedCommandId, s32 unusedSecondArg);

/// Ignores the island's direction-trigger action requests and returns zero.
///
/// Installed for `DIRECTION_MESSAGE_ROOM_ACTION`. All arguments are ignored;
/// the borrowed request is neither accessed nor retained and no event starts.
s32 neoArkIslandIgnoreActionMessage(Task* unusedTask, s32 unusedMessageId, const DirectionActionRequest* unusedRequest, s32 unusedSecondArg);

/// Maps island CAP sound cues to scripts in the island's room bank.
///
/// Installed for `ROOM_MESSAGE_SOUND`. Cue 3 plays bank entry 3; cue 101 plays
/// entry 4 only when the current CAP variant key is zero. Other cues do nothing.
/// Always returns zero; the remaining arguments are ignored. Queues sounds
/// with zero pan and depth while the room is loaded.
s32 neoArkIslandSoundMessage(Task* unusedTask, s32 unusedMessageId, s32 cueKey, s32 unusedSecondArg);

#endif // SRC_ROOMS_NEO_ARK_ISLAND_NEO_ARK_ISLAND_PRIVATE_H
