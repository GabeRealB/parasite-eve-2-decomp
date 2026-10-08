/* The Dryfield parking lot's events, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PARKING_LOT_H
#define SRC_SHARED_PARKING_LOT_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

/// The area records the 0x11 event applies when it fires. The night room
/// defines them; in the day build the address lies past the package's end, so
/// the linker resolves it as an absolute symbol.
extern AreaApplyRec gParkingLotAreaRecs[];

/// Resolves parking-lot departures and gates the lobby and saloon door events.
///
/// Handles `ROOM_EVENT_MESSAGE_RESOLVE`; receiver and message ID are unused.
/// Borrows a complete request and writable reply, which may alias. Copies it
/// before resolving main-street/balcony variants on execution. Lobby execution
/// that requests an event applies saved area updates and records progress;
/// saloon execution that requests an event identifies its key item. Queries
/// clear the gate's start indication while suppressing those effects.
/// Returns the gate reply (0 missing collection, 1 ordinary, 2 eligible), with
/// lobby refusal mapped to 2. Keep latched room snapshots loaded and unchanged
/// until the event task requests reload; allocation failure still commits them.
s32 parkingLotResolveMessage(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

#endif /* SRC_SHARED_PARKING_LOT_H */
