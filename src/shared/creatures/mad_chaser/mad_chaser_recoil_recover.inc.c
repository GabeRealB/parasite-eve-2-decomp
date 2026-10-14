/* Part of the Mad Chaser library; see mad_chaser.h. */

/// Completes light recoil using the interrupted clip's saved stance.
///
/// Requires live initialized Mad Chaser work and animation storage in combat
/// light-recoil recovery. Upright stance restarts clip 11 at twice normal rate
/// for any nonzero hit latch with a light reaction, retaining the latch/reaction.
/// Otherwise an exact-one hit latch consumes its reaction before boundary status
/// can resume walking. Low stance ignores hit requests and claims the alert at
/// a boundary, control jump or held pose. Behavior changes reset subState; slot
/// status is retained and the combat frame callback owns animation ticking.
static void _madChaserRecoilLightRecover(Task* task)
{
    MadChaserWork* work = task->work;

    if (work->stateScratch == MAD_CHASER_STANCE_UPRIGHT) {
        if (work->hitTaken != 0 && work->hitReaction == MAD_CHASER_HIT_REACTION_LIGHT) {
            work->animRate    = 2 * ANIMATION_RATE_ONE;
            work->animId      = MAD_CHASER_LIGHT_RECOIL_UPRIGHT_CLIP;
            work->animRequest = MAD_CHASER_ANIM_REQUEST_RESET;
            return;
        }
        // A consumed hit takes precedence over returning to the walk.
        if (_madChaserTakeHitReaction(task) == 0 && _madChaserAnimHasBoundaryStatusInline(task)) {
            _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_WALK);
        }
    } else if (_madChaserAnimHasBoundaryStatusInline(task)) {
        _madChaserSetAlertHold(task, 1);
        _madChaserSetBehaviorState(task, MAD_CHASER_COMBAT_STATE_ALERT);
    }
}
