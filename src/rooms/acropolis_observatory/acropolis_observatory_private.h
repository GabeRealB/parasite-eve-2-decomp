#ifndef SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H
#define SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H

#include "types.h"

#include "gameplay/animation.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"

extern TaskDesc D_acropolis_observatory_8017E7DC[4];

extern GpAnimSet D_acropolis_observatory_8017FE38;

/// Borrowed player animation table with the observatory clip in entry one.
extern GpAnimSet* gAcropolisObservatoryPlayerAnimationSets[2];

extern TaskDesc D_acropolis_observatory_8017FE6C;

extern GpScriptCmd D_acropolis_observatory_80183480[6];

extern GpScriptRec D_acropolis_observatory_80183498[2];

extern GpScriptCmd D_acropolis_observatory_801834A0[6];

extern GpScriptRec D_acropolis_observatory_801834B8[2];

#endif // SRC_ROOMS_ACROPOLIS_OBSERVATORY_ACROPOLIS_OBSERVATORY_PRIVATE_H
