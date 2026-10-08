#ifndef SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/evs.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern ActorCommand gRoamerCommand;

extern u16 gRoamerReserveHp[5];

extern TaskDesc D_neo_ark_forest_zone_80181DBC;

extern TaskMessageEntry D_neo_ark_forest_zone_80181DC8[6];

extern s32 D_neo_ark_forest_zone_80181E30;

extern s32 D_neo_ark_forest_zone_80181E38;

extern Task* D_neo_ark_forest_zone_80181E68;

extern EvsCommand D_neo_ark_forest_zone_80181E6C[23];

extern TaskDesc D_neo_ark_forest_zone_80182E18;

// Callbacks referenced by the overlay's shared data tables.

/// Key-item use request from the inventory menu; its first argument is an item ID.
enum { NEO_ARK_FOREST_ZONE_MESSAGE_USE_KEY_ITEM = 0x13F1 };

/// Refuses every key-item use in the forest zone.
///
/// Returns zero, which the key-item menu reports as unusable. All arguments
/// are ignored; no item is consumed and no message payload is retained.
s32 neoArkForestZoneRejectKeyItemMessage(Task* task, s32 messageId, s32 itemId, s32 secondArg);

/// Resolves room selection and gates the first departure scene to the woodland path.
///
/// Copies the complete borrowed request to the writable reply, which may alias it.
/// Execute requests queue a forest-ambience stop with control 60, for every area.
/// For the woodland destination, returns 1 if its scene flag is already set or 2
/// when eligible, including queries. Execution latches the resolved transition and
/// CAP 2/sound event, sets the scene flag and starts the deferred event controller.
/// Other destinations return 1. Deferred copies and resources need the room loaded.
s32 neoArkForestZoneResolveTransition(Task* unusedTask, s32 unusedMessageId, RoomEventMsg* request, RoomEventMsg* reply);

/// Ignores forest-zone room commands and returns zero.
///
/// `commandId` selects the command sent by the CAP interpreter. All arguments
/// are ignored and no room state changes.
s32 neoArkForestZoneIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg);

/// Starts the forest's first-visit action scene and forwards the action to pool B.
///
/// Borrows the four-byte `DirectionActionRequest` until dispatch returns.
/// Action 1 in variant 1 starts the HUD-managed script once and latches its flag.
/// Every action then forwards the same request and second argument word to the
/// pool-B task. Returns -1 before that task exists, otherwise its result; the
/// receiver is unused. Forest trigger selectors are 1..5, with zero clearing
/// pending spawns in the child latch. No payload pointer is retained here.
s32 neoArkForestZoneHandleAction(Task* unusedTask, s32 messageId, const DirectionActionRequest* request, s32 secondArg);

/// Forwards forest actor events to the room's roaming-enemy pool B.
///
/// Installed for `ROOM_MESSAGE_ACTOR_EVENT`; both payload words pass through
/// unchanged and `unusedTask` is ignored. Returns -1 before the pool task
/// exists, zero when it has no message table, or the pool's reply (1 after
/// extending its arrival cooldown). The receiver and overlay must stay live
/// through synchronous dispatch; no payload is retained here.
s32 neoArkForestZoneForwardActorEvent(Task* unusedTask, s32 messageId, s32 eventValue, s32 secondArg);

/// Requests the forest pool's placed-enemy ambush from the first-visit script.
///
/// Sends forest command 2 to the live pool-B task, reviving placed actor 0
/// from reserve when available. Does nothing before that task exists and
/// ignores its reply. The command storage is borrowed through dispatch.
void neoArkForestZoneStartRoamerAmbush(void);

#endif // SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H
