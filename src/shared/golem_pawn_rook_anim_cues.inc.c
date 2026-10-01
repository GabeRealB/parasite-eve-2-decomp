/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Animation sound cues. While the enemy has a voice row (`field_6D6`), reads the
/// current record of animation slot 1 and plays the row's two cues on the
/// falling edge of its 0x20 and 0x10 bits, remembering them in `field_6A0`.
void golemPawnRookPlayAnimCues(Task* arg0)
{
    s32                    snd;
    s32                    pan;
    s32                    pan2;
    GolemPawnRookWork*     work;
    GfxCoord*              self;
    const AnimationRecord* rec;

    work = arg0->work;
    self = arg0->extra.tmd->coords;
    if (work->field_6D6 != 0) {
        rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
        if (rec != NULL) {
            if (!(rec->flags & ANIMATION_RECORD_CUE_2) && (work->field_6A0 & ANIMATION_RECORD_CUE_2)) {
                snd = gGolemPawnRookVoiceCues[work->field_6D6 * 2 - 1] |
                      ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(self));
            }
            if (!(rec->flags & ANIMATION_RECORD_CUE_1) && (work->field_6A0 & ANIMATION_RECORD_CUE_1)) {
                snd = gGolemPawnRookVoiceCues[work->field_6D6 * 2] |
                      ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan2 = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(snd, pan2, (s8)worldCoordGetOriginAudioDepth(self));
            }
            work->field_6A0 = (u16)(rec->flags & ANIMATION_RECORD_CUE_MASK);
        }
    }
}
