/* Part of the actor motion library; see actor_motion.h. */

/// State 2 of the main-body table `D_actor_141000_80131E58`: the approach
/// test. Once the X/Z distance from the root coordinate to `target` stops
/// shrinking below `lastDistance`, plays anim 0x7D3 with a preset carrying the
/// `model.nextAnimId` byte, clears `velocity` and advances the state; otherwise records
/// the distance as the new `lastDistance`.
void actorMotionArrive19(Task* arg0)
{
    ActorMotion19WalkWork* work;
    GfxCoord*              coord;
    SVECTOR                d;
    s32                    dx;
    s32                    dz;
    AnimationPlayRequest   preset;

    work  = (ActorMotion19WalkWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.lastDistance.vx && d.vz >= work->walk.lastDistance.vz) {
        preset.source.index         = 0;
        preset.animationId          = work->model.nextAnimId;
        preset.blend                = ANIMATION_BLEND_INTERPOLATE;
        preset.blendFrames          = 5;
        preset.enableWorldCollision = ANIMATION_WORLD_COLLISION_DISABLE;
#ifdef ACTOR_MOTION_PLAY19_HANDLER
        ACTOR_MOTION_PLAY19_HANDLER(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
#else
        _actorMotionPlayAnim19(arg0, ACTOR_MESSAGE_PLAY_ANIMATION, &preset, 0);
#endif
        work->walk.velocity.vx = 0;
        work->walk.velocity.vy = 0;
        work->walk.velocity.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.lastDistance.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.lastDistance.vz = d.vz < 0 ? -d.vz : d.vz;
}
