#ifndef INCLUDE_ACTORS_ACTOR_444000_H
#define INCLUDE_ACTORS_ACTOR_444000_H

#include "types.h"

#include "actors/glutton_shake.h"

#include "gameplay/animation.h"
#include "gameplay/pad_script.h"

#include "main/task_types.h"
#include "main/tmd_types.h"

/// Requests a vertical screen shake from the garbage-incinerator Glutton host.
///
/// `actor_444000` must be loaded and its successfully spawned host task and work
/// block must still be alive. The request is handled on a later host update;
/// repeating the armed level does not restart the shake.
/// `GLUTTON_SHAKE_SHORT`, `GLUTTON_SHAKE_MEDIUM` and `GLUTTON_SHAKE_LONG` select
/// 5, 10 and 22 frames. `level` is stored as an unsigned byte without validation;
/// other values, including `GLUTTON_SHAKE_NONE`, neither arm a changed request
/// nor immediately clear the display offset.
void actor444000GluttonSetShakeLevel(s8 level);

/// Two-lane impact vibration program for the incinerator Glutton and its staged body.
///
/// Starts a seven-frame binary-motor pulse and a 27-frame variable-motor ramp
/// simultaneously, then stops both lanes. Pass with
/// `gActor444000GluttonImpactPadSegments`; both arrays are borrowed by script
/// tasks, so this overlay must remain loaded until those tasks finish.
extern PadScriptCmd gActor444000GluttonImpactPadCommands[2];

/// Timed segments of `gActor444000GluttonImpactPadCommands`.
///
/// Entry 0 supplies the binary lane's seven frames; entry 1 ramps variable
/// intensity from 255 toward 53 over 27 script frames. Intensity endpoints
/// are bytes before attenuation. The stored fourth byte has no established use.
extern PadScriptVibrationSegment gActor444000GluttonImpactPadSegments[2];

// Native animation sets shared with the companion actor overlay.
extern AnimationSet gActor444000Animation20BD4;

extern AnimationSet gActor444000Animation20C80;

extern AnimationSet gActor444000Animation20D2C;

extern AnimationSet gActor444000Animation21014;

extern AnimationSet gActor444000Animation212D8;

extern AnimationSet gActor444000Animation215B4;

extern AnimationSet gActor444000Animation2C240;

extern AnimationSet gActor444000Animation2C2CC;

extern AnimationSet gActor444000Animation2C358;

extern AnimationSet gActor444000Animation25BE0;

extern AnimationSet gActor444000Animation25F14;

extern AnimationSet gActor444000Animation26240;

extern AnimationSet gActor444000Animation2E198;

extern AnimationSet gActor444000Animation2E548;

extern AnimationSet gActor444000Animation2EA80;

extern AnimationSet gActor444000Animation2EE14;

extern TmdSource gGluttonBody;

extern TmdSource gGluttonActor403200Model12884;

extern TmdSource gGluttonActor403200Model13774;

extern TmdSource gGluttonLegLeft;

extern TmdSource gGluttonLegRight;

extern TmdSource gGluttonActor403200Model18BE4;

extern TmdSource gActor444000Model1C814;

extern TmdSource gActor444000Model1D36C;

extern TmdSource gActor444000Model1DC9C;

extern TmdSource gActor444000Model1E14C;

/// Bodyless priority-192 controller for the incinerator post-boss event.
///
/// Spawn argument 1 selects the delayed alert path when zero, or the immediate
/// later-phase descent when nonzero; argument 2 is unused. The callback allocates
/// and publishes owned work, drives queued player actions and hands completion
/// back to the room. Requires the loaded actor and incinerator room overlays and
/// live player through event completion; the descriptor is borrowed during spawn.
extern TaskDesc gActor444000IncineratorEventTaskDesc;

#endif // INCLUDE_ACTORS_ACTOR_444000_H
