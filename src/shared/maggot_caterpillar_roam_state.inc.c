/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// `MAGGOT_CATERPILLAR_BEHAVIOUR_ROAM`: idles for its row's
/// `gMaggotCaterpillarRoamDelay` plus a random spread and turns toward the
/// player; then it sprays (`MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY`) when close,
/// not burning (`burning`) and the player is not in darkness - a Maggot only,
/// `isCaterpillar` clear - or pounces (`MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE`)
/// within `MAGGOT_CATERPILLAR_POUNCE_RANGE`.
void maggotCaterpillarRoamState(Task* arg0)
{
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    distance;
    s32                    sound;
    s32                    dx;
    s32                    dz;
    s32                    pan;
    u32                    random;
    u32                    random2;
    ActorFaceScratch*      delta;
    ActorFaceScratch*      scratchEnd;

    scratchEnd                                                                          = *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET);
    delta                                                                               = scratchEnd - 1;
    *(ActorFaceScratch**)PLAYSTATION_SCRATCHPAD_ADDRESS(SCRATCH_STACK_HEAD_BYTE_OFFSET) = delta;
    work                                                                                = arg0->work;
    state                                                                               = work->step;
    coord                                                                               = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (--work->stateCounter <= 0) {
                work->step         = 1;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_CRAWL;
                random             = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarRoamDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random >> 0x10) & 0x3FF);
                gRandomLcgState    = random;
                return;
            }
            return;
        case 1:
            scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            delta->delta.vy         = 0;
            delta->delta.vz         = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            work->targetYaw         = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)delta->delta.vz) & 0xFFF;
            work->turnRate          = 0x12;
            if (work->animFrame >= 0xB) {
                work->forwardSpeed = 0x17;
            }
            work->stateCounter -= work->forwardSpeed;
            if (work->stateCounter <= 0) {
                work->step         = 0;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                random2            = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)arg0->spawnArg2.pointer)->place->rowIndex] + ((random2 >> 0x10) & 0xF);
                gRandomLcgState    = random2;
                return;
            }
            if (work->animFrame == 0xC) {
                sound = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x401A0001;
                pan   = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(sound, (s32)pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= 0x29) {
                work->animFrame = 0xB;
            }
            if (work->targetYaw == work->yaw) {
                scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                delta->delta.vy         = 0;
                dz                      = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                delta->delta.vz         = dz;
                dx                      = scratchEnd[-1].delta.vx;
                distance                = SquareRoot0((dx * dx) + (dz * dz));
                if ((work->isCaterpillar == 0) && (distance < 0x578) && (work->burning == 0) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_SPRAY;
                    work->puffCount = 0;
                } else if (distance < MAGGOT_CATERPILLAR_POUNCE_RANGE) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                }
            }
            break;
    }
}
