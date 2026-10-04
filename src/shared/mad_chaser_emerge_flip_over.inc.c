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
    work->stateFrames++;
    work->rotation.vx += (0x800 - work->rotation.vx) >> 3;
    if ((s16)work->stateFrames == 1) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0009;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
        soundId2 = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0003;
        pan2     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId2, pan2, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    speed                                 = -0x5A;
    angle                                 = work->rotation.vy;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    coord->coord.t[1]                    += work->moveSpeed;
    work->moveAccel                      += 4;
    work->moveSpeed                      += work->moveAccel;
    if (coord->coord.t[1] > 0) {
        coord->coord.t[1]  = -0x3C;
        work->rotation.vx  = 0;
        work->rotation.vz  = 0;
        work->rotation.vy += 0x800;
        anim               = (MadChaserWork*)arg0->work;
        anim->animRate     = ANIMATION_RATE_ONE;
        anim->animId       = 0x11;
        anim->animRequest  = MAD_CHASER_ANIM_REQUEST_RESET;
        work->stateFrames  = 0;
        work->state++;
    }
}
