#ifndef SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
#define SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/tmd_types.h"

#include "overlay.h"

/// Execution priority of this package's controller, parts and stationary body.
enum { ACTOR_205200_ENEMY_TASK_PRIORITY = 96 };

extern TmdSource gActor205200EveBreaMaskedBody;

extern ScreenWaveCtx* gScreenWaveCtx;

extern ScreenWaveGridOscillator gScreenWaveColumns[10];

extern ScreenWaveGridOscillator gScreenWaveRows[30];

/// Double-buffered 8 by 30 meshes of textured quads, one mesh per frame
/// buffer. `_screenWaveGridTask` builds them once and moves their corners.
extern POLY_FT4 gScreenWaveGrid[2][30][8];

/// Persistent, untinted ramp context for the controller's screen-wave pulses.
///
/// A finished phase permits the next spawn: rise length 15 ticks, peak scale 160.
/// The grid task borrows this storage through its final draw and owns frame/rise
/// updates; the controller requests falling. Zero BSS keeps texture modulation off.
extern ScreenWaveCtx gActor205200ScreenWaveSpawnCtx;

#endif // SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
