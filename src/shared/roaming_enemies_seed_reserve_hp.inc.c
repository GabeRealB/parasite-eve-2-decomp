/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Refills the armed roaming-enemy reserve slots and empties the unused slots.
///
/// Requires the shared nonnegative reserve count and enemy parameters. Caps
/// that live count at the five-slot array capacity, then stores maximum HP
/// in each armed slot and zero in the rest. Persisted flag counts and the
/// cooldown are left to the caller; retreat HP from an earlier visit is replaced.
static __inline__ void _roamerSeedReserveHp(void)
{
    enum { ROAMER_RESERVE_SLOTS = ARRAY_SIZE(gRoamerReserveHp) };
    s16 reserveSlot;

    if (gRoamerReserveCount >= ROAMER_RESERVE_SLOTS + 1) {
        gRoamerReserveCount = ROAMER_RESERVE_SLOTS;
    }
    for (reserveSlot = 0; reserveSlot < ROAMER_RESERVE_SLOTS; reserveSlot++) {
        if (reserveSlot < gRoamerReserveCount) {
            gRoamerReserveHp[reserveSlot] = gRoamerParams.hpMax;
        } else {
            gRoamerReserveHp[reserveSlot] = 0;
        }
    }
}
