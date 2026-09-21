#ifndef ACTORS_SHARED_801359CC_H
#define ACTORS_SHARED_801359CC_H

#include "actors/actors_shared_80138efc.h"

/// Turns the actor toward slot 0, then chooses the next state.
///
/// While `field_BA8` is clear, the bearing to slot 0 is stored at `field_B90`
/// and a 2..33 frame countdown is drawn from the LCG into `field_B8C`. The
/// latch then slews `field_B8E` toward that bearing, clamped to ±0x600, by
/// 0xC0 a frame, and arms the weapon once it arrives. After the countdown the
/// model's yaw tracks the fresh bearing by at most 0x10 and is masked to 12
/// bits. A bearing inside ±0x7F picks state 0xB, 0xC or 0xD from the LCG when
/// the spawn argument is set, and otherwise 0xC, 0xD or 0xE from the squared
/// distance to slot 3. Either choice clears the latch.
///
/// Carried by `actor_101100`, `actor_104900`, `actor_201100` and `actor_204900`.
void ActorsShared801359cc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif
