/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Plays the body animation's two sound cues on their falling edges.
///
/// soundSet 0 disables cues; values 1..4 choose the carrier's footstep cue pair.
/// Slot 1 supplies cue flags, remembered only when a current record exists.
/// Sound requests carry the enemy's placement instance and the root's spatial pan
/// and depth; the task, rig and spawn Enemy remain borrowed.
static void _golemPawnRookPlayAnimCues(Task* actor)
{
    s32                    soundId;
    s32                    cue2Pan;
    s32                    cue1Pan;
    GolemPawnRookWork*     work;
    GfxCoord*              root;
    const AnimationRecord* record;

    /// Plays a carrier cue at the body origin with its placement instance tag.
    ///
    /// Borrows actor/root and the carrier's gGolemPawnRookVoiceCues table; cueIndex
    /// must be valid there. soundId and audioPan are writable s32 lvalues. Arguments
    /// must be side-effect-free; root and outputs occur repeatedly. The compound
    /// statement is confined to this handler and undefined below.
#define GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, cueIndex, soundId, audioPan)                                                                                                   \
    {                                                                                                                                                                             \
        (soundId)  = gGolemPawnRookVoiceCues[(cueIndex)] | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
        (audioPan) = (s8)worldCoordGetOriginAudioPan((root));                                                                                                                     \
        sndEvtRequestScriptStart((soundId), (audioPan), (s8)worldCoordGetOriginAudioDepth((root)));                                                                               \
    }

    work = actor->work;
    root = actor->extra.tmd->coords;
    if (work->soundSet != 0) {
        record = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
        if (record != NULL) {
            if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, work->soundSet * 2 - 1, soundId, cue2Pan);
            }
            if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->prevCueFlags & ANIMATION_RECORD_CUE_1)) {
                GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, work->soundSet * 2, soundId, cue1Pan);
            }
            work->prevCueFlags = (u16)(record->flags & ANIMATION_RECORD_CUE_MASK);
        }
    }

#undef GOLEM_PAWN_ROOK_PLAY_ANIM_CUE
}
