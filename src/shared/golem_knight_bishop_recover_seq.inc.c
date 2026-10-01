/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Rolls the actor's cue countdown. State 0 puts the slot set on animation
/// 0xB and drops the state to 1, arming `field_6D4` from the `gRandomLcgState` LCG
/// (`GOLEM_KNIGHT_BISHOP_RECOVER_DELAY` plus 0..0x1F); while no flinch is already running it also raises the hit
/// descriptor `field_494`/`field_49A`. State 1 ticks `field_6D4` down and, on
/// the frame it runs out, arms the `field_6DA`/`field_6DC`/`field_6DE`/
/// `field_6E0` timers, clears the state and `field_6CC`, and queues the actor's
/// cue, panned and depth-attenuated from the display object.
void golemKnightBishopRecoverSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    pan;
    u32                    random;
    s16                    timer;

    SCRATCH_STACK_RESERVE_BYTES(8);
    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->field_6C0 = 0xB;
            work->field_6CE = 1;
            random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
            gRandomLcgState = random;
            work->field_6D4 = (u16)(((random >> 16) & 0x1F) + GOLEM_KNIGHT_BISHOP_RECOVER_DELAY);
            if (work->field_6C6 == 0) {
                work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_494  = work->field_716 | 0x30000;
            }
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6DA = 3;
                work->field_6DC = 0xA;
                work->field_6CC = 0;
                work->field_6CE = 0;
                work->field_6DE = 5;
                work->field_6E0 = 0;
                work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan             = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(work->field_6BC, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(8);
}
