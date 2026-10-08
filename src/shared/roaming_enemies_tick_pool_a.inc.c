/* Part of the roaming enemies library; see roaming_enemies.h. */

/// Returns the number of living reserve enemies across all five banked slots.
///
/// The u16 storage is tested as signed HP: zero and negative halfwords do not
/// count. Scans the complete bank regardless of the active reserve count and
/// returns 0..5 without consuming a slot.
static __inline__ s16 _roamerCountReserveSlots(void)
{
    s16 count;
    s16 slot;

    count = 0;
    for (slot = 0; slot < ARRAY_SIZE(gRoamerReserveHp); slot++) {
        if ((s16)gRoamerReserveHp[slot] > 0) {
            count++;
        }
    }
    return count;
}

/// Places a revived pool-A actor at the requested room spawn point.
///
/// actorIndex is 0..1 and its placed actor/model must remain live. The selector
/// is one-based (forest 1..5, woodland 1..6). Position is in the root's parent
/// frame and yaw uses 4096 units per turn. Marks dirty before replacing rotation.
static __inline__ void _roamerPlacePoolAActor(s16 actorIndex)
{
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[0]   = gRoamerSpawnPointsA[_gRoamerPendingSpawnPoint - 1].x;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[1]   = 0;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[2]   = gRoamerSpawnPointsA[_gRoamerPendingSpawnPoint - 1].z;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixY(&sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord,
                  gRoamerSpawnPointsA[_gRoamerPendingSpawnPoint - 1].yaw, GRAPHICS_ROTATION_REPLACE);
}

/// Advances pool A's battle bookkeeping and revives at most one waiting placed actor.
///
/// Requires initialized reserve storage (count 0..5), live room/session state
/// and location variant 0..13; an unarmed variant returns without consuming its
/// pending request. On the last battle release, remaining signed-positive slots
/// update the saved kill/reserve nibbles and impose a 150-tick cooldown.
/// A pending one-based point revives the first hp=-999 actor among slots 0..1,
/// takes the first positive reserve, acquires a battle hold, requests its leap-in
/// and places it. Forest triggers supply 1..5, woodland triggers 1..6;
/// no clamp is applied. A processed request is cleared even if no actor is found.
/// Placed tasks, models and enemies keep their existing ownership.
static void _roamerTickPoolA(Task* task)
{
    enum {
        ROAMER_POOL_A_PLACED_ACTORS   = 2,
        ROAMER_WAITING_ACTOR_HP       = -999,
        ROAMER_POOL_A_LEAP_IN_COMMAND = 11,
        ROAMER_POOL_A_RELEASE_ARG     = 13
    };
    s16    actorIndex;
    s16    remainingSlots;
    s32    killedCount;
    s32    previousReserveCount;
    Enemy* enemy;
    s16    reserveSlot;

    gameGetTaskSlot(GAME_TASK_SLOT_PLAYER);
    if (gRoamerArmCountsA[gGameSession->location.loc.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    if (gRoamerReleasePending == 1 && gSceneCombatState.battleRefs >= 2) {
        gRoamerReleasePending = 0;
        sceneReleaseBattleRef(task, ROAMER_POOL_A_RELEASE_ARG);
    }
    // Persist losses only when the previous frame still held a battle reference.
    if (gSceneCombatState.battleRefs == 0 && gRoamerPrevBattleRefs > 0) {
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
        killedCount            = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A);
        previousReserveCount   = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
        remainingSlots         = _roamerCountReserveSlots();
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A, killedCount + (previousReserveCount - remainingSlots));
        remainingSlots = _roamerCountReserveSlots();
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, remainingSlots);
        areaSyncLocationVariant(&gGameSession->location.loc);
    }
    gRoamerPrevBattleRefs = gSceneCombatState.battleRefs;
    if (gGameSession->battleResetPending == 1 && _gRoamerCooldownFrames == ROAMER_COOLDOWN_READY) {
        gSceneCombatState.signals.bytes.battlePhase = SCENE_COMBAT_BATTLE_IDLE;
        gSceneCombatState.peTargetCount             = 0;
        gSceneCombatState.battleRefs                = 0;
        gSceneCombatState.expReward                 = 0;
        gSceneCombatState.bpReward                  = 0;
        gSceneCombatState.mpReward                  = 0;
        gGameSession->battleResetPending            = 0;
    }
    if (gSceneCombatState.signals.bytes.battlePhase != SCENE_COMBAT_BATTLE_FINISHED && _gRoamerPendingSpawnPoint != ROAMER_SPAWN_POINT_NONE) {
        gRoamerCommand.context.loc.stage = GAME_STAGE_SHELTER_NEO_ARK;
        gRoamerCommand.context.loc.area  = GAME_AREA_NEO_ARK_WOODLAND_PATH;
        gRoamerCommand.command           = ROAMER_POOL_A_LEAP_IN_COMMAND;
        for (actorIndex = 0; actorIndex < ROAMER_POOL_A_PLACED_ACTORS; actorIndex++) {
            if (sceneFindPlacedActor(actorIndex) == 0) {
                break;
            }
            enemy = sceneFindPlacedActor(actorIndex)->spawnArg2.pointer;
            if (enemy == NULL) {
                break;
            }
            if (enemy->hp == ROAMER_WAITING_ACTOR_HP) {
                for (reserveSlot = 0; reserveSlot < gRoamerReserveCount; reserveSlot++) {
                    if (((s16*)gRoamerReserveHp)[reserveSlot] > 0) {
                        enemy->hp                     = gRoamerReserveHp[reserveSlot];
                        enemy->reactionFlags          = 0;
                        gRoamerReserveHp[reserveSlot] = 0;
                        break;
                    }
                }
                if (enemy->hp > 0) {
                    sceneAcquireBattleRef(0);
                    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                    TASK_MESSAGE_DISPATCH_POINTER(sceneFindPlacedActor(actorIndex), ACTOR_COMMAND_MESSAGE_APPLY, &gRoamerCommand, 0);
                    _roamerPlacePoolAActor(actorIndex);
                }
                break;
            }
        }
    }
    _gRoamerPendingSpawnPoint = ROAMER_SPAWN_POINT_NONE;
}
