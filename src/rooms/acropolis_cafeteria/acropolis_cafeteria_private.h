#ifndef ACROPOLIS_CAFETERIA_PRIVATE_H
#define ACROPOLIS_CAFETERIA_PRIVATE_H

#include "main/task_types.h"

#include "gameplay/light.h"

// Retained exporter slots follow the active spotlights. Their contents
// include stale/incomplete addresses; preserve them as bytes pending review.
typedef struct {
    GpSpotLight active[1];
    u8 retained[1512];
} AcropolisCafeteriaSpotLightStorage;
STATIC_ASSERT_SIZEOF(AcropolisCafeteriaSpotLightStorage, 1620);

extern AcropolisCafeteriaSpotLightStorage D_acropolis_cafeteria_8018A3C4;

// Callbacks referenced by the overlay's shared data tables.
void func_acropolis_cafeteria_8017E47C(Task *);
void func_acropolis_cafeteria_8017E658(Task *);
void func_acropolis_cafeteria_8017E6B8(Task *);
s32 func_acropolis_cafeteria_8017F908(Task *, s32, s32, s32);

#endif
