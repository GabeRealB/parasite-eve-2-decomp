/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Collects ordered occurrences of the two nearest nodes into the plan's slot lists.
///
/// Each list takes up to eight entries. Retains the count-indexed marker writes,
/// including the full-list overrun; valid caller orders have fewer than eight
/// occurrences of each node. Borrows the live plan and navigation records.
static inline void _bossStrangerCollectPlanSlots(const BossStrangerWalker* walker, BossStrangerPlanTowardScratch* plan)
{
    plan->actorSlotCount = 0;
    plan->selfSlotCount  = 0;
    for (plan->outerIndex = 0; plan->outerIndex < walker->nav->orderCount; plan->outerIndex++) {
        if (walker->nav->nodeOrder[plan->outerIndex] == plan->actorNode && plan->actorSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            plan->actorSlots[plan->actorSlotCount] = plan->outerIndex;
            plan->actorSlotCount++;
        }
        if (walker->nav->nodeOrder[plan->outerIndex] == plan->selfNode && plan->selfSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            plan->selfSlots[plan->selfSlotCount] = plan->outerIndex;
            plan->selfSlotCount++;
        }
    }

    // A full list has no room for its end marker; see the block's notes.
    plan->actorSlots[plan->actorSlotCount] = BOSS_STRANGER_PLAN_SLOT_END;
    plan->selfSlots[plan->selfSlotCount]   = BOSS_STRANGER_PLAN_SLOT_END;
}

/// Selects the closest slot pair and records the walker's starting slot and direction.
///
/// Ties keep the first pair. An empty list leaves the walker's cursor, goal slot
/// and direction untouched, with `bestGap` at its no-pair sentinel. The main
/// plan operation subsequently advances the cursor even in that case.
static inline void _bossStrangerSelectClosestPlanSlots(BossStrangerWalker* walker, BossStrangerPlanTowardScratch* plan)
{
    s32 slotGap;
    s32 previousBestGap;

    // Take the pair of slots, one from each list, that are closest together.
    plan->bestGap = BOSS_STRANGER_PLAN_GAP_NONE;
    for (plan->outerIndex = 0; plan->outerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; plan->outerIndex++) {
        if (plan->actorSlots[plan->outerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
            break;
        }
        for (plan->innerIndex = 0; plan->innerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; plan->innerIndex++) {
            if (plan->selfSlots[plan->innerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
                break;
            }
            slotGap         = plan->actorSlots[plan->outerIndex] - plan->selfSlots[plan->innerIndex];
            previousBestGap = plan->bestGap;
            plan->gap       = slotGap;
            slotGap         = ABS(slotGap);
            if (slotGap < previousBestGap) {
                plan->bestGap    = slotGap;
                walker->cursor   = plan->selfSlots[plan->innerIndex];
                walker->goalSlot = plan->actorSlots[plan->outerIndex];
                if (plan->gap < 0) {
                    walker->orderStep = -1;
                } else {
                    walker->orderStep = 1;
                }
            }
        }
    }
}

/// Plans one step along the navigation node order toward the selected player.
///
/// Finds the nodes nearest the player and walker, collects their order slots,
/// and selects the pair with the smallest absolute slot gap (first pair wins
/// ties). Sets `goalSlot`, sets `orderStep` to -1 or +1, and advances `cursor`
/// one slot from the selected walker-side slot, with byte wrap and no bound check.
/// If no pair exists, prints a diagnostic and still advances the old cursor
/// using the old direction; it does not leave the cursor unchanged.
///
/// Requires the nearest-node scan contracts, a readable `orderCount`-entry
/// `nodeOrder`, and a resulting cursor that names a valid order slot.
/// `playerId` is one-based; every known caller supplies the resident player 1.
/// Each scratch list accepts eight slots, then drops later matches, but its
/// marker is written at the count: eight player slots write one byte beyond
/// the block, and eight walker slots overwrite the first player slot with the
/// end marker, preventing any pair. Valid orders keep each count below eight;
/// both carriers name each node once. Borrows initialized scratch storage for
/// the plan and nested node scans; all walker/navigation pointers remain borrowed.
static void _bossStrangerPlanToward(BossStrangerWalker* walker, s16 playerId)
{
    BossStrangerPlanTowardScratch* plan;

    plan = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerPlanTowardScratch);

    // Collect the order slots that name each of the two nodes.
    plan->actorNode = _bossStrangerNodeNearestPlayer(walker, playerId);
    plan->selfNode  = _bossStrangerNodeNearestSelf(walker);
    _bossStrangerCollectPlanSlots(walker, plan);
    _bossStrangerSelectClosestPlanSlots(walker, plan);

    if (plan->bestGap == BOSS_STRANGER_PLAN_GAP_NONE) {
        printf(_gPatrolNoPairMsg);
    }
    walker->cursor += (u8)walker->orderStep;
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerPlanTowardScratch);
}
