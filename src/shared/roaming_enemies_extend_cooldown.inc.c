/* Part of the roaming enemies library; see roaming_enemies.h. */

// Include roaming_enemies.h and declare the carrier's signed-halfword
// _gRoamerCooldownFrames before this fragment. Its pool-B table owns this
// private callback; both forest rooms use the same cooldown contract.

/// Delays the next roaming-enemy arrival after a pool-B actor event.
///
/// Handles `ROOM_MESSAGE_ACTOR_EVENT`, adding 90 pool-update ticks and
/// returning 1. All callback arguments are unused; no retreat HP is banked.
/// The shared signed-halfword counter retains its low 16 bits on addition,
/// including when it was paused at -1. Repeated events accumulate delays.
static s32 _roamerExtendCooldown(Task* unusedTask, s32 unusedMessageId, s32 unusedEventValue, s32 unusedSecondArg)
{
    enum { ROAMER_COOLDOWN_EVENT_HANDLED = 1 };

    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
    return ROAMER_COOLDOWN_EVENT_HANDLED;
}
