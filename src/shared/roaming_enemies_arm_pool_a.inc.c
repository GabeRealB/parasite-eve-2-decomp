/* Part of the roaming enemies library; see roaming_enemies.h. */

/// First arming state: with no spawns to arm for the session's slot it only
/// advances; otherwise it installs its message table, folds the slot's spawn
/// count into game flag 0x10C (remembering the slot in 0x10D), caps it at five
/// and fills that many spawn slots with the room's ceiling, zeroing the rest.
void roamerArmPoolA(Task* task)
{
    s16 i;
    s16 nib;

    if (gRoamerArmCountsA[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable      = gRoamerMsgTableA;
    gRoamerReserveCount = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
    nib                 = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT);
    if (gGameSession->location.loc.variant != nib) {
        gRoamerReserveCount = gRoamerReserveCount + gRoamerArmCountsA[gGameSession->location.loc.variant];
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, gRoamerReserveCount);
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT, gGameSession->location.loc.variant);
    }
    if (gRoamerReserveCount >= 6) {
        gRoamerReserveCount = 5;
    }
    for (i = 0; i < 5; i++) {
        if (i < gRoamerReserveCount) {
            gRoamerReserveHp[i] = gRoamerParams.hpMax;
        } else {
            gRoamerReserveHp[i] = 0;
        }
    }
    _gRoamerCooldownFrames = ROAMER_ACTION_COOLDOWN_FRAMES;
    task->state            = task->state + 1;
}
