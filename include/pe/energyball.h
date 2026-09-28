#ifndef PE_ENERGYBALL_H
#define PE_ENERGYBALL_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include "overlay.h"

#include "gameplay/actor.h"

#include "main/session_types.h"

/// One 4-byte row of `D_energyball_80131194`, indexed by `GpEffWork.index`
/// (`Gp_StateC08.field_0 % 10 - 1`). `field_0` is the full size the ball grows
/// to before it is launched (`GpEffWork.angle`; half of it is the linked
/// `GpObj.radius`, twice it the burst's final size) and `field_2` the
/// per-frame growth step, also the initial upward speed while charging.
typedef struct EnergyBallStep {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} EnergyBallStep;
STATIC_ASSERT_SIZEOF(EnergyBallStep, 4);

/// Collision block allocated by `func_energyball_8012F180` (`memCalloc(0x38)`)
/// and stored in `Task::work`: `obj` is linked on list 1 with `ctx.recs`
/// pointing at the one-element `rec` table (terminator `field_0 = 2`).
typedef struct EnergyBallWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec;
} EnergyBallWork;
STATIC_ASSERT_SIZEOF(EnergyBallWork, 0x38);

#endif /* PE_ENERGYBALL_H */
