/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Animation state 4: drives the two sound/tracking windows the same way
/// `func_actor_400600_801361AC` does, one frame-count pair per sound event.
void stalkerZebraIvoryStepClip4(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    GfxCoord*              coord;
    /* The first window starts at frame 0. `start0` is still its own `u8`: the
     * width is what folds both of its tests against a literal zero, and the
     * wider first temp below is what keeps the zero arm a fresh constant
     * instead of a copy of it. */
    u8  start0;
    u32 tmp0;
    u8  tmp1;
    u8  tmp2;
    u8  end0;
    u8  start1;
    u8  end1;
    s32 id;
    u32 sound;
    u32 voice;
    s32 pan;

    work  = (StalkerZebraIvoryWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->animClip != 4) {
        work->animStep    = 0x10;
        work->animBlend   = 4;
        work->animClip    = 4;
        work->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_BLEND;
        _stalkerZebraIvoryTickAnimInline(arg0);
    }
    start0 = 0;
    if (((StalkerZebraIvoryWork*)arg0->work)->animStep == 0) {
        tmp0 = 0;
    } else {
        tmp0 = (u32)(0xD00 / ((StalkerZebraIvoryWork*)arg0->work)->animStep) >> 4;
    }
    end0 = tmp0;
    if (((StalkerZebraIvoryWork*)arg0->work)->animStep == 0) {
        tmp1 = 0;
    } else {
        tmp1 = (u32)(0xE00 / ((StalkerZebraIvoryWork*)arg0->work)->animStep) >> 4;
    }
    start1 = tmp1;
    if (((StalkerZebraIvoryWork*)arg0->work)->animStep == 0) {
        tmp2 = 0;
    } else {
        tmp2 = (u32)(0x1B00 / ((StalkerZebraIvoryWork*)arg0->work)->animStep) >> 4;
    }
    end1 = tmp2;
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->animFrame = 0;
    }
    if (work->animFrame == start0) {
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 8, &work->anchorPos);
        id = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 1;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0001;
        }
        /* `voice` is a plain copy that the compiler propagates away; writing
         * `sound = id | sound` instead swaps the operands of the `or`. */
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame == start1) {
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        id = STALKER_ZEBRA_IVORY_STEP_SOUNDS | 2;
        if ((arg0->spawnArg1.value & 0xF0) == 0x10) {
            id = 0x404A0002;
        }
        sound   = ((Enemy*)arg0->spawnArg2.pointer)->placeKey;
        sound >>= 0xC;
        sound <<= 8;
        voice   = sound;
        sound   = id | voice;
        pan     = worldCoordGetOriginAudioPan(arg0->extra.tmd->coords) << 24;
        pan   >>= 24;
        sndEvtRequestScriptStart(sound, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->animFrame >= start0 && work->animFrame <= end0) {
        _stalkerZebraIvoryPinPartXZ(arg0, 8, &work->anchorPos);
    }
    if (work->animFrame >= start1 && work->animFrame <= end1) {
        _stalkerZebraIvoryPinPartXZ(arg0, 0xB, &work->anchorPos);
    }
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
