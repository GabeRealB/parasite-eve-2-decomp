/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Caps the active reserve at five slots and seeds their banked HP.
///
/// Armed slots receive the enemy kind's maximum HP; remaining slots become empty.
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

/// Arms the roaming-enemy reserve feeding Neo Ark area 11 for this visit.
///
/// Requires a location variant in 0..15. A zero arming count clears the task's
/// message table and advances without changing the reserve or cooldown.
/// Otherwise installs pool B's messages and adds the variant's count once per
/// change from the persisted variant. Persists the sum before capping the live
/// reserve at five, refills banked HP and starts a 90-update cooldown.
/// Shares reserve storage with pool A; neither pool may own separate live slots.
static void _roamerArmPoolB(Task* task)
{
    s16 previousVariant;

    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable      = gRoamerMsgTableB;
    gRoamerReserveCount = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
    previousVariant     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_VARIANT);
    // Persist the accumulated count before limiting this visit's live slots.
    if (gGameSession->location.loc.variant != previousVariant) {
        gRoamerReserveCount = gRoamerReserveCount + gRoamerArmCountsB[gGameSession->location.loc.variant];
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, gRoamerReserveCount);
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_VARIANT, gGameSession->location.loc.variant);
    }
    _roamerSeedReserveHp();
    _gRoamerCooldownFrames = ROAMER_ACTION_COOLDOWN_FRAMES;
    task->state            = task->state + 1;
}
