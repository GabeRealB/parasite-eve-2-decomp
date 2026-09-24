#ifndef ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H
#define ROOMS_SHELTER_B1_POD_SERVICE_GANTRY_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Scratch block the room's spinning-sprite draw takes from `G_SCRATCH_HEAD`
/// and zeroes before use. `vec` is the coordinate's translation, projected
/// through `GsWSMATRIX` with one `RTPS`: `sx`/`sy` receive the screen
/// position, `flag` the GTE flag and `otz` the ordering-table depth. `dx`/`dy`
/// hold the current rotated half-extents added to and subtracted from `sx`/`sy`
/// to build the quad's corners; only their low halves are read back.
typedef struct ShelterB1PodServiceGantrySpinScratch {
    s32     otz;
    s32     flag;
    s32     dx;
    s32     dy;
    SVECTOR vec;
    s16     sx;
    s16     sy;
} ShelterB1PodServiceGantrySpinScratch;
STATIC_ASSERT_SIZEOF(ShelterB1PodServiceGantrySpinScratch, 0x1C);

/// The room task's three states, dispatched by
/// `func_shelter_b1_pod_service_gantry_8017D89C`.
extern const TaskFuncTable3 D_shelter_b1_pod_service_gantry_8017D5C4;

#endif
