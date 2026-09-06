#ifndef PE_INFERNO_H
#define PE_INFERNO_H

#include "common.h"

/// The `SndEvt_EnqueueType6` id the inferno cast plays, indexed by
/// `Gp_StateC08.field_0 % 10 - 1` so the roar scales with the combo counter.
/// The same index also picks the state `func_inferno_8012EF88` advances to,
/// which is why the three ids and the three state chains run in step.
extern s32 D_inferno_801304F0[];

#endif /* PE_INFERNO_H */
