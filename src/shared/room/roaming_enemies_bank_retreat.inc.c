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
            if (gRoamerReserveHp[slotIndex] == 0) {
                bankedHp                    = hp * ROAMER_RETREAT_HP_PERCENT / 100;
                gRoamerReserveHp[slotIndex] = bankedHp;
                if (_gRoamerParams.hpMax < bankedHp) {
                    gRoamerReserveHp[slotIndex] = _gRoamerParams.hpMax;
                }
                if (gSceneCombatState.battleRefs >= 2) {
                    sceneReleaseBattleRef(task, 0xD);
                } else {
                    _gRoamerBattleReleasePending = ROAMER_BATTLE_RELEASE_PENDING;
                }
                _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
                return;
            }
        }
        return;
    }
    _gRoamerCooldownFrames += ROAMER_ACTION_COOLDOWN_FRAMES;
}

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

    if (_gRoamerReserveCount >= ROAMER_RESERVE_SLOTS + 1) {
        _gRoamerReserveCount = ROAMER_RESERVE_SLOTS;
    }
    for (reserveSlot = 0; reserveSlot < ROAMER_RESERVE_SLOTS; reserveSlot++) {
        if (reserveSlot < _gRoamerReserveCount) {
            gRoamerReserveHp[reserveSlot] = _gRoamerParams.hpMax;
        } else {
            gRoamerReserveHp[reserveSlot] = 0;
        }
    }
}

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

    if (_gRoamerPoolAArmCounts[gGameSession->location.variant] == 0) {
        task->msgTable = NULL;
        task->state    = task->state + 1;
        return;
    }
    task->msgTable       = _gRoamerPoolAMessages;
    _gRoamerReserveCount = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
    previousVariant      = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT);
    // Persist the accumulated count before limiting this visit's live slots.
    if (gGameSession->location.variant != previousVariant) {
        _gRoamerReserveCount = _gRoamerReserveCount + _gRoamerPoolAArmCounts[gGameSession->location.variant];
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, _gRoamerReserveCount);
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_VARIANT, gGameSession->location.variant);
    }
    _roamerSeedReserveHp();
    _gRoamerCooldownFrames = ROAMER_ACTION_COOLDOWN_FRAMES;
    task->state            = task->state + 1;
}

/// Returns the number of living reserve enemies across all five banked slots.
///
/// Positive signed-halfword HP denotes a live reserve; zero and negative
/// values do not count. Scans the complete bank regardless of the active reserve count and
/// returns 0..5 without consuming a slot.
static __inline__ s16 _roamerCountReserveSlots(void)
{
    s16 count;
    s16 slot;

    count = 0;
    for (slot = 0; slot < ARRAY_SIZE(gRoamerReserveHp); slot++) {
        if (gRoamerReserveHp[slot] > 0) {
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
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[0]   = _gRoamerPoolASpawnPoints[_gRoamerPendingSpawnPoint - 1].x;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[1]   = 0;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord.t[2]   = _gRoamerPoolASpawnPoints[_gRoamerPendingSpawnPoint - 1].z;
    sceneFindPlacedActor(actorIndex)->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixY(&sceneFindPlacedActor(actorIndex)->extra.tmd->coords->coord,
                  _gRoamerPoolASpawnPoints[_gRoamerPendingSpawnPoint - 1].yaw, GRAPHICS_ROTATION_REPLACE);
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
    if (_gRoamerPoolAArmCounts[gGameSession->location.variant] == 0) {
        return;
    }
    if (_gRoamerCooldownFrames > ROAMER_COOLDOWN_READY) {
        _gRoamerCooldownFrames--;
    }
    if (_gRoamerBattleReleasePending == ROAMER_BATTLE_RELEASE_PENDING && gSceneCombatState.battleRefs >= 2) {
        _gRoamerBattleReleasePending = ROAMER_BATTLE_RELEASE_NONE;
        sceneReleaseBattleRef(task, ROAMER_POOL_A_RELEASE_ARG);
    }
    // Persist losses only when the previous frame still held a battle reference.
    if (gSceneCombatState.battleRefs == 0 && _gRoamerPreviousBattleRefs > 0) {
        _gRoamerCooldownFrames = ROAMER_POST_BATTLE_COOLDOWN_FRAMES;
        killedCount            = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A);
        previousReserveCount   = gameFlagGetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE);
        remainingSlots         = _roamerCountReserveSlots();
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_KILLS_POOL_A, killedCount + (previousReserveCount - remainingSlots));
        remainingSlots = _roamerCountReserveSlots();
        gameFlagSetNibble(GAME_FLAG_NEO_ARK_ROAMER_POOL_A_RESERVE, remainingSlots);
        areaSyncLocationVariant(&gGameSession->location);
    }
    _gRoamerPreviousBattleRefs = gSceneCombatState.battleRefs;
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
                for (reserveSlot = 0; reserveSlot < _gRoamerReserveCount; reserveSlot++) {
                    if (gRoamerReserveHp[reserveSlot] > 0) {
                        enemy->hp                     = (u16)gRoamerReserveHp[reserveSlot];
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
