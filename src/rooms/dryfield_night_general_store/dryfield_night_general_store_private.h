#ifndef SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H

#include "types.h"

#include "gameplay/display.h"
#include "gameplay/message.h"

#include "rooms/room_common.h"

/// Live-save view selector captured before the underpass prompt switches to view 16.
///
/// Cancellation restores this byte. One transition owns the singleton capture
/// until cancellation or reload; loading the room image resets it to zero.
extern u8 gGeneralStoreUnderpassSavedView;

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

/// Selects the night General Store's retained destination for deferred room events.
///
/// Include before `room_events.h` in both room translation units. The binding
/// is a writable `RoomEventMsg` lvalue with no side effects; it allocates no
/// storage. The gate replaces all eight bytes on eligible execution, even if
/// task allocation fails; queries leave them unchanged. Keep the snapshot
/// loaded and unchanged until the task commits area/warp/room and requests reload.
#define ROOM_EVENT_DESTINATION (gRoomEventMsg)

extern u8 gRoomEventActive;

/// Underpass arrival-warp byte latched by the accepted departure request.
///
/// Written after requesting the deferred task, then copied into the live save
/// on acceptance. Keep unchanged through prompt completion; reload discards it.
extern u8 gGeneralStoreUnderpassArrivalWarp;

/// Underpass room selector latched from the request after destination resolution.
///
/// An aliased request/reply supplies the resolved room; distinct records retain
/// the input room. Acceptance copies this byte to the live save. One pending
/// transition owns the latch until cancellation or reload; no queue is stored.
extern u8 gGeneralStoreUnderpassRoom;

extern RoomEventReq gRoomEventReq;

#endif // SRC_ROOMS_DRYFIELD_NIGHT_GENERAL_STORE_DRYFIELD_NIGHT_GENERAL_STORE_PRIVATE_H
