/* Part of the reversing walker library; see reversing_walker.h. */

/// Spawn-placement message handler: seeds the work block's position and
/// rotation from `place`, picks the start animation from `anim` (or anim 3,
/// 2 once `field_4C4` is set) and installs it with the body of
/// `actorMotionPlayAnim19` written out inline. Returns 0.
s32 reverseWalkStartMsg(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    Actor350500Work*      work;
    Actor350500Work*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                    = (Actor350500Work*)task->work;
    w->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    w->walk.motionStep   = 0;
    w->walk.target.vx    = place->pos.vx;
    w->walk.target.vy    = place->pos.vy;
    w->walk.target.vz    = place->pos.vz;
    w->walk.targetRot.vx = place->rot.vx;
    w->walk.targetRot.vy = place->rot.vy;
    w->walk.targetRot.vz = place->rot.vz;
    preset.source.index  = 0;
    if (anim != NULL) {
        preset.animationId  = anim->animationId;
        w->model.nextAnimId = anim->nextAnimId;
    } else {
        if (w->field_4C4 != 0) {
            preset.animationId = 2;
        } else {
            preset.animationId = 3;
        }
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (Actor350500Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank   = msg->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], ext, work->rig.poses,
                             work->rig.slots);
    }
    if (msg->animationId != work->model.animId) {
        work->model.animId = msg->animationId;
        if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (i = 1; i < 0x13; i++) {
                animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
            }
        } else {
            for (i = 1; i < 0x13; i++) {
                animationResetSlot(&work->rig.anim, i, work->model.animId);
            }
        }
        for (i = 1; i < 0x13; i++) {
            animationTickSlot(&work->rig.anim, i);
        }
        work->model.ticking = 1;
    }
    return 0;
}
