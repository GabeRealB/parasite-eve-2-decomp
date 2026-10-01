/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Fires the cue pair the work block's `field_712` selects: while the second
/// animation slot carries `flags` bit 0x20 or 0x10, a sound is queued on the
/// frame that bit has just dropped from `GolemKnightBishopWork::field_6CA`, panned
/// and depth-attenuated from the actor's display object. The cue id is the
/// matching word of `gGolemKnightBishopAnimCues` with the `Enemy` work id's high
/// nibble in bits 8-11, and a zero `field_712` disarms the body. The record's
/// two bits are latched for the next frame at the end.
void golemKnightBishopPlayAnimCues(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    const AnimationRecord* rec;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->field_712 != 0) {
        rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_6CA & ANIMATION_RECORD_CUE_2)) {
                snd = gGolemKnightBishopAnimCues[work->field_712 * 2 - 1] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_6CA & ANIMATION_RECORD_CUE_1)) {
                snd  = gGolemKnightBishopAnimCues[work->field_712 * 2] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan2 = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            work->field_6CA = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
        }
    }
}
