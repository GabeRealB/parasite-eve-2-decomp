#include "main/random.h"
#include "main/sound.h"

/* Part of the Sucklerceph library; see sucklerceph.h. */

/// Sound banks of the default specimen and its nonzero spawn-argument variant.
enum { SUCKLERCEPH_SOUND_BANK_DEFAULT = 0x2E,
       SUCKLERCEPH_SOUND_BANK_VARIANT = 0x46 };

/// Advances crawling or the five-tick swelling that starts the specimen's death.
///
/// Requires the live task, work, Enemy and composed root. Crawling turns toward
/// the player, steps 20 parent-frame units and wraps its animation counter at 29.
/// Idle sounds redraw an 80..179-frame delay from the shared LCG. Swelling adds
/// 200 Q12 scale units per call, then chooses burst/slump death and clears HP.
/// Contact correction and animation playback follow in the update handler.
static void _sucklercephAwakeTick(Task* task)
{
    SucklercephWork* work;
    Enemy*           enemy;
    GfxCoord*        rootCoord;
    s16              awakeStage;
    enum { SUCKLERCEPH_CRAWL_SPEED        = 20,
           SUCKLERCEPH_CRAWL_CYCLE_FRAMES = 29 };

    rootCoord  = task->extra.tmd->coords;
    work       = task->work;
    enemy      = task->spawnArg2.pointer;
    awakeStage = work->awakeStage;
    switch (awakeStage) {
        case SUCKLERCEPH_AWAKE_STAGE_CRAWL:
            _sucklercephTickIdleSound(task, rootCoord, work);
            work->forwardSpeed = SUCKLERCEPH_CRAWL_SPEED;
            work->animId       = SUCKLERCEPH_ANIM_CRAWL;
            _sucklercephTurnToPlayer(task);
            _sucklercephStep(task);
            if ((s16)work->animFrames >= SUCKLERCEPH_CRAWL_CYCLE_FRAMES) {
                work->animFrames = 0;
            }
            break;
        case SUCKLERCEPH_AWAKE_STAGE_SWELL:
            work->swellFrames++;
            work->swellScale += SUCKLERCEPH_SWELL_SCALE_STEP_Q12;
            if (work->swellFrames >= SUCKLERCEPH_SWELL_DURATION_FRAMES) {
                _sucklercephKill(task, 0);
                task->killCountdown = SUCKLERCEPH_DEATH_COUNTDOWN_FRAMES;
                work->deathPhase    = SUCKLERCEPH_DEATH_PHASE_COUNTDOWN;
                task->state         = awakeStage;
                enemy->hp           = 0;
            }
            break;
    }
}
