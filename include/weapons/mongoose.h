#ifndef INCLUDE_WEAPONS_MONGOOSE_H
#define INCLUDE_WEAPONS_MONGOOSE_H

#include "gameplay/animation.h"

/// Native player animation bank supplied by the loaded Mongoose package (asset weapons/mongoose/bank).
///
/// Character 1 selects bank index 10 for equipped weapon 9. Base slots 1..44
/// contain player clips; slots 0, 45 and 46 are NULL. The 32 raw extension
/// words at indices 47..78 start cleared and remain writable for scripted
/// copies, including non-pointer data. Play an extension entry only after
/// installing a valid clip pointer there. Keep Mongoose loaded while playback
/// borrows this bank or its native clips; copied clips also require their
/// source data to remain loaded.
extern AnimationSet* gMongoosePlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

#endif // INCLUDE_WEAPONS_MONGOOSE_H
