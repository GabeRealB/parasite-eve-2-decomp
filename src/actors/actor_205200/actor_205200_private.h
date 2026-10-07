#ifndef SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
#define SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/tmd_types.h"

#include "overlay.h"

extern TmdSource gActor205200EveBreaMaskedBody;

extern ScreenWaveCtx* gScreenWaveCtx;

extern ScreenWaveGridOscillator gScreenWaveColumns[10];

extern ScreenWaveGridOscillator gScreenWaveRows[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `_screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

extern ScreenWaveCtx D_actor_205200_8015B458;

#endif // SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
