#ifndef GAMEPLAY_DIRECTION_INPUT_H
#define GAMEPLAY_DIRECTION_INPUT_H

#include "types.h"

/// Whether a latched world-trigger direction action is being serviced.
///
/// Zero is inactive and one is active. Eligible automatic or manual hits set
/// it; cancellation, consumed one-shot actions and completed transitions clear
/// it. Multi-phase warp/stair actions retain it between direction updates.
/// Other overlays use it to defer scene controllers while an action is active.
extern u8 gDirectionActionActive;

/// Eligible idle direction updates remaining before another manual interaction.
///
/// Zero permits manual activation. Nonzero values decrement only while no
/// direction action is active and attachment mode is idle; automatic actions
/// bypass the counter. View/display changes, input gates and event exits rearm
/// it to ten. The decrement views the bits as u16 before narrowing back to s16.
extern s16 gDirectionInteractionRearmUpdates;

#endif // GAMEPLAY_DIRECTION_INPUT_H
