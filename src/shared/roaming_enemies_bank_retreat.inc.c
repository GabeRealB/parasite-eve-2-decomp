/* Part of the roaming enemies library; see roaming_enemies.h. */

/// 0x13F4 handler of the first arming state's message table: a positive
/// `arg2` fills the first free spawn slot with 110% of it, clamped to the
/// room's ceiling, and releases the `gSceneCombatState` reference (or marks it for
/// release once that is allowed). The countdown is bumped by 0x5A unless every
/// slot was already full.
void roamerBankRetreat(Task* task, s32 arg1, s32 arg2, s32 arg3)
{
    s16 i;
    s16 v;

    if (arg2 > 0) {
        for (i = 0; i < 5; i++) {
            if (((s16*)gRoamerReserveHp)[i] == 0) {
                v                           = arg2 * 0x6E / 100;
                ((s16*)gRoamerReserveHp)[i] = v;
                if (gRoamerParams.hpMax < v) {
                    ((s16*)gRoamerReserveHp)[i] = gRoamerParams.hpMax;
                }
                if (gSceneCombatState.battleRefs >= 2) {
                    sceneReleaseBattleRef(task, 0xD);
                } else {
                    gRoamerReleasePending = 1;
                }
                gRoamerCooldown += 0x5A;
                return;
            }
        }
        return;
    }
    gRoamerCooldown += 0x5A;
}
