/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Second arming state: the same as `roamerArmPoolA` with
/// its own gate, message table and game flags 0x10A / 0x10B.
void roamerArmPoolB(Task* task)
{
    s16 i;
    s16 nib;

    if (gRoamerArmCountsB[gGameSession->location.loc.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable      = gRoamerMsgTableB;
    gRoamerReserveCount = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE);
    nib                 = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_VARIANT);
    if (gGameSession->location.loc.variant != nib) {
        gRoamerReserveCount = gRoamerReserveCount + gRoamerArmCountsB[gGameSession->location.loc.variant];
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_RESERVE, gRoamerReserveCount);
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_B_VARIANT, gGameSession->location.loc.variant);
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
