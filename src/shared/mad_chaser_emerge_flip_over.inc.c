/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Plays sounds 9 and 3 on the first frame and backs off 0x5A units a frame,
/// pitching toward 0x800, under the accelerating drop; on landing levels
/// out, turns around, requests animation 0x11 and advances the state.
void madChaserEmergeFlipOver(Task* arg0)
{
    MadChaserWork* work;
    MadChaserWork* anim;
    GfxCoord*      coord;
    s32            soundId;
    s32            pan;
    s32            soundId2;
    s32            pan2;
    s16            angle;
    s16            speed;

    work  = (MadChaserWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    work->field_412++;
    work->field_78 += (0x800 - work->field_78) >> 3;
    if ((s16)work->field_412 == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
        pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        SndEvt_EnqueueType6(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x5A;
    angle                                 = work->field_7A;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->field_42A;
    work->field_428                      += 4;
    work->field_42A                      += work->field_428;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1] = -0x3C;
        work->field_78    = 0;
        work->field_7C    = 0;
        work->field_7A   += 0x800;
        anim              = (MadChaserWork*)arg0->work;
        anim->field_41C   = 0x10;
        anim->field_418   = 0x11;
        anim->field_414   = 2;
        work->field_412   = 0;
        work->field_420++;
    }
}
