/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Per-frame state after `roamerArmPoolA`: counts the
/// countdown down, releases a pending `gSceneCombatState` reference, and once that
/// reference has dropped folds the still-pending spawn slots back into game
/// flags 0x168 and 0x10C. On a placement request it hands the first pending
/// slot to a waiting slot-4 task (one whose enemy `hp` still reads -999), sends it
/// message 0x7DB and places it at the requested point.
void roamerTickPoolA(Task* task)
{
    s16    i;
    s16    count;
    s32    a;
    s32    b;
    Enemy* obj;
    s16    j;
    s16    k;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsA[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (gRoamerCooldown > 0) {
        gRoamerCooldown--;
    }
    if (gRoamerReleasePending == 1 && gSceneCombatState.battleRefs >= 2) {
        gRoamerReleasePending = 0;
        sceneReleaseBattleRef(task, 0xD);
    }
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        gRoamerCooldown = 0x96;
        a               = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A);
        b               = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
        count           = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A, a + (b - count));
        count = 0;
        for (k = 0; k < 5; k++) {
            if (((s16*)gRoamerReserveHp)[k] > 0) {
                count++;
            }
        }
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, count);
        areaSyncLocationVariant(&gGameSession->location.loc);
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    if (gGameSession->battleResetPending == 1 && gRoamerCooldown == 0) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && gRoamerSpawnRequest != 0) {
        gRoamerCommand.context.loc.stage = 5;
        gRoamerCommand.context.loc.area  = 0x1D;
        gRoamerCommand.command           = 0xB;
        for (i = 0; i < 2; i++) {
            if (Gp_LookupSlot4(i) == 0) {
                break;
            }
            obj = Gp_LookupSlot4(i)->spawnArg2.pointer;
            if (obj == NULL) {
                break;
            }
            if (obj->hp == -999) {
                for (j = 0; j < gRoamerReserveCount; j++) {
                    if (((s16*)gRoamerReserveHp)[j] > 0) {
                        obj->hp             = gRoamerReserveHp[j];
                        obj->reactionFlags  = 0;
                        gRoamerReserveHp[j] = 0;
                        break;
                    }
                }
                if (obj->hp > 0) {
                    sceneAcquireBattleRef(0);
                    gRoamerCooldown += 0x5A;
                    TASK_MESSAGE_DISPATCH_POINTER(Gp_LookupSlot4(i), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[0]   = gRoamerSpawnPointsA[gRoamerSpawnRequest - 1].x;
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[1]   = 0;
                    Gp_LookupSlot4(i)->extra.tmd->coords->coord.t[2]   = gRoamerSpawnPointsA[gRoamerSpawnRequest - 1].z;
                    Gp_LookupSlot4(i)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
                    gfxRotMatrixY(&Gp_LookupSlot4(i)->extra.tmd->coords->coord,
                                  gRoamerSpawnPointsA[gRoamerSpawnRequest - 1].yaw, 1);
                }
                break;
            }
        }
    }
    gRoamerSpawnRequest = 0;
}
