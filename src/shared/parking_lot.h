/* The Dryfield parking lot's events, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_PARKING_LOT_H
#define SRC_SHARED_PARKING_LOT_H

#include "types.h"

#include "main/task_types.h"

/// The area records the 0x11 event applies when it fires. The night room
/// defines them; in the day build the address lies past the package's end, so
/// the linker resolves it as an absolute symbol.
extern AreaApplyRec gParkingLotAreaRecs[];

s32 parkingLotEventMsg(Task* task, s32 msgId, RoomEventMsg* msg, RoomEventMsg* out);

#endif /* SRC_SHARED_PARKING_LOT_H */
