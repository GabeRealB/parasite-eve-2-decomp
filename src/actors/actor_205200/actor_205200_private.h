#ifndef SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H
#define SRC_ACTORS_ACTOR_205200_ACTOR_205200_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>
#include <psyq/libgpu.h>

#include "main/tmd_types.h"

#include "overlay.h"

extern TmdSource gEveMaskedBody;

/// Borrowed context of this renderer's active wave task.
///
/// Set on the seed tick and left unchanged at teardown. Only one wave may use
/// this storage; the context must stay live through its final drawing tick.
extern ScreenWaveCtx* gScreenWaveContext;

/// Column sine oscillators for horizontal screen displacement.
///
/// The per-frame renderer uses entries 0..10; the prebuilt-grid renderer uses
/// 0..8. Any remaining declared capacity is unaccessed; its role is unproven.
extern ScreenWaveGridOscillator gScreenWaveColumnWaves[10];

/// Thirty active row sine oscillators for vertical screen displacement.
///
/// Seeded at wave startup and advanced on drawing updates. Any declared
/// entries beyond 29 are unaccessed; their role is unproven.
extern ScreenWaveGridOscillator gScreenWaveRowWaves[30];

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
