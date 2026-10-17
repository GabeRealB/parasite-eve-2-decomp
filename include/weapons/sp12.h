#ifndef INCLUDE_WEAPONS_SP12_H
#define INCLUDE_WEAPONS_SP12_H

#include "gameplay/animation.h"

/// Native player animation bank supplied by the loaded SP12 package.
///
/// Character 1 selects bank index 15 for equipped weapon 14. Base slots 1..44
/// contain nineteen-track player clips; slots 0, 45 and 46 are NULL. The 32
/// raw extension words at indices 47..78 start cleared and remain writable for
/// scripted copies, including non-pointer data. Play an extension entry only
/// after installing a valid clip pointer there. Keep SP12 loaded while playback
/// borrows this bank or its native clips; copied clips also require their source
/// data to remain loaded.
extern AnimationSet* gSp12PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

#endif // INCLUDE_WEAPONS_SP12_H
