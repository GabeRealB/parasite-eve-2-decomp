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

/// The one view drawn with the 1x depth shift: view 8 in the day room, view 5
/// at night.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define TRAILER_COACH_FINE_DEPTH_VIEW 8
#else
#define TRAILER_COACH_FINE_DEPTH_VIEW 5
#endif

#endif /* SRC_SHARED_TRAILER_COACH_H */
