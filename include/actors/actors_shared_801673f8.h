#ifndef ACTORS_SHARED_801673F8_H
#define ACTORS_SHARED_801673F8_H

#include "common.h"

#include "main/task.h"

/// 8-byte spawn point in the absolute tables `D_8018B74C` (map 0x427) and
/// `D_801874C4` (map 0x428), indexed by bits 8..11 of the 0x2C00 message
/// halfword (`ActorsShared80168d3cWork::field_44C`). `ActorsShared801673f8`
/// places the model root at `x` / `y` / `z` facing `heading` + 0x800.
typedef struct ActorsShared801673f8Spot {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ u16 heading;
} ActorsShared801673f8Spot;
STATIC_ASSERT_SIZEOF(ActorsShared801673f8Spot, 0x8);

extern ActorsShared801673f8Spot D_801874C4[]; // absolute, map 0x428's spawn points
extern ActorsShared801673f8Spot D_8018B74C[]; // absolute, map 0x427's spawn points

/// Message 0x2C00 with low nibble 1 (see `field_44C`): reveals the model,
/// places its root at the spawn point bits 8..11 select from the current map's
/// table (playing the appearance sound on map 0x427), and starts state 1, 4
/// or 7 by bits 4..7. Shared by `actor_341700` and `actor_342400`.
void ActorsShared801673f8(Task* arg0);

#endif
