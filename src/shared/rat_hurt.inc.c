/* Part of the Rat library; see rat.h. */

/// Plays a stationary hit flinch, then returns to idle with an attack pending.
///
/// Requires live work/model and an `Enemy` in `Task::spawnArg2.pointer`. Entry restarts the hurt
/// clip even after another flinch; recovery begins at animation frame 24.
static void _ratHurt(Task* actor)
{
    enum {
        RAT_HURT_RECOVER_FRAME = 24,
    };

    RatWork*   work;
    TmdObject* model;
    GfxCoord*  rootCoord;
    s32        soundId;
    s32        audioPan;

    work      = actor->work;
    model     = actor->extra.tmd;
    rootCoord = model->coords;
    switch (work->step) {
        case RAT_HURT_STEP_BEGIN:
            work->animId        = RAT_ANIM_HURT;
            work->appliedAnimId = RAT_ANIM_IDLE;
            work->forwardSpeed  = 0;
            work->turnRate      = 0;
            work->step          = RAT_HURT_STEP_RECOVER;
            soundId             = ((((Enemy*)actor->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << RAT_SOUND_PLACE_INDEX_SHIFT) | RAT_SOUND_HURT;
            audioPan            = (s8)worldCoordGetOriginAudioPan(rootCoord);
            sndEvtRequestScriptStart(soundId, audioPan, (s8)worldCoordGetOriginAudioDepth(rootCoord));
            break;
        case RAT_HURT_STEP_RECOVER:
            if (work->animFrame < RAT_HURT_RECOVER_FRAME) {
                break;
            }
            work->mode            = RAT_MODE_IDLE;
            work->step            = RAT_IDLE_STEP_REST;
            work->animId          = RAT_ANIM_IDLE;
            work->timer           = 0;
            work->attackRequested = 1;
            break;
    }
}
