/* The Dryfield back street's events, the same in the day and night builds.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_BACK_STREET_H
#define SRC_SHARED_BACK_STREET_H

#include "types.h"

#include "gameplay/message.h"

#include "main/task_types.h"

static s32 _roomVariantResolveBackStreet(Task* task, s32 messageId, const RoomEventMsg* request, RoomEventMsg* reply);

#endif /* SRC_SHARED_BACK_STREET_H */
