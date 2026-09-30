/* Part of the web spider library; see web_spider.h. */

/// Status-effect step, run every frame while `field_3B0` is set. `field_3B2`
/// cycles through 0x50 frames; every 12 frames effect 3 is spawned, alternating
/// between model nodes 3 and 5; every 0x24 frames sound 0x401A0005 is played
/// with the top nibble of the context's `field_8` in bits 8-11, panned to the
/// actor.
void spiderBurnStep(Task* arg0)
{
    Actor105500Work* work;
    GfxCoord*        coord;
    s32              sound;
    s32              pan;
    u16              timer;
    u16              effectTimer;
    u16              countdown;

    work            = arg0->work;
    coord           = arg0->extra.tmd->coords;
    timer           = work->field_3B2 + 1;
    work->field_3B2 = timer;
    if ((s16)timer >= 0x50) {
        work->field_3B2 = 0U;
    }
    effectTimer     = work->field_3B4 + 1;
    work->field_3B4 = effectTimer;
    if ((s16)effectTimer == 0xC) {
        work->field_3B4 = 0U;
        if (work->field_3B6 == 0) {
            func_800FDB18(3, arg0->extra.tmd->coords + 3, NULL, &work->field_354);
            work->field_3B6 = 1;
        } else {
            func_800FDB18(3, arg0->extra.tmd->coords + 5, NULL, &work->field_354);
            work->field_3B6 = 0;
        }
    }
    countdown       = work->field_3BE - 1;
    work->field_3BE = countdown;
    if ((s16)countdown <= 0) {
        work->field_3BE = 0x24U;
        sound           = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0005;
        pan             = (s8)Gp_GetObjPan(coord);
        SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)gpGetObjDepth(coord));
    }
}
