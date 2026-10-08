#ifndef INCLUDE_ACTORS_ACTOR_444000_H
#define INCLUDE_ACTORS_ACTOR_444000_H

#include "types.h"

#include "actors/glutton_shake.h"

#include "gameplay/animation.h"

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

extern TmdSource gActor444000Actor403200Model10824;

extern TmdSource gActor444000Actor403200Model12884;

extern TmdSource gActor444000Actor403200Model13774;

extern TmdSource gActor444000GluttonLegLeft;

extern TmdSource gActor444000GluttonLegRight;

extern TmdSource gActor444000Actor403200Model18BE4;

extern TmdSource gActor444000Model1C814;

extern TmdSource gActor444000Model1D36C;

extern TmdSource gActor444000Model1DC9C;

extern TmdSource gActor444000Model1E14C;

#endif // INCLUDE_ACTORS_ACTOR_444000_H
