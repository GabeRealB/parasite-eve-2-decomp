#ifndef INCLUDE_WEAPONS_UNUSED_86_H
#define INCLUDE_WEAPONS_UNUSED_86_H

#include "gameplay/animation.h"

/// Native player animation bank shared by the five unused weapon packages (asset weapons/unused_86/bank).
///
/// The `unused_86`, `unused_87`, `unused_89`, `unused_91` and `unused_97`
/// packages provide identical banks, selected at player bank indices 8, 9,
/// 11, 19 and 25 respectively. Base slots 1..44 hold 36 distinct clips;
/// slots 0, 45 and 46 are NULL. The 32 extension words at indices 47..78
/// start cleared and remain writable for raw-word scripted copies, including
/// payloads that are not clip pointers. Play extension slots only when they
/// contain valid clips.
///
/// Playback borrows this bank and its native clip data from the loaded weapon
/// package. Keep that package loaded while any slot or cached record pointer
/// refers to them; copied clips also require their source packages to remain
/// loaded. These packages contain animations without executable attack code.
extern AnimationSet* gUnused86PlayerAnimationBank[ANIMATION_BANK_SET_CAPACITY];

#endif // INCLUDE_WEAPONS_UNUSED_86_H
