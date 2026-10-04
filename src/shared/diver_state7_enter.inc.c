/* Part of the Diver library; see diver.h. */

/// Sub-state 0 of state 7: requests a blend into clip 0xA (6 frames, normal rate),
/// plays voice cue 6 and moves on to sub-state 1.
void diverState7Enter(Task* arg0)
{
    s32        sound;
    s32        pan;
    DiverWork* work;

    work              = arg0->work;
    work->animBlend   = 6;
    work->animStep    = ANIMATION_RATE_ONE;
    work->animClip    = 0xA;
    work->animRequest = DIVER_ANIM_REQUEST_BLEND;
    sound             = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40040006;
    pan               = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
    sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    work->subState++;
}
