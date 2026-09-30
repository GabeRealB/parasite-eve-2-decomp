/* Part of the cloaked stalker library; see cloaked_stalker.h. */

/// Runs the actor's animation-reseed sequence. State 0 puts the slot set on
/// animation 8, clears `field_6C8` and drops the state to 1; unless the mode at
/// `field_6EC` is already 1 it also arms the `field_6DA`/`field_6DC`/`field_6DE`
/// timers and queues the actor's cue, panned and depth-attenuated from the
/// display object. State 1 waits for the animation to reach 0x37 frames and
/// then puts the state back to 0, flipping the mode to 2 and raising
/// `field_6CC` if it was 1.
void stalkerLightFlinchSeq(Task* arg0)
{
    Actor402200Work* work;
    GfxCoord*        coord;
    s32              state;
    s32              pan;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_6C0 = 8;
            work->field_6CE = 1;
            work->field_6C8 = 0;
            if (work->field_6EC != 1) {
                work->field_6DA = 3;
                work->field_6DC = 0x1E;
                work->field_6DE = 0xF;
                work->field_6BC = gStalkerPainCue | (((u16)((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan             = (s8)Gp_GetObjPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)gpGetObjDepth(coord));
                break;
            }
            break;
        case 1:
            if (work->field_6C4 >= 0x37) {
                if (work->field_6EC == state) {
                    work->field_6CC = 4;
                    work->field_6EC = 2;
                } else {
                    work->field_6CC = 0;
                }
                work->field_6CE = 0;
            }
            break;
    }
}
