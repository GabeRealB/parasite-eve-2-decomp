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

    /// Requests an animation sound script at the composed body origin.
    ///
    /// actor borrows a live `Enemy` at spawnArg2.pointer; root is composed into the
    /// active view. Captures the carrier's `_gGolemPawnRookMotionCues[17]`, whose
    /// script IDs have zero instance bytes. cueIndex is evaluated once and must
    /// be in 0..16; animation callers use 1..8 from sound sets 1..4. The placement
    /// index (0..15) is inserted at bits 8..11. soundId/audioPan are writable s32
    /// lvalues; pan is sampled before depth and both narrow to signed bytes. The
    /// pan query changes GTE state and borrows projection scratch. Arguments must
    /// be side-effect-free; root and outputs occur repeatedly. Use the compound
    /// statement only as a standalone invocation; it is undefined below.
#define GOLEM_PAWN_ROOK_PLAY_ANIM_CUE(actor, root, cueIndex, soundId, audioPan)                                                                                                     \
    {                                                                                                                                                                               \
        (soundId)  = _gGolemPawnRookMotionCues[(cueIndex)] | ((((Enemy*)(actor)->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_PAWN_ROOK_SOUND_INSTANCE_SHIFT); \
        (audioPan) = (s8)worldCoordGetOriginAudioPan((root));                                                                                                                       \
        sndEvtRequestScriptStart((soundId), (audioPan), (s8)worldCoordGetOriginAudioDepth((root)));                                                                                 \
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
