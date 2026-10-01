/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Spawn the effect a hit record `arg2` names at one of twelve model offsets
/// picked by the signed damage `arg1`: the `gRandomLcgState` draw's low bits
/// bucket `|arg1|` into below 0x200 / above 0x600 / positive / non-positive,
/// each selecting from its own run of `gOddStrangerHitOffsets`. The chosen
/// offset goes into the work block's `field_8C0` and the `field_8B8` argument
/// record, which anchors it at the model's second coordinate part, scale
/// 0x300 and count 2 — the effect `func_800FDB18` then spawns hangs off the
/// part the vector's `pad` names. The 8-byte scratch the offset is built in is
/// carved off and given back around the call. Same body as
/// `Actor00100_Fn03340`, which keeps its record inline and scales by 0x100.
void oddStrangerSpawnHitEffect(Task* arg0, s16 arg1, s32 arg2)
{
    SVECTOR*         sc;
    s32              mag;
    OddStrangerWork* work;

    sc   = (SVECTOR*)SCRATCH_STACK_RESERVE_BYTES(8);
    mag  = (arg1 >= 0) ? arg1 : -arg1;
    work = arg0->work;
    if (mag < 0x200) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 3) {
            case 0:
                *sc = gOddStrangerHitOffsets[0];
                break;
            case 1:
                *sc = gOddStrangerHitOffsets[1];
                break;
            case 2:
                *sc = gOddStrangerHitOffsets[2];
                break;
            case 3:
                *sc = gOddStrangerHitOffsets[3];
                break;
            default:
                *sc = gOddStrangerHitOffsets[4];
                break;
        }
    } else if (mag > 0x600) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        switch ((s32)(gRandomLcgState >> 16) & 2) {
            case 0:
                *sc = gOddStrangerHitOffsets[5];
                break;
            case 1:
                *sc = gOddStrangerHitOffsets[6];
                break;
            default:
                *sc = gOddStrangerHitOffsets[7];
                break;
        }
    } else if (arg1 > 0) {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = gOddStrangerHitOffsets[8];
        } else {
            *sc = gOddStrangerHitOffsets[9];
        }
    } else {
        gRandomLcgState = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
        if ((gRandomLcgState >> 16) & 1) {
            *sc = gOddStrangerHitOffsets[10];
        } else {
            *sc = gOddStrangerHitOffsets[11];
        }
    }
    work->field_8B8.coord      = &arg0->extra.tmd->coords[1];
    work->field_8B8.spawnArgLo = 0x300;
    work->field_8B8.spawnArgHi = 2;
#if ODD_STRANGER_HIT_FX_OFFSET
    work->field_8C0 = *sc;
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], &work->field_8C0, &work->field_8B8);
#else
    func_800FDB18(Gp_GetIdParam1(arg2) & 0xFFFF, &arg0->extra.tmd->coords[sc->pad], sc, &work->field_8B8);
#endif
    SCRATCH_STACK_RELEASE_BYTES(8);
}
