#ifndef SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H

#include "types.h"

#include "gameplay/display.h"
#include "gameplay/message.h"

#include "rooms/room_common.h"

extern u8 gStoreSavedView;

/// Room-owned subtractive fade control for an accepted underpass departure.
///
/// The shared transition starts an eight-frame ramp; the resident fade task
/// borrows and may update this record throughout its lifetime. Keep the room
/// loaded for that task; the transition requests reload without a return ramp
/// or completion wait.
extern ScreenFade gGeneralStoreUnderpassFade;

/// Snapshot of the accepted transition, copied by the gate for the deferred event.
///
/// All eight bytes are replaced on another accepted start. Keep this room's
/// snapshot unchanged and loaded until the task commits its destination.
extern RoomEventMsg gRoomEventMsg;

/// Selects this overlay's externally linked transition snapshot for the shared event code.
#define ROOM_EVENT_MSG gRoomEventMsg

extern u8 gRoomEventActive;

extern u8 gStoreWarp;

extern u8 gStoreRoom;

extern RoomEventReq gRoomEventReq;

#endif // SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H
