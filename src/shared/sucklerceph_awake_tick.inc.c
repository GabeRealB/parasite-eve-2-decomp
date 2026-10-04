#include "main/random.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Live handler of the first enemy, dispatched on its stage `awakeStage`.
/// Stage 1 counts `idleSoundFrames` down to an idle sound, re-rolled to 0x50..0xB3
/// frames with `variant` picking the sound set, then walks: step length 0x14,
/// animation 2, a turn toward the player and a step of the root, with the
/// animation's frame count restarting at 0x1D. Stage 2 counts `swellFrames` up
/// and grows the scale factor by 0xC8 a frame; on the fifth frame the enemy is
/// killed through `sucklercephKill`, with a five-frame countdown, the death
/// phase reset, the task put into the stage's state and the HP cleared.
void sucklercephAwakeTick(Task* arg0)
{
    SucklercephWork* work;
    Enemy*           enemy;
    GfxCoord*        coord;
    s16              mode;
    s32              soundId;
    u32              rng;

    coord = arg0->extra.tmd->coords;
    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    mode  = work->awakeStage;
    switch (mode) {
        case SUCKLERCEPH_AWAKE_STAGE_CRAWL:
            work->idleSoundFrames--;
            if (work->idleSoundFrames <= 0) {
                rng                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState       = rng;
                work->idleSoundFrames = (rng >> 16) % 100 + 0x50;
                if (work->variant != 0) {
                    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x40460009;
                    SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                } else {
                    soundId = ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8) | 0x402E0001;
                    SndEvt_EnqueueType6(soundId, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            work->forwardSpeed = 0x14;
            work->animId       = SUCKLERCEPH_ANIM_CRAWL;
            sucklercephTurnToPlayer(arg0);
            sucklercephStep(arg0);
            if ((s16)work->animFrames >= 0x1D) {
                work->animFrames = 0;
            }
            break;
        case SUCKLERCEPH_AWAKE_STAGE_SWELL:
            work->swellFrames++;
            work->swellScale += 0xC8;
            if (work->swellFrames >= 5) {
                sucklercephKill(arg0, 0);
                arg0->killCountdown = 5;
                work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                arg0->state         = mode;
                enemy->hp           = 0;
            }
            break;
    }
}
