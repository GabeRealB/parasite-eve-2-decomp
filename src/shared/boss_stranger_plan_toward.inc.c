/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Re-plans the walker's position in `nav`'s `nodeOrder` so that it heads
/// towards actor `actor`. It collects every slot of that order naming the
/// node nearest the actor and every slot naming the node nearest the walker,
/// then picks the pair of slots that are closest together: the walker's
/// `cursor` becomes the slot on its own side, `goalSlot` records the slot on
/// the actor's side, and `orderStep` becomes the +1 / -1 direction the cursor
/// has to travel along the order to close the gap -- which the caller then
/// applies, as does the last line here. Both lists hold at most eight slots,
/// so an order with more matches than that is silently truncated; if no pair
/// was found at all the routine only complains and leaves the cursor where it
/// was.
void bossStrangerPlanToward(BossStrangerWalker* work, s16 actor)
{
    BossStrangerPlanTowardScratch* s;
    s32                            gap;
    s32                            bestGap;

    s = SCRATCH_STACK_RESERVE_BLOCK(BossStrangerPlanTowardScratch);

    // Collect the order slots that name each of the two nodes.
    s->actorNode      = bossStrangerNodeNearestActor(work, actor);
    s->selfNode       = bossStrangerNodeNearestSelf(work);
    s->actorSlotCount = 0;
    s->selfSlotCount  = 0;
    for (s->outerIndex = 0; s->outerIndex < work->nav->orderCount; s->outerIndex++) {
        if (work->nav->nodeOrder[s->outerIndex] == s->actorNode && s->actorSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            s->actorSlots[s->actorSlotCount] = s->outerIndex;
            s->actorSlotCount++;
        }
        if (work->nav->nodeOrder[s->outerIndex] == s->selfNode && s->selfSlotCount < BOSS_STRANGER_PLAN_SLOT_CAPACITY) {
            s->selfSlots[s->selfSlotCount] = s->outerIndex;
            s->selfSlotCount++;
        }
    }

    // A full list has no room for its end marker; see the block's notes.
    s->actorSlots[s->actorSlotCount] = BOSS_STRANGER_PLAN_SLOT_END;
    s->selfSlots[s->selfSlotCount]   = BOSS_STRANGER_PLAN_SLOT_END;

    // Take the pair of slots, one from each list, that are closest together.
    s->bestGap = BOSS_STRANGER_PLAN_GAP_NONE;
    for (s->outerIndex = 0; s->outerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; s->outerIndex++) {
        if (s->actorSlots[s->outerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
            break;
        }
        for (s->innerIndex = 0; s->innerIndex < BOSS_STRANGER_PLAN_SLOT_CAPACITY; s->innerIndex++) {
            if (s->selfSlots[s->innerIndex] == BOSS_STRANGER_PLAN_SLOT_END) {
                break;
            }
            gap     = s->actorSlots[s->outerIndex] - s->selfSlots[s->innerIndex];
            bestGap = s->bestGap;
            s->gap  = gap;
            gap     = ABS(gap);
            if (gap < bestGap) {
                s->bestGap     = gap;
                work->cursor   = s->selfSlots[s->innerIndex];
                work->goalSlot = s->actorSlots[s->outerIndex];
                if (s->gap < 0) {
                    work->orderStep = -1;
                } else {
                    work->orderStep = 1;
                }
            }
        }
    }

    if (s->bestGap == BOSS_STRANGER_PLAN_GAP_NONE) {
        printf(_gPatrolNoPairMsg);
    }
    work->cursor += (u8)work->orderStep;
    SCRATCH_STACK_RELEASE_BLOCK(BossStrangerPlanTowardScratch);
}
