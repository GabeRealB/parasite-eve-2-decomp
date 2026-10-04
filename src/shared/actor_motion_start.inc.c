/* Part of the actor motion library; see actor_motion.h. */

/// The 0x7DD entry of `D_actor_135600_8013B0F4`: starts the motion sequence,
/// storing the placement position as `target` and its rotation in
/// `walk.targetRot`, then applies a start preset -- `anim`'s
/// `animationId` and `nextAnimId`, or animation 0xD and next id 1 when `anim`
/// is absent -- with the body of
/// `func_actor_135600_801330A8` written out inline. Returns 0.
s32 actorMotionStartWalk(Task* task, s32 arg1, ActorTransform* place, ActorMotionWalkAnim* anim)
{
    ActorMotionWork*      work;
    ActorMotionWork*      w;
    AnimationPlayRequest  preset;
    AnimationPlayRequest* msg;
    s32                   i;
    TmdObject*            ext;

    w                    = (ActorMotionWork*)task->work;
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
        preset.animationId  = 0xD;
        w->model.nextAnimId = 1;
    }
    preset.blend                = ANIMATION_BLEND_INTERPOLATE;
    preset.blendFrames          = 5;
    preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    msg  = &preset;
    work = (ActorMotionWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->source.index != work->model.bank) {
        work->model.bank = msg->source.index;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks[work->model.bank], ext, work->rig.poses, work->rig.slots);
    }
    work->model.animId = msg->animationId;
    if (msg->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            animationSeekSlotWithBlend(&work->rig.anim, i, work->model.animId, 0, msg->blendFrames);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            animationResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        animationTickSlot(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}
