#ifndef INCLUDE_PE_ANTIBODY_H
#define INCLUDE_PE_ANTIBODY_H

#include "main/task_types.h"

/// Updates an Antibody mote as it converges on the caster, rises or becomes a player-linked flash.
///
/// Bank-6 effect `EFFECT_ANTIBODY_MOTE` supplies a live single-coordinate body
/// and an owned `EffectWork` in `spawnArg2.pointer`. The spawn offset is in the
/// borrowed parent's local coordinate units; that parent must remain live.
/// Initialization requires an Antibody attachment id (PE level 1..3), selects
/// that level's tuning and falls through to the first inward step.
///
/// Ages 1..16 subtract one sixteenth of the spawn offset per frame unless a
/// random transition freezes the position and draws a larger sprite plus a
/// strip to the player's second model coordinate. Otherwise ages 17..21 move
/// upward by 128 local units per frame. Both final phases draw at age 21 before
/// releasing the work and task. The flash requires a live player model with at
/// least two composed coordinates; drawing requires the current frame's GPU
/// primitive arena, ordering table and initialized scratch stack.
void antibodyMoteTask(Task* task);

void func_antibody_8012EF34(Task* arg0);

#endif // INCLUDE_PE_ANTIBODY_H
