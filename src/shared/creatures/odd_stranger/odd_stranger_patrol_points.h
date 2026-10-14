#ifndef SRC_SHARED_ODD_STRANGER_PATROL_POINTS_H
#define SRC_SHARED_ODD_STRANGER_PATROL_POINTS_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/inline_c.h>

#include "main/gfx.h"

/// Seeds the Odd Stranger's two patrol waypoints along its horizontal forward axis.
///
/// Requires live task/model and OddStrangerWork storage. directionVector is a
/// writable SVECTOR lvalue, directionPtr == &directionVector. Arguments are
/// evaluated repeatedly and must have no side effects or refer to the local
/// ODD_STRANGER_PATROL_SPAN enumerator. Normalizes with Y cleared, scales to
/// 2000 parent-coordinate units through the GTE, and narrows waypoint X/Z to
/// signed halfwords. Captures no caller identifiers; forms one compound statement.
/// Both actor_401000 and actor_401800 include this immediately before use and
/// undefine the macro afterward. Requires gfx/VectorNormalSS and GTE interfaces.
#define ODD_STRANGER_INITIALIZE_PATROL_POINTS(actorTask, actorWork, directionVector, directionPtr)          \
    {                                                                                                       \
        enum { ODD_STRANGER_PATROL_SPAN = 2000 };                                                           \
        (actorWork)->patrolTarget      = 0;                                                                 \
        (actorWork)->patrolPoints[0].x = (actorTask)->extra.tmd->coords->coord.t[0];                        \
        (actorWork)->patrolPoints[0].z = (actorTask)->extra.tmd->coords->coord.t[2];                        \
        gfxReadMatrixZAxis(&(actorTask)->extra.tmd->coords->coord, (directionPtr));                         \
        (directionVector).vy = 0;                                                                           \
        VectorNormalSS((directionPtr), (directionPtr));                                                     \
        gte_lddp(ODD_STRANGER_PATROL_SPAN);                                                                 \
        gte_ldsv((directionPtr));                                                                           \
        gte_gpf12();                                                                                        \
        gte_stsv((directionPtr));                                                                           \
        (actorWork)->patrolPoints[1].x = (actorTask)->extra.tmd->coords->coord.t[0] + (directionVector).vx; \
        (actorWork)->patrolPoints[1].z = (actorTask)->extra.tmd->coords->coord.t[2] + (directionVector).vz; \
    }

#endif // SRC_SHARED_ODD_STRANGER_PATROL_POINTS_H
