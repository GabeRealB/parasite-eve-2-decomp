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

/// Finds the last body-obstacle contact's horizontal push in released scratch.
///
/// Borrows a live coordinate and `contactCount` readable contacts (0..32),
/// stopping at the first zero key. Returns 1 if a player/enemy body was found,
/// otherwise 0; frozen actors or a ready view return 0 without touching scratch.
/// Composes and then invalidates the coordinate. The push is capped at 256 world
/// units and remains in the released block rather than being returned. With no
/// obstacle, prior scratch push bytes still undergo the length/GTE operations;
/// neither a zero result nor release makes those bytes a valid new push.
static s32 _actorContactFindLastObstaclePush(GfxCoord* coord, const WorldCollisionContact* contacts, s16 contactCount)
{
    enum { ACTOR_CONTACT_LAST_OBSTACLE_MAX_PUSH = 256 };
    _ActorContactFindPushScratch* scratch;
    SVECTOR*                      pushDelta;

    if (gMcSaveData[MEMORY_CARD_SAVE_LIVE].state.actorsFrozen == 1 || gGameSession->viewReady == 1) {
        return 0;
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    scratch             = SCRATCH_STACK_RESERVE_BLOCK(_ActorContactFindPushScratch);
    actorRenderComposeCoord(coord);
    scratch->position.vx     = coord->workm.t[0];
    scratch->position.vy     = coord->workm.t[1];
    scratch->position.vz     = coord->workm.t[2];
    scratch->uncappedPush.vz = 0;
    scratch->uncappedPush.vy = 0;
    scratch->uncappedPush.vx = 0;
    scratch->hit             = 0;
    for (scratch->recordIndex = 0; scratch->recordIndex < contactCount; scratch->recordIndex++) {
        if (contacts[scratch->recordIndex].key.value == 0) {
            scratch->marks[scratch->recordIndex] = ACTOR_CONTACT_FIND_PUSH_MARK_END;
            break;
        }
        scratch->kind = contacts[scratch->recordIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK;
        if (scratch->kind == WORLD_COLLISION_CONTACT_PLAYER_BODY || scratch->kind == WORLD_COLLISION_CONTACT_ENEMY_BODY) {
            scratch->hit = 1;
            _actorContactCalcHorizontalPushback(&scratch->position, &contacts[scratch->recordIndex], &scratch->push);
            scratch->uncappedPush.vx = scratch->push.vx;
            scratch->uncappedPush.vz = scratch->push.vz;
        }
    }
    scratch->pushLength = SquareRoot0(scratch->push.vx * scratch->push.vx + scratch->push.vy * scratch->push.vy +
                                      scratch->push.vz * scratch->push.vz);
    // Preserve the no-obstacle path's reads from the uncleared scratch block.
    if (scratch->pushLength > ACTOR_CONTACT_LAST_OBSTACLE_MAX_PUSH) {
        pushDelta = &scratch->push;
        VectorNormalSS(pushDelta, pushDelta);
        gte_lddp(ACTOR_CONTACT_LAST_OBSTACLE_MAX_PUSH);
        gte_ldsv(pushDelta);
        gte_gpf12();
        gte_stsv(pushDelta);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
    SCRATCH_STACK_RELEASE_BLOCK(_ActorContactFindPushScratch);
    return scratch->hit;
}
