/* Part of the Maggot/Caterpillar library; see maggot_caterpillar.h. */

/// Alternates timed idle with a crawl toward the player and chooses an attack.
///
/// The placement row must fit the eight-entry idle and crawl-distance tables.
/// Idle timing comes from `gMaggotCaterpillarIdleDelay`; the crawl counter
/// spends coordinate units, including a random 0..1023-unit spread. A facing
/// Maggot can spray if not burning and the player is not blinded; either kind
/// can pounce in range. Player and actor roots must share a parent frame.
///
/// One 24-byte scratch block stays reserved until the next game-frame reset.
/// Each roaming instance adds another reservation; subsequent scratch users
/// must fit the space left below the cursor.
static void _maggotCaterpillarRoamState(Task* actor)
{
    enum {
        MAGGOT_CATERPILLAR_ROAM_IDLE         = 0,
        MAGGOT_CATERPILLAR_ROAM_CRAWL        = 1,
        MAGGOT_CATERPILLAR_ROAM_SPRAY_RANGE  = 1400,
        MAGGOT_CATERPILLAR_ROAM_TURN_RATE    = 18,
        MAGGOT_CATERPILLAR_ROAM_SPEED        = 23,
        MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME = 11,
        MAGGOT_CATERPILLAR_ROAM_SOUND_FRAME  = 12,
        MAGGOT_CATERPILLAR_ROAM_LOOP_FRAME   = 41,
        MAGGOT_CATERPILLAR_ROAM_SOUND        = 0x401A0001,
    };
    MaggotCaterpillarWork* work;
    GfxCoord*              coord;
    s32                    step;
    s32                    playerDistance;
    s32                    soundKey;
    s32                    playerOffsetX;
    s32                    playerOffsetZ;
    s32                    crawlPan;
    u32                    crawlRandom;
    u32                    idleRandom;
    ActorFaceScratch*      scratch;
    ActorFaceScratch*      scratchEnd;

    scratchEnd                             = SCRATCH_STACK_CURSOR(ActorFaceScratch);
    scratch                                = scratchEnd - 1;
    SCRATCH_STACK_CURSOR(ActorFaceScratch) = scratch;
    work                                   = actor->work;
    step                                   = work->step;
    coord                                  = actor->extra.tmd->coords;
    switch (step) {
        case MAGGOT_CATERPILLAR_ROAM_IDLE:
            work->reactionMode = MAGGOT_CATERPILLAR_REACTION_NORMAL;
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (--work->stateCounter <= 0) {
                work->step         = MAGGOT_CATERPILLAR_ROAM_CRAWL;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_CRAWL;
                crawlRandom        = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarRoamDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((crawlRandom >> 0x10) & 0x3FF);
                gRandomLcgState    = crawlRandom;
                return;
            }
            return;
        case MAGGOT_CATERPILLAR_ROAM_CRAWL:
            scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
            scratch->delta.vy       = 0;
            scratch->delta.vz       = (s32)(gPlayerStatus.coordMtx->t[2] - coord->coord.t[2]);
            work->targetYaw         = ratan2((s32)(s16)scratchEnd[-1].delta.vx, (s32)(s16)scratch->delta.vz) & (ONE - 1);
            work->turnRate          = MAGGOT_CATERPILLAR_ROAM_TURN_RATE;
            if (work->animFrame >= MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME) {
                work->forwardSpeed = MAGGOT_CATERPILLAR_ROAM_SPEED;
            }
            work->stateCounter -= work->forwardSpeed;
            if (work->stateCounter <= 0) {
                work->step         = MAGGOT_CATERPILLAR_ROAM_IDLE;
                work->animId       = MAGGOT_CATERPILLAR_ANIM_IDLE;
                idleRandom         = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                work->stateCounter = gMaggotCaterpillarIdleDelay[((Enemy*)actor->spawnArg2.pointer)->place->rowIndex] + ((idleRandom >> 0x10) & 0xF);
                gRandomLcgState    = idleRandom;
                return;
            }
            if (work->animFrame == MAGGOT_CATERPILLAR_ROAM_SOUND_FRAME) {
                soundKey = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | MAGGOT_CATERPILLAR_ROAM_SOUND;
                crawlPan = (s8)worldCoordGetOriginAudioPan(coord);
                sndEvtRequestScriptStart(soundKey, (s32)crawlPan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->animFrame >= MAGGOT_CATERPILLAR_ROAM_LOOP_FRAME) {
                work->animFrame = MAGGOT_CATERPILLAR_ROAM_MOTION_FRAME;
            }
            if (work->targetYaw == work->yaw) {
                scratchEnd[-1].delta.vx = (s32)(gPlayerStatus.coordMtx->t[0] - coord->coord.t[0]);
                scratch->delta.vy       = 0;
                playerOffsetZ           = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                scratch->delta.vz       = playerOffsetZ;
                playerOffsetX           = scratchEnd[-1].delta.vx;
                playerDistance          = SquareRoot0((playerOffsetX * playerOffsetX) + (playerOffsetZ * playerOffsetZ));
                if ((work->isCaterpillar == 0) && (playerDistance < MAGGOT_CATERPILLAR_ROAM_SPRAY_RANGE) && (work->burning == 0) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_DARKNESS)) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_SPRAY;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_SPRAY;
                    work->puffCount = 0;
                } else if (playerDistance < MAGGOT_CATERPILLAR_POUNCE_RANGE) {
                    work->behaviour = MAGGOT_CATERPILLAR_BEHAVIOUR_POUNCE;
                    work->step      = 0;
                    work->animId    = MAGGOT_CATERPILLAR_ANIM_POUNCE;
                }
            }
            break;
    }
}
