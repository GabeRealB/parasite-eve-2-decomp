/* Part of the actor contacts library; see actor_contacts.h. */

static s32 ActorContact_PushContact(GfxCoord* coord, WorldCollisionContact* rec, s16 arg2)
{
    OverlayDeltaFlag* s;
    s32               val;

    s        = SCRATCH_STACK_RESERVE_BLOCK(OverlayDeltaFlag);
    s->moved = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                    += s->delta.fixed.vx.word >> 16;
        coord->coord.t[2]                    += s->delta.fixed.vz.word >> 16;
        ActorContact_GetScratchPosition()->vx = s->delta.fixed.vx.word >> 16;
        ActorContact_GetScratchPosition()->vy = s->delta.fixed.vy.word >> 16;
        ActorContact_GetScratchPosition()->vz = s->delta.fixed.vz.word >> 16;
        val                                   = s->delta.fixed.vx.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                ActorContact_GetScratchPosition()->vx++;
            } else {
                coord->coord.t[0]--;
                ActorContact_GetScratchPosition()->vx--;
            }
        }
        val = s->delta.fixed.vz.word;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                ActorContact_GetScratchPosition()->vz++;
            } else {
                coord->coord.t[2]--;
                ActorContact_GetScratchPosition()->vz--;
            }
        }
    }
    if (s->delta.fixed.vx.word != 0 || s->delta.fixed.vz.word != 0) {
        s->moved = 1;
    }
    SCRATCH_STACK_RELEASE_BLOCK(OverlayDeltaFlag);
    return s->moved;
}
