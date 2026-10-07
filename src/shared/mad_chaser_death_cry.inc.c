/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Releases the ordinary-death target and fades its positional alert cry.
///
/// Requires a live enemy, model and task-owned Mad Chaser work. Releases this
/// enemy's alert claim and unlinks its target node, retaining battle rewards,
/// collision bodies and resource storage. A blast reaction hides the model,
/// resets stateFrames and selects death behavior 7 at sub-state zero; other
/// reactions advance from behavior 0 to the reward/settle step.
static void _madChaserDeathReleaseTarget(Task* task)
{
    enum {
        MAD_CHASER_DEATH_CRY_FADE_UPDATES     = 15,
        MAD_CHASER_DEATH_BLAST_PAUSE_STATE    = 7,
        MAD_CHASER_DEATH_SOUND_INSTANCE_SHIFT = 8,
    };
    Enemy*         enemy;
    MadChaserWork* work;
    TmdObject*     model;
    MadChaserWork* stateWork;

    enemy = task->spawnArg2.pointer;
    model = task->extra.tmd;
    work  = task->work;
    sndEvtRequestScriptStop(((enemy->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << MAD_CHASER_DEATH_SOUND_INSTANCE_SHIFT) | SOUND_MAD_CHASER_ALERT_CRY, MAD_CHASER_DEATH_CRY_FADE_UPDATES);
    _madChaserSetAlertHold(task, 0);
    worldTargetUnlinkNode(&enemy->node);
    if (work->hitReaction == MAD_CHASER_HIT_REACTION_BLAST) {
        // A lethal blast skips settling and shrinking before the burst.
        work->stateFrames   = 0;
        model->flags        = model->flags | TMD_OBJECT_SKIP_ACTIVE_DRAW;
        stateWork           = task->work;
        stateWork->state    = MAD_CHASER_DEATH_BLAST_PAUSE_STATE;
        stateWork->subState = 0;
        return;
    }
    work->state = work->state + 1;
}
