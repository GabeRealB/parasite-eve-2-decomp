/* The Dryfield G & R kitchen's door handler, the same in the day and night
 * builds but for the stage's sound bank.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_G_R_KITCHEN_H
#define SRC_SHARED_G_R_KITCHEN_H

#include "types.h"

#include "main/task_types.h"

#include "dryfield_time.h"

s32 grKitchenDoorMsg(Task* arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out);

#endif /* SRC_SHARED_G_R_KITCHEN_H */
