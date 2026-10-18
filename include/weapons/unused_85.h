#ifndef INCLUDE_WEAPONS_UNUSED_85_H
#define INCLUDE_WEAPONS_UNUSED_85_H

#include "gameplay/animation.h"

/// Retained native player animation bank for unused weapon index 6 (asset weapons/unused_85/bank).
///
/// Character 1 selects bank index 7. Base slots 1..44 contain player clips;
/// slots 0, 45 and 46 are NULL. The 32 raw extension words at indices 47..78
/// start cleared and remain writable for scripted copies, including non-
/// pointer data. Play an extension entry only after installing a valid clip
/// pointer there. Keep unused_85 loaded while playback borrows this bank or
/// its native clips; copied clips also require their source data to remain
/// loaded.
extern AnimationSet* gUnused85PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

#endif // INCLUDE_WEAPONS_UNUSED_85_H
