#ifndef GAMEPLAY_SOUND_H
#define GAMEPLAY_SOUND_H

#include "types.h"

#include "main/sound_ids.h"

/// Resolves a sound id's nonzero stage nibble against the current session stage.
///
/// Bits 24..27 are replaced only when at least one of them is set. A zero
/// nibble leaves the id unchanged, including common, character and weapon ids.
/// Bank type, area/bank low byte, instance and entry are retained. The current
/// session must be live whenever substitution occurs; its stage must fit four
/// bits. Requires the gameplay overlay. No sound is queued or bank validated.
s32 sndScriptResolveStageId(s32 soundId);

/// Queues a sound-script start after resolving its stage nibble to the current stage.
///
/// Stage substitution follows `sndScriptResolveStageId`. Pan and attenuation
/// are narrowed to signed bytes before dispatch: pan uses three SPU pan steps
/// per unit, and attenuation uses the start-request magnitude scale described
/// by `sndEvtRequestScriptStart`. Zero offsets leave the base mix unchanged.
/// Its admission failures are discarded, so the request may silently fail.
/// Requires the gameplay overlay and the loaded-bank lifetime of that resident API.
void sndEvtRequestStageScriptStart(s32 soundId, s32 panOffset, s32 attenuation);

/// Queues a sound-script stop after resolving its stage nibble to the current stage.
///
/// Stage substitution follows `sndScriptResolveStageId`. Only the low sixteen
/// bits of `stopControl` reach `sndEvtRequestScriptStop`: 0 uses the normal
/// no-fade release policy, 1 keeps the existing release settings, and 2..65535
/// request a fade in audio updates for running entries. Bank-type selectors
/// ignore fade durations. Requires the gameplay overlay; resident stop-selector,
/// bank-lifetime and queue-admission rules apply and the request may be dropped.
void sndEvtRequestStageScriptStop(s32 soundSelector, s32 stopControl);

#endif // GAMEPLAY_SOUND_H
