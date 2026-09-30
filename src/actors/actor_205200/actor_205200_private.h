#ifndef SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
#define SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/tmd_types.h"

#include "overlay.h"

extern TmdSource D_actor_205200_801517EC;

extern OverlayWaveCtx* gScreenWaveCtx;

extern OverlayWaveRec gScreenWaveColumns[10];

extern OverlayWaveRec gScreenWaveRows[30];

extern POLY_FT4 gScreenWaveGrid[2][30][8];

extern OverlayWaveCtx D_actor_205200_8015B458;

#endif // SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
