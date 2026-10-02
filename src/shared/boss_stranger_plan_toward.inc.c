/* Part of the Boss Stranger library; see boss_stranger.h. */

/// Re-plans the walker's position in `nav`'s `nodeOrder` so that it heads
/// towards actor `actor`. It collects every slot of that order naming the
/// node nearest the actor and every slot naming the node nearest the walker,
/// then picks the pair of slots that are closest together: the walker's
/// `cursor` becomes the slot on its own side, `field_75` records the slot on
/// the actor's side, and `field_73` becomes the +1 / -1 direction the cursor
/// has to travel along the order to close the gap -- which the caller then
/// applies, as does the last line here. Both lists hold at most eight slots,
/// so an order with more matches than that is silently truncated; if no pair
/// was found at all the routine only complains and leaves the cursor where it
/// was.
void bossStrangerPlanToward(OverlayWalker* work, s16 actor)
{
    OverlayWalkerRouteScratch* s;
    u8*                        head;
    s32                        diff;
    s32                        best;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x1C;
    s                        = (OverlayWalkerRouteScratch*)(head - 0x1C);

    s->nodeA  = bossStrangerNodeNearestActor(work, actor);
    s->nodeB  = bossStrangerNodeNearestSelf(work);
    s->countA = 0;
    s->countB = 0;
    for (s->i = 0; s->i < work->nav->orderCount; s->i++) {
        if (work->nav->nodeOrder[s->i] == s->nodeA && s->countA < 8) {
            s->listA[s->countA] = s->i;
            s->countA++;
        }
        if (work->nav->nodeOrder[s->i] == s->nodeB && s->countB < 8) {
            s->listB[s->countB] = s->i;
            s->countB++;
        }
    }

    s->listA[s->countA] = 0xFF;
    s->listB[s->countB] = 0xFF;
    s->best             = 0xFF;
    for (s->i = 0; s->i < 8; s->i++) {
        if (s->listA[s->i] == 0xFF) {
            break;
        }
        for (s->j = 0; s->j < 8; s->j++) {
            if (s->listB[s->j] == 0xFF) {
                break;
            }
            diff    = s->listA[s->i] - s->listB[s->j];
            best    = s->best;
            s->diff = diff;
            diff    = ABS(diff);
            if (diff < best) {
                s->best        = diff;
                work->cursor   = s->listB[s->j];
                work->field_75 = s->listA[s->i];
                if (s->diff < 0) {
                    work->field_73 = -1;
                } else {
                    work->field_73 = 1;
                }
            }
        }
    }

    if (s->best == 0xFF) {
        printf(_gPatrolNoPairMsg);
    }
    work->cursor += (u8)work->field_73;
    SCRATCH_STACK_RELEASE_BYTES(0x1C);
}
