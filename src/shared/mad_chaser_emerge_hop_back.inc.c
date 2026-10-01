/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sound 9 on the first frame and hops backwards 0x50 units a frame,
/// easing the pitch back to zero, under the accelerating drop; on landing
/// advances the state.
void madChaserEmergeHopBack(Task* arg0)
{
    MadChaserWork* work;
    GfxCoord*      coord;
    s32            soundId;
    s32            pan;
    s16            angle;
    s16            speed;

    work  = (MadChaserWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_412++;
    work->field_78 += -work->field_78 >> 5;
    if ((s16)work->field_412 == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x50;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 4;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_412   = 0;
        work->field_420++;
    }
}
