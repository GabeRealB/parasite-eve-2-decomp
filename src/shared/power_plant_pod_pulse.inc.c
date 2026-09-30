/* Part of the power plant pod library; see power_plant_pod.h. */

/// Idle schedule of the enemy, one of the steps the tick handler
/// `podTickState` runs each frame. The sub-state (`field_32C`)
/// picks what it does: state 0 walks `gPodIdlePulse` once the
/// countdown `field_32A` has run out, and on that table's terminator row
/// resets the row index, reseeds the countdown from the gameplay LCG and plays
/// the sound id `gPodPulseSoundId` with the placement number in the
/// high nibble of `Enemy::placeKey`; state 1 (entered on a hit) walks
/// `gPodHitPulse` and moves to state 2 on its terminator; state 2
/// returns to pose 1 and state 0 once the pose has run 0x23 frames past its
/// entry of `gPodPoseStartFrames`. The row's `field_2` is the scale
/// `modelPlacementSetScaled` applies to the saved coordinate matrix
/// `field_2FC`, 0x1000 when no row was read, and while the session's
/// `viewReady` is 1 the per-view row of `gPodViewSound` is enqueued
/// with the work block's sound id.
void podPulse(Task* arg0)
{
    Actor05300Work* work;
    GfxCoord*       coord;
    u16             scale;
    s32             pan;
    s32             sndId;

    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    scale = 0x1000;
    switch ((s16)work->field_32C) {
        case 0:
            if ((s16)work->field_32A <= 0) {
                scale = gPodIdlePulse[(s16)work->field_328].field_2;
                if (gPodIdlePulse[(s16)work->field_328].field_0 != 0) {
                    work->field_328 = 0;
                    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                    work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
                    sndId           = gPodPulseSoundId |
                            ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    pan = (s8)Gp_GetObjPan(coord);
                    SndEvt_EnqueueType6(sndId, pan, (s8)gpGetObjDepth(coord));
                } else {
                    work->field_328 = work->field_328 + 1;
                }
            } else {
                work->field_32A = work->field_32A - 1;
            }
            break;
        case 1:
            scale = gPodHitPulse[(s16)work->field_328].field_2;
            if (gPodHitPulse[(s16)work->field_328].field_0 != 0) {
                work->field_328 = 0;
                work->field_32C = 2;
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_32A = ((Gp_LcgState >> 16) & 0x3F) + 0x1E;
            } else {
                work->field_328 = work->field_328 + 1;
            }
            break;
        case 2:
            if ((s16)work->field_324 >= gPodPoseStartFrames[(s16)work->field_320] + 0x23) {
                work->field_320 = 1;
                work->field_32C = 0;
            }
            break;
    }
    modelPlacementSetScaled(arg0, &work->field_2FC, scale, 1);
    if (gGameSession->viewReady == 1) {
        SndEvt_EnqueueTypeA(work->field_31C, gPodViewSound[gGameSession->location.loc.view].field_0,
                            gPodViewSound[gGameSession->location.loc.view].field_2);
    }
}
