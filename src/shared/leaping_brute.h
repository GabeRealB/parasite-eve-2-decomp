/* Pose helpers of one large enemy species that ships as actor_400600 and
 * actor_405800. It leaps and lands with a slam that raises dust, and carries
 * two child models parented to root parts 10 and 7. The helpers turn it toward
 * a point, read a part's view-space position, light it from its body part and
 * rebuild its root rotation from pitch, yaw and roll. The packages use
 * different work types, so the fragments reach the block through a type name
 * each package defines (only the angles at 0x80-0x84 are touched).
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_LEAPING_BRUTE_H
#define SRC_SHARED_LEAPING_BRUTE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "common.h"

#include "main/task_types.h"

/// A model part's view-space position as the pose helpers hand it back; only
/// `x` and `z` are written.
typedef struct BruteViewPos {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
} BruteViewPos;
STATIC_ASSERT_SIZEOF(BruteViewPos, 0x6);

/// The part of the brute's work block the pose helpers touch: its root
/// rotation as pitch, yaw and roll. The rest is each package's own.
typedef struct BruteWork {
    /* 0x00 */ byte pad_0[0x80];
    /* 0x80 */ u16  pitch;
    /* 0x82 */ u16  yaw;
    /* 0x84 */ u16  roll;
} BruteWork;

#include "main/task_types.h"

void bruteTurnToward(Task* arg0, SVECTOR* target, s32 step);
void bruteReadPartViewXZ(Task* task, s16 index, BruteViewPos* out);
void bruteUpdateColor(Task* task);
void bruteApplyRotation(Task* arg0);

#endif /* SRC_SHARED_LEAPING_BRUTE_H */
