#ifndef GAMEPLAY_PRIVATE_ROOM_EFFECTS_H
#define GAMEPLAY_PRIVATE_ROOM_EFFECTS_H

#include "types.h"

/// Tint selectors without a corresponding named `PLAYER_STATUS_*` constant.
///
/// Reaction 4 requests its tint without applying a player status. The 0x20
/// selector accompanies the timed status whose gameplay meaning is unproven.
enum {
    ROOM_EFFECT_STATUS_TINT_REACTION_4 = 0x08,
    ROOM_EFFECT_STATUS_TINT_STATUS_20  = 0x20,
};

/// Starts Darkness's screen dimming when no dim task has claimed the screen.
///
/// Requires a live room-effect state. The effect-limit bypass remains counted;
/// allocation failure is ignored. The task claims the flag on its first tick,
/// so calls before that tick can start more than one task. Does not apply the
/// player's Darkness status.
void roomEffectStartDarknessDim(void);

/// Publishes a player-status tint selector and starts its short screen pulse.
///
/// Requires a live room-effect state. Only `statusMask`'s low byte is retained
/// and forwarded. Callers use zero or one of the eight single-bit selectors;
/// replacing it retires pulses whose selector differs. Uses a counted spawn
/// that bypasses the ordinary limit. Even if allocation fails, the new selector
/// remains published. Does not apply or clear player status flags.
void roomEffectStartStatusTint(s32 statusMask);

/// Clears the Berserker glow claim and requests a fresh shot-glow task.
///
/// Requires a live room-effect state and a player model when the task starts.
/// The screen burst guard suppresses the request; no setter for that guard is
/// established. An accepted request clears the shared PE burst claim before a
/// counted spawn that bypasses the ordinary limit. Failure is ignored. The glow
/// task later claims the bit and consumes shot requests while Berserker and
/// battle state permit it; this function does not request a shot or apply status.
void roomEffectStartBerserkerGlow(void);

#endif // GAMEPLAY_PRIVATE_ROOM_EFFECTS_H
