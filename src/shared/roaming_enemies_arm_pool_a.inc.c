/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Arms the roaming-enemy reserve feeding Neo Ark area 29 for this visit.
///
/// Requires a location variant in 0..13, indexing the carrier's arming table.
/// A zero count clears the task's messages and advances without changing the
/// reserve or cooldown. Otherwise installs pool A's messages and adds that
/// variant's count when it differs from the persisted variant. Persists the
/// sum before capping the live reserve at five, refills banked HP and starts
/// a 90-update cooldown. Both pools share this storage and replace retreat HP.
static void _roamerArmPoolA(Task* task)
{
    s16 previousVariant;

    if (gRoamerArmCountsA[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable      = gRoamerMsgTableA;
    gRoamerReserveCount = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
    previousVariant     = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT);
    // Persist the accumulated count before limiting this visit's live slots.
    if (gGameSession->location.loc.variant != previousVariant) {
        gRoamerReserveCount = gRoamerReserveCount + gRoamerArmCountsA[gGameSession->location.loc.variant];
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, gRoamerReserveCount);
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT, gGameSession->location.loc.variant);
    }
    _roamerSeedReserveHp();
    _gRoamerCooldownFrames = ROAMER_ACTION_COOLDOWN_FRAMES;
    task->state            = task->state + 1;
}
