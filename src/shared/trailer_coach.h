/* The Dryfield trailer coach's depth-shift task, the same in the day and night
 * builds but for which view it applies to.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_TRAILER_COACH_H
#define SRC_SHARED_TRAILER_COACH_H

#include "types.h"

#include "main/task_types.h"

#include "dryfield_time.h"

#if DRYFIELD_TIME == DRYFIELD_DAY
/// Saved view ID that selects 1x ordering-table depth in the daytime coach.
///
/// Selected at compilation by DRYFIELD_TIME; other views use 8x depth.
enum { TRAILER_COACH_FINE_DEPTH_VIEW = 8 };
#else
/// Saved view ID that selects 1x ordering-table depth in the nighttime coach.
///
/// Selected at compilation by DRYFIELD_TIME; other views use 8x depth.
enum { TRAILER_COACH_FINE_DEPTH_VIEW = 5 };
#endif

#endif /* SRC_SHARED_TRAILER_COACH_H */
