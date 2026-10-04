/* Part of the actor contacts library; see actor_contacts.h. */

/// Value a `_ActorContactFindPushScratch::marks` slot takes at the record
/// that ends the walk early.
enum { ACTOR_CONTACT_FIND_PUSH_MARK_END = 0x7FFE };

/// Scratch-stack block of the walk that finds the push moving a coordinate
/// out of the obstacles among its contact records.
///
/// The walk reserves one block, visits the contact table up to the caller's
/// record count or the first record with no key, and for each obstacle record
/// (key kind 0x10000 or 0x30000) replaces `push` with the push out of that
/// record, so the last obstacle in the table decides it. `push` is then
/// shortened to 0x100 units when it is longer. The block is released before
/// the walk returns `hit`, which it reads while the bytes are still intact;
/// `push` is left in the released bytes the same way and nothing else returns
/// it.
///
/// Nothing clears the block when it is reserved, so `push` holds whatever the
/// scratch stack held when no record is an obstacle, and its length is taken
/// all the same. `marks` has one slot per contact record and the walk does not
/// clamp the count, so a caller must pass no more than its 32 slots.
typedef struct {
    byte    unknown_0[0x20]; // Reserved with the block and never accessed; role unproven
    SVECTOR push;            // Push out of the latest obstacle record, turned by the transpose of the room grid's view rotation, with `vy` zero; rescaled to length 0x100 when longer
    SVECTOR uncappedPush;    // Zeroed, then given `push`'s X and Z at each obstacle record, before the shortening; nothing reads it
    SVECTOR position;        // The coordinate's composed translation, each component cut to 16 bits: the point the push is measured from
    s32     kind;            // Kind bits of the current record's key
    u32     pushLength;      // Length of `push` over all three axes, world units
    s16     marks[32];       // One slot per contact record. Only the slot of a keyless record is written, with `ACTOR_CONTACT_FIND_PUSH_MARK_END`, and nothing reads any; role otherwise unproven
    s16     recordIndex;     // Contact record the walk is on
    byte    unknown_82[0x4]; // Reserved with the block and never accessed; role unproven
    s16     hit;             // 1 once an obstacle record was met, 0 otherwise
} _ActorContactFindPushScratch;
STATIC_ASSERT_SIZEOF(_ActorContactFindPushScratch, 0x88);

/// Walks the first `count` contact records (stopping at a zero key) and keeps,
/// in a scratch block carved off the scratch stack, the push that would move
/// `coord` out of the last record of kind 0x10000 or 0x30000, scaled down to
/// 0x100 units when longer. Returns whether any such record was found; returns
/// 0 at once when `gGameSession->viewReady` or `gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen` is 1.
static s32 ActorContact_FindPush(GfxCoord* coord, WorldCollisionContact* recs, s16 count)
{
    _ActorContactFindPushScratch* head;
    _ActorContactFindPushScratch* s;
    _ActorContactFindPushScratch* blk;
    SVECTOR*                      offset;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->composeStamp                                = GRAPHICS_COORD_DIRTY;
    head                                               = SCRATCH_STACK_CURSOR(_ActorContactFindPushScratch);
    blk                                                = head - 1;
    SCRATCH_STACK_CURSOR(_ActorContactFindPushScratch) = blk;
    s                                                  = blk;
    Gp_UpdateCoord(coord);
    s->position.vx     = coord->workm.t[0];
    s->position.vy     = coord->workm.t[1];
    s->position.vz     = coord->workm.t[2];
    s->uncappedPush.vz = 0;
    s->uncappedPush.vy = 0;
    s->uncappedPush.vx = 0;
    s->hit             = 0;
    for (s->recordIndex = 0; s->recordIndex < count; s->recordIndex++) {
        if (recs[s->recordIndex].key.value == 0) {
            s->marks[s->recordIndex] = ACTOR_CONTACT_FIND_PUSH_MARK_END;
            break;
        }
        s->kind = recs[s->recordIndex].key.value & 0xFFFF0000;
        if (s->kind == 0x10000 || s->kind == 0x30000) {
            s->hit = 1;
            actorCalcPush(&s->position, &recs[s->recordIndex], &s->push);
            s->uncappedPush.vx = s->push.vx;
            s->uncappedPush.vz = s->push.vz;
        }
    }
    s->pushLength = SquareRoot0(s->push.vx * s->push.vx + s->push.vy * s->push.vy +
                                s->push.vz * s->push.vz);
    if (s->pushLength > 0x100) {
        offset = &s->push;
        VectorNormalSS(offset, offset);
        gte_lddp(0x100);
        gte_ldsv(offset);
        gte_gpf12();
        gte_stsv(offset);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(_ActorContactFindPushScratch);
    return s->hit;
}
