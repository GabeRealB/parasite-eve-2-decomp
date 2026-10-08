#ifndef SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_FOREST_ZONE_NEO_ARK_FOREST_ZONE_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

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

s32 func_neo_ark_forest_zone_8017D7E4(Task*, s32, RoomEventMsg*, RoomEventMsg*);

/// Ignores forest-zone room commands and returns zero.
///
/// `commandId` selects the command sent by the CAP interpreter. All arguments
/// are ignored and no room state changes.
s32 neoArkForestZoneIgnoreCommandMessage(Task* task, s32 messageId, s32 commandId, s32 secondArg);

s32 func_neo_ark_forest_zone_8017D958(Task*, s32, RoomEventMsg*, RoomEventMsg*);

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
