/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Converts a normal-rate crawl bound to byte-truncated update ticks.
///
/// Zero playback rate returns zero. Signed division precedes unsigned shifting;
/// the caller supplies nonnegative frames whose eight-bit shift fits s32.
static __inline__ u8 _stalkerZebraIvoryCrawlFrameToTicks(Task* task, s32 normalFrame)
{
    enum { STALKER_ZEBRA_IVORY_CRAWL_FRAME_FRACTION_BITS = 8,
           STALKER_ZEBRA_IVORY_CRAWL_RATE_FRACTION_BITS  = 4 };
    StalkerZebraIvoryWork* frameWork = task->work;

    if (frameWork->animStep == 0) {
        return 0;
    }
    return (u32)((normalFrame << STALKER_ZEBRA_IVORY_CRAWL_FRAME_FRACTION_BITS) / frameWork->animStep) >> STALKER_ZEBRA_IVORY_CRAWL_RATE_FRACTION_BITS;
}

/// Starts one crawl step sound using the enemy's placement voice and root position.
///
/// Water-entrance spawns select `waterScriptId`; others use `baseScriptId`.
/// Pan and depth retain only the signed low byte of the origin-audio queries.
static __inline__ void _stalkerZebraIvoryPlayCrawlStep(Task* task, s32 baseScriptId, s32 waterScriptId)
{
    enum { STALKER_ZEBRA_IVORY_CRAWL_SPAWN_KIND_MASK = 0xF0,
           STALKER_ZEBRA_IVORY_CRAWL_SPAWN_WATER     = 0x10,
           STALKER_ZEBRA_IVORY_CRAWL_VOICE_SHIFT     = 8 };
    u32 soundId;
    u32 placementVoice;
    s32 soundPan;

    if ((task->spawnArg1.value & STALKER_ZEBRA_IVORY_CRAWL_SPAWN_KIND_MASK) == STALKER_ZEBRA_IVORY_CRAWL_SPAWN_WATER) {
        baseScriptId = waterScriptId;
    }
    soundId        = ((Enemy*)task->spawnArg2.pointer)->placeKey;
    soundId      >>= ENEMY_PLACE_INDEX_SHIFT;
    soundId      <<= STALKER_ZEBRA_IVORY_CRAWL_VOICE_SHIFT;
    placementVoice = soundId;
    soundId        = baseScriptId | placementVoice;
    soundPan       = (s8)worldCoordGetOriginAudioPan(task->extra.tmd->coords);
    sndEvtRequestScriptStart(soundId, soundPan, (s8)worldCoordGetOriginAudioDepth(task->extra.tmd->coords));
}

/// Advances the hand-anchored crawl on the Stalker's back.
///
/// Blends into clip 4 at normal rate if needed, then loops the counter at any
/// slot-1 completion. Right/left hands anchor world X/Z during normal-rate
/// frames 0..13 and 14..27 inclusive, sounding each window's first tick.
/// Bounds narrow to u8 after signed division and unsigned shifting; zero rate
/// produces zero bounds. The caller ticks ongoing playback and applies yaw.
/// Requires the initialized rig, live model parts 8/11 and the Enemy spawn
/// record; uses the part-query scratch space and preserves root Y.
static void _stalkerZebraIvoryTickOnBackCrawl(Task* task)
{
    enum {
        STALKER_ZEBRA_IVORY_CRAWL_CLIP              = 4,
        STALKER_ZEBRA_IVORY_CRAWL_BLEND_FRAMES      = 4,
        STALKER_ZEBRA_IVORY_CRAWL_RIGHT_HAND        = 8,
        STALKER_ZEBRA_IVORY_CRAWL_LEFT_HAND         = 11,
        STALKER_ZEBRA_IVORY_CRAWL_RIGHT_END_FRAME   = 13,
        STALKER_ZEBRA_IVORY_CRAWL_LEFT_START_FRAME  = 14,
        STALKER_ZEBRA_IVORY_CRAWL_LEFT_END_FRAME    = 27,
        STALKER_ZEBRA_IVORY_CRAWL_SOUND_RIGHT       = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 1,
        STALKER_ZEBRA_IVORY_CRAWL_SOUND_LEFT        = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 2,
        STALKER_ZEBRA_IVORY_CRAWL_SOUND_WATER_RIGHT = 0x404A0001,
        STALKER_ZEBRA_IVORY_CRAWL_SOUND_WATER_LEFT  = 0x404A0002,
    };
    StalkerZebraIvoryWork* work;
    GfxCoord*              rootCoord;
    // Keep the frame-zero start and byte-truncated window bounds.
    u8  rightStart;
    u32 rightEndTicks;
    u8  leftStartTicks;
    u8  leftEndTicks;
    u8  rightEnd;
    u8  leftStart;
    u8  leftEnd;

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
    if (work->animClip != STALKER_ZEBRA_IVORY_CRAWL_CLIP) {
        work->animStep    = ANIMATION_RATE_ONE;
        work->animBlend   = STALKER_ZEBRA_IVORY_CRAWL_BLEND_FRAMES;
        work->animClip    = STALKER_ZEBRA_IVORY_CRAWL_CLIP;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        _stalkerZebraIvoryTickAnimInline(task);
    }
    rightStart     = 0;
    rightEndTicks  = _stalkerZebraIvoryCrawlFrameToTicks(task, STALKER_ZEBRA_IVORY_CRAWL_RIGHT_END_FRAME);
    rightEnd       = rightEndTicks;
    leftStartTicks = _stalkerZebraIvoryCrawlFrameToTicks(task, STALKER_ZEBRA_IVORY_CRAWL_LEFT_START_FRAME);
    leftStart      = leftStartTicks;
    leftEndTicks   = _stalkerZebraIvoryCrawlFrameToTicks(task, STALKER_ZEBRA_IVORY_CRAWL_LEFT_END_FRAME);
    leftEnd        = leftEndTicks;
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->animFrame = 0;
    }
    if (work->animFrame == rightStart) {
        _stalkerZebraIvoryReadPartWorldXZ(task, STALKER_ZEBRA_IVORY_CRAWL_RIGHT_HAND, &work->anchorPos);
        _stalkerZebraIvoryPlayCrawlStep(task, STALKER_ZEBRA_IVORY_CRAWL_SOUND_RIGHT, STALKER_ZEBRA_IVORY_CRAWL_SOUND_WATER_RIGHT);
    }
    if (work->animFrame == leftStart) {
        _stalkerZebraIvoryReadPartWorldXZ(task, STALKER_ZEBRA_IVORY_CRAWL_LEFT_HAND, &work->anchorPos);
        _stalkerZebraIvoryPlayCrawlStep(task, STALKER_ZEBRA_IVORY_CRAWL_SOUND_LEFT, STALKER_ZEBRA_IVORY_CRAWL_SOUND_WATER_LEFT);
    }
    if (work->animFrame >= rightStart && work->animFrame <= rightEnd) {
        _stalkerZebraIvoryPinPartXZ(task, STALKER_ZEBRA_IVORY_CRAWL_RIGHT_HAND, &work->anchorPos);
    }
    if (work->animFrame >= leftStart && work->animFrame <= leftEnd) {
        _stalkerZebraIvoryPinPartXZ(task, STALKER_ZEBRA_IVORY_CRAWL_LEFT_HAND, &work->anchorPos);
    }
    rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
}
