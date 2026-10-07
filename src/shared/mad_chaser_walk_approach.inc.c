/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Walks toward the player: keeps the speed at least the distance band's, turns
/// toward the player, steps forward at the animation speed and plays a footstep
/// at each slot-1 boundary, jump or hold. Advances once inside the random range with the player
/// ahead and the leap cooldown spent (or within 1500).
void madChaserWalkApproach(Task* arg0)
{
    MadChaserWork* work = (MadChaserWork*)arg0->work;
    s16            dist;
    s16            limit;
    s16            step;
    s16            angle;
    s16            speed;
    s32            soundId;
    s32            pan;

    dist = work->playerDist;
    if (dist < 1000) {
        limit = 0x10;
        step  = 0x10;
    } else if (dist < 2000) {
        step  = 0x12;
        limit = 0x14;
    } else if (dist < 3000) {
        step  = 0x14;
        limit = 0x18;
    } else if (dist < 4000) {
        step  = 0x16;
        limit = 0x1C;
    } else if (dist < 5000) {
        limit = 0x20;
        step  = 0x18;
    } else {
        limit = 0x40;
        step  = 0x20;
    }
    if (work->animRate < limit) {
        work->animRate = limit;
        work->turnStep = step;
    }
    _madChaserTurnToPlayer(arg0, work->turnStep);
    speed                                 = _madChaserScaleByAnimRate(arg0, -0x10);
    angle                                 = work->rotation.vy;
    arg0->extra.tmd->coords->coord.t[0]  += ((rsin(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->coord.t[2]  += ((rcos(angle) << 4) * speed) >> 0x10;
    arg0->extra.tmd->coords->composeStamp = GRAPHICS_COORD_DIRTY;
    if (_madChaserAnimHasBoundaryStatus(arg0)) {
        soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402C0001;
        pan     = (s8)worldCoordGetOriginAudioPan(arg0->extra.tmd->coords);
        sndEvtRequestScriptStart(soundId, pan, (s8)worldCoordGetOriginAudioDepth(arg0->extra.tmd->coords));
    }
    if (work->playerDist < work->leapRangeBonus + 2000 && (work->playerDist < 1500 || work->leapCooldown == 0) &&
        (u16)(((work->playerBearing + 0x800) & 0xFFF) - 0x200) > 0xC00) {
        work->subState++;
    }
}
