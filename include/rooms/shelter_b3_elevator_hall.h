#ifndef ROOMS_SHELTER_B3_ELEVATOR_HALL_H
#define ROOMS_SHELTER_B3_ELEVATOR_HALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

/// The gameplay-resident light slot the room's glows write: `mode` becomes 2
/// and `data.light` takes the glow's world position and a randomised
/// intensity.
typedef struct {
    s32 mode;
    union {
        GsCOORDINATE2 coord;
        GpObj44       light;
    } data;
} ShelterB3ElevatorHallLight;

extern ShelterB3ElevatorHallLight D_80114FF8;

#endif // ROOMS_SHELTER_B3_ELEVATOR_HALL_H
