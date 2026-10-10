#ifndef SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H
#define SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H

#include "common.h"

#include "gameplay/inventory.h"
#include "gameplay/message.h"

#include "rooms/room_common.h"

/// Displayed weapon-supply quantity in signed 24.8 fixed point.
///
/// Refill commits the live weapon load to capacity immediately, then this value
/// rises from the old quantity by one quarter-unit per callback and clamps at
/// capacity. The panel uses it for both the number and its meter, through closure.
extern s32 gShopRefillDisplayQuantityFixed;

/// Borrowed weapon-supply catalogue row currently displayed by the refill panel.
///
/// NULL until the first carried supply is selected. Retained while the panel
/// closes; gameplay and its catalogue must remain loaded throughout the session.
extern const EquipmentWeaponSupply* gShopRefillSupply;

/// Snapshot of the accepted transition, copied by the gate for the deferred event.
///
/// All eight bytes are replaced on another accepted start. Keep this room's
/// snapshot unchanged and loaded until the task commits its destination.
extern RoomEventMsg gRoomEventMsg;

/// Selects this overlay's externally linked transition snapshot for the shared event code.
#define ROOM_EVENT_MSG gRoomEventMsg

extern RoomEventActiveBytes gRoomEventActive;
extern RoomEventReq         gRoomEventReq;

extern RoomSavedViewStorage D_shelter_b1_armory_8018557C;

#endif // SRC_ROOMS_SHELTER_B1_ARMORY_SHELTER_B1_ARMORY_PRIVATE_H
