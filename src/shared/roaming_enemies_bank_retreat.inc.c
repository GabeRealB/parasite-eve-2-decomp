/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Banks a retreating enemy's HP in the first free reserve slot.
///
/// Installed for `ROOM_MESSAGE_ACTOR_EVENT`; `hp` is the reported hit-point
/// count. Positive HP is scaled to 110 percent, narrowed to a signed halfword,
/// then capped at the pool's maximum. A stored retreat releases a battle
/// reference, or defers its release while fewer than two references remain.
/// A stored retreat or nonpositive report adds the action cooldown; a full
/// reserve leaves it unchanged. Defines no result; message ID and final
/// payload are unused. Signed word scaling must remain in range.
static void _roamerBankRetreat(Task* task, s32 messageId, s32 hp, s32 unusedArg)
{
    enum { ROAMER_RETREAT_HP_PERCENT = 110 };
    s16 slotIndex;
    s16 bankedHp;

    if (hp > 0) {
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(gRoamerReserveHp); slotIndex++) {
            if (((s16*)gRoamerReserveHp)[slotIndex] == 0) {
                bankedHp                    = hp * ROAMER_RETREAT_HP_PERCENT / 100;
                gRoamerReserveHp[slotIndex] = bankedHp;
                if (gRoamerParams.hpMax < bankedHp) {
                    gRoamerReserveHp[slotIndex] = gRoamerParams.hpMax;
                }
                if (gSceneCombatState.battleRefs >= 2) {
                    sceneReleaseBattleRef(task, 0xD);
                } else {
                    gRoamerReleasePending = 1;
                }
                _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                return;
            }
        }
        return;
    }
    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
}
