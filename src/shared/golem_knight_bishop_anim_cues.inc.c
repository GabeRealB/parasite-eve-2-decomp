/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Queues a GOLEM cue with placement-instance selection and spatial audio.
///
/// `root` is a `GfxCoord*` with a composed view matrix; `placeKey` is the Enemy's
/// unsigned halfword placement key, promoted before extracting its index.
/// `soundId` is a base script ID. Invoke as a standalone statement inside a
/// braced block. Root is evaluated twice, the integers once each; arguments
/// must have no side effects. Captures the enclosing function's writable s32
/// `instanceSoundId`, which must not alias an argument; pan is local to the block.
#define GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, soundId, placeKey)                                                         \
    {                                                                                                                      \
        enum { GOLEM_KNIGHT_BISHOP_CUE_INSTANCE_SHIFT = 8 };                                                               \
        s32 audioPan;                                                                                                      \
        instanceSoundId = (soundId) | (((placeKey) >> ENEMY_PLACE_INDEX_SHIFT) << GOLEM_KNIGHT_BISHOP_CUE_INSTANCE_SHIFT); \
        audioPan        = (s8)worldCoordGetOriginAudioPan((root));                                                         \
        sndEvtRequestScriptStart(instanceSoundId, audioPan, (s8)worldCoordGetOriginAudioDepth((root)));                    \
    }

/// Plays spatial sound scripts on falling edges of the GOLEM's animation cues.
///
/// `task` owns a live rig/model and borrows its Enemy through `spawnArg2`.
/// Room sound sets 1..4 select paired script IDs; set 0 disables cues.
/// Slot 1 supplies cue bits 0x20 and 0x10, played in that order if both fall.
/// The Enemy placement index occupies script bits 8..11. Pan and depth narrow
/// to signed bytes. Disabled cues and buffered poses retain the previous bits.
static void _golemKnightBishopPlayAnimCues(Task* task)
{
    s32                    instanceSoundId;
    GolemKnightBishopWork* work;
    GfxCoord*              root;
    const AnimationRecord* record;

    work = task->work;
    root = task->extra.tmd->coords;
    if (work->soundSet != 0) {
        record = animationGetCurrentRecord(&work->rig.anim, &work->rig.slots[1]);
        if (record != NULL) {
            if (!(record->flags & ANIMATION_RECORD_CUE_2) && (work->prevCueFlags & ANIMATION_RECORD_CUE_2)) {
                GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, gGolemKnightBishopAnimCues[work->soundSet * 2 - 1], ((Enemy*)task->spawnArg2.pointer)->placeKey);
            }
            if (!(record->flags & ANIMATION_RECORD_CUE_1) && (work->prevCueFlags & ANIMATION_RECORD_CUE_1)) {
                GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE(root, gGolemKnightBishopAnimCues[work->soundSet * 2], ((Enemy*)task->spawnArg2.pointer)->placeKey);
            }
            work->prevCueFlags = record->flags & ANIMATION_RECORD_CUE_MASK;
        }
    }
}

#undef GOLEM_KNIGHT_BISHOP_PLAY_ANIM_CUE
