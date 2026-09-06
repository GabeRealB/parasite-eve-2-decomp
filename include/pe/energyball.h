#ifndef PE_ENERGYBALL_H
#define PE_ENERGYBALL_H

#include "common.h"

/// `SndEvt` ids for the energy ball, indexed by `GpEffWork.field_20`
/// (`Gp_StateC08.field_0 % 10 - 1`, so the sound scales with the combo
/// counter). The first three are the charge loop `func_energyball_8012EF48`
/// starts with `SndEvt_EnqueueType6`; the last three are the matching `...0001`
/// variants.
extern s32 D_energyball_8013117C[];

/// Sixteen 8-bit draws from `Gp_LcgState`, refilled once per cast by
/// `func_energyball_8012EF48` and consumed by the GTE pass in
/// `func_energyball_80130B54` as the per-vertex jitter of the ball's surface.
extern s16 D_energyball_801311A0[];

#endif /* PE_ENERGYBALL_H */
