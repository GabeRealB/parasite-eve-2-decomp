/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Rights the Stalker off its back: once the clip is done it turns half a
/// revolution, restarts clip 2, records part 0xB's view position, clears
/// `onBack` and returns to state 2. The Zebra build first pins part 0xE to the
/// recorded position (`STALKER_ZEBRA_IVORY_RIGHTING_PINS_PART`).
void stalkerZebraIvoryRightItself(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* work2;
    StalkerZebraIvoryWork* work3;
    GfxCoord*              coord;

    work  = (StalkerZebraIvoryWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
#if STALKER_ZEBRA_IVORY_RIGHTING_PINS_PART
    _stalkerZebraIvoryPinPartXZ(arg0, 0xE, &work->anchorPos);
#endif
    if ((_stalkerZebraIvoryClipDone(arg0) << 0x10) != 0) {
        work->yaw          = (work->yaw + 0x800) & 0xFFF;
        work2              = (StalkerZebraIvoryWork*)arg0->work;
        work2->animStep    = 0x10;
        work2->animClip    = 2;
        work2->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        _stalkerZebraIvoryApplyRotationInline(arg0);
        _stalkerZebraIvoryTickAnimInline(arg0);
        coord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(coord);
        _stalkerZebraIvoryReadPartWorldXZ(arg0, 0xB, &work->anchorPos);
        work->onBack    = 0;
        work3           = (StalkerZebraIvoryWork*)arg0->work;
        work3->state    = 2;
        work3->subState = 0;
    }
}
