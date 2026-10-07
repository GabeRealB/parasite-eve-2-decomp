/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Finishes righting from the back and returns to the walking behavior.
///
/// At clip completion, turns yaw half a revolution, restarts walk clip 2 at
/// normal rate, rebuilds the pose and captures the left hand's world X/Z as
/// the new anchor. Clears `onBack` and resets `subState`. Zebra also pins body
/// part 14 to the existing anchor before testing completion; Ivory omits it.
/// The caller advances the righting animation; requires the initialized rig,
/// composed ancestors and the shared rotation/part-query scratch space.
static void _stalkerZebraIvoryRightItself(Task* task)
{
    StalkerZebraIvoryWork* work;
    StalkerZebraIvoryWork* animationWork;
    GfxCoord*              rootCoord;

    enum { STALKER_ZEBRA_IVORY_RIGHTING_BODY_PART      = 14,
           STALKER_ZEBRA_IVORY_RIGHTING_LEFT_HAND_PART = 11,
           STALKER_ZEBRA_IVORY_RIGHTING_WALK_CLIP      = 2 };

    work      = task->work;
    rootCoord = task->extra.tmd->coords;
#if STALKER_ZEBRA_IVORY_RIGHTING_PINS_PART
    _stalkerZebraIvoryPinPartXZ(task, STALKER_ZEBRA_IVORY_RIGHTING_BODY_PART, &work->anchorPos);
#endif
    if ((s16)_stalkerZebraIvoryClipDone(task) != 0) {
        work->yaw                  = (work->yaw + ACTOR_TRANSFORM_ANGLE_HALF_TURN) & ACTOR_TRANSFORM_ANGLE_MASK;
        animationWork              = task->work;
        animationWork->animStep    = ANIMATION_RATE_ONE;
        animationWork->animClip    = STALKER_ZEBRA_IVORY_RIGHTING_WALK_CLIP;
        animationWork->animRequest = STALKER_ZEBRA_IVORY_ANIM_REQUEST_RESTART;
        // Change the root pose before sampling the replacement hand anchor.
        _stalkerZebraIvoryApplyRotationInline(task);
        _stalkerZebraIvoryTickAnimInline(task);
        rootCoord->composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(rootCoord);
        _stalkerZebraIvoryReadPartWorldXZ(task, STALKER_ZEBRA_IVORY_RIGHTING_LEFT_HAND_PART, &work->anchorPos);
        work->onBack = 0;
        _stalkerZebraIvorySelectState(task, STALKER_ZEBRA_IVORY_STATE_WALK);
    }
}
