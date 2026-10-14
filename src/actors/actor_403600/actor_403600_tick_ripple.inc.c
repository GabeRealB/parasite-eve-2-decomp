/* Shared by both units of actor_403600. */

/// Records one ripple-source sample and advances its outward history.
///
/// Requires a live initialized ripple and head in 0..31. Source strength uses
/// 4096 as unity: emission adds 512 while below unity, release subtracts 128
/// while positive, and an emission edge restarts phase. The new sample is cleared
/// first, then recorded only at nonzero strength. Source phase is in 4096 units
/// per turn; stored samples retain its signed low halfword. Owns no storage.
static inline void _actor403600TickRipple(Actor403600Ripple* state)
{
    enum {
        ACTOR_403600_RIPPLE_FULL_STRENGTH      = ONE,
        ACTOR_403600_RIPPLE_STRENGTH_RISE      = 512,
        ACTOR_403600_RIPPLE_STRENGTH_FALL      = 128,
        ACTOR_403600_RIPPLE_PHASE_STEP         = 384,
        ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP = 256
    };
    s32 head;

    state->head          += ACTOR_403600_RIPPLE_SAMPLE_COUNT - 1;
    state->head          %= ACTOR_403600_RIPPLE_SAMPLE_COUNT;
    head                  = state->head;
    state->phase[head]    = 0;
    state->strength[head] = 0;
    if (state->emitting != 0) {
        if (state->wasEmitting == 0) {
            state->sourcePhase = 0;
        }
        if (state->sourceStrength < ACTOR_403600_RIPPLE_FULL_STRENGTH) {
            state->sourceStrength += ACTOR_403600_RIPPLE_STRENGTH_RISE;
        }
    } else if (state->sourceStrength > 0) {
        state->sourceStrength -= ACTOR_403600_RIPPLE_STRENGTH_FALL;
    }
    state->wasEmitting = state->emitting;
    if (state->sourceStrength != 0) {
        state->phase[head]    = state->sourcePhase;
        state->strength[head] = state->sourceStrength;
        if (state->shallow == 0) {
            state->sourcePhase += ACTOR_403600_RIPPLE_PHASE_STEP;
        } else {
            state->sourcePhase += ACTOR_403600_RIPPLE_SHALLOW_PHASE_STEP;
        }
    }
}
