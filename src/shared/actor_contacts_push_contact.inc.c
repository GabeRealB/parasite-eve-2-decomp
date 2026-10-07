/* Part of the actor contacts library; see actor_contacts.h. */

/// Applies a contact table's grid pushback to a coordinate's X/Z translation.
///
/// Reads exactly `contactCount` records (1..32767), ignoring table end markers
/// and non-grid contacts, under `worldCollisionResolvePushback`'s contract.
/// Correction and local translation must share the room coordinate frame.
/// For each X/Z component, adds its signed 16.16 word shifted right by 16,
/// then one further unit in its sign when a fraction remains. Negative
/// fractions therefore step below the arithmetic shift's flooring.
/// Coordinate additions must fit signed words; the saved vector retains
/// only each component's low 16 bits.
///
/// On a grid hit, saves the applied X/Z step and the unapplied Y integer part
/// in this carrier's last-push vector, narrowed to signed halfwords. With no
/// grid hit that vector stays intact. Returns 1 for nonzero resolved X/Z,
/// otherwise 0; a grid hit with only Y correction returns 0. Leaves Y,
/// rotation and the composition stamp intact; callers invalidate the cache.
/// Inputs must be live and clear of the initialized scratch stack's 72-byte
/// peak reservation. Retains no pointers and releases its block before return.
static s32 _actorContactApplyGridPushback(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount)
{
    enum {
        ACTOR_CONTACT_PUSH_FRACTION_BITS = 16,
        ACTOR_CONTACT_PUSH_FRACTION_MASK = 0xFFFF
    };

    /// Adds one unit in a fractional correction's sign to its translation and saved step.
    ///
    /// `correctionWord` is a side-effect-free signed 16.16 value, read up to
    /// twice. `translation` and `savedStep` are writable signed word/halfword lvalues,
    /// each evaluated once on the selected fractional path, in that order.
#define ACTOR_CONTACT_APPLY_FRACTIONAL_PUSH(correctionWord, translation, savedStep) \
    {                                                                               \
        if (((correctionWord) & ACTOR_CONTACT_PUSH_FRACTION_MASK) != 0) {           \
            if ((correctionWord) > 0) {                                             \
                (translation)++;                                                    \
                (savedStep)++;                                                      \
            } else {                                                                \
                (translation)--;                                                    \
                (savedStep)--;                                                      \
            }                                                                       \
        }                                                                           \
    }

    ActorContactPushScratch* scratch;
    s32                      correctionWord;

    scratch        = SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    scratch->moved = 0;

    // Apply the horizontal correction, retaining Y only in the saved step.
    if (worldCollisionResolvePushback(contacts, &scratch->delta, contactCount, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        coord->coord.t[0]                 += scratch->delta.fixed.vx.word >> ACTOR_CONTACT_PUSH_FRACTION_BITS;
        coord->coord.t[2]                 += scratch->delta.fixed.vz.word >> ACTOR_CONTACT_PUSH_FRACTION_BITS;
        _actorContactGetLastPushStep()->vx = scratch->delta.fixed.vx.word >> ACTOR_CONTACT_PUSH_FRACTION_BITS;
        _actorContactGetLastPushStep()->vy = scratch->delta.fixed.vy.word >> ACTOR_CONTACT_PUSH_FRACTION_BITS;
        _actorContactGetLastPushStep()->vz = scratch->delta.fixed.vz.word >> ACTOR_CONTACT_PUSH_FRACTION_BITS;
        correctionWord                     = scratch->delta.fixed.vx.word;
        ACTOR_CONTACT_APPLY_FRACTIONAL_PUSH(correctionWord, coord->coord.t[0], _actorContactGetLastPushStep()->vx);
        correctionWord = scratch->delta.fixed.vz.word;
        ACTOR_CONTACT_APPLY_FRACTIONAL_PUSH(correctionWord, coord->coord.t[2], _actorContactGetLastPushStep()->vz);
    }
    if (scratch->delta.fixed.vx.word != 0 || scratch->delta.fixed.vz.word != 0) {
        scratch->moved = 1;
    }

    // No reservation or call intervenes before reading the released block's result.
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return scratch->moved;
#undef ACTOR_CONTACT_APPLY_FRACTIONAL_PUSH
}
