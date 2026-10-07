/* Part of the actor contacts library; see actor_contacts.h. */

static s32 ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2)
{
    ActorContactPushScratch* block;
    s32                      val;

    block        = SCRATCH_STACK_RESERVE_BLOCK(ActorContactPushScratch);
    block->moved = 0;
    if (worldCollisionResolvePushback(rec, &block->delta, arg2, NULL) != WORLD_COLLISION_PUSHBACK_NO_GRID_HIT) {
        coord->coord.t[0]                 += block->delta.fixed.vx.word >> 16;
        coord->coord.t[2]                 += block->delta.fixed.vz.word >> 16;
        _actorContactGetLastPushStep()->vx = block->delta.fixed.vx.word >> 16;
        _actorContactGetLastPushStep()->vy = block->delta.fixed.vy.word >> 16;
        _actorContactGetLastPushStep()->vz = block->delta.fixed.vz.word >> 16;
        val                                = block->delta.fixed.vx.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                _actorContactGetLastPushStep()->vx++;
            } else {
                coord->coord.t[0]--;
                _actorContactGetLastPushStep()->vx--;
            }
        }
        val = block->delta.fixed.vz.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                _actorContactGetLastPushStep()->vz++;
            } else {
                coord->coord.t[2]--;
                _actorContactGetLastPushStep()->vz--;
            }
        }
    }
    if (block->delta.fixed.vx.word != 0 || block->delta.fixed.vz.word != 0) {
        block->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(ActorContactPushScratch);
    return block->moved;
}
