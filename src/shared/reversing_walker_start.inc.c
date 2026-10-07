/* Part of the reversing walker library; see reversing_walker.h. */

/// Starts a changed walk clip on the reversing walker's nineteen-part rig.
///
/// Borrows the task handle read only; its live model and `ReverseWalkWork`
/// remain writable. Initialize `model.bank` to `ACTOR_MODEL_STATE_NONE` and
/// `model.ticking` to zero before the first request. Both carriers provide
/// bank 0 with loaded clips 1..4. Bank and clip are compared as signed words,
/// then stored and used as signed bytes; no bounds are checked.
///
/// A bank change binds the context and invalidates the previous clip. A changed
/// clip drives slots 1..18, leaving root slot 0 alone. Nonzero `blend` with an
/// already ticking rig captures its poses and blends toward each track start;
/// otherwise the slots reset. `blendFrames` counts whole normal-rate frames
/// (0..2047 keeps the signed blend time nonnegative) and is ignored on reset.
/// The new slots tick once before `model.ticking` is set. An unchanged clip in
/// the same bank leaves playback alone, even for a reset request.
///
/// The request must be readable and separate from playback storage; no request
/// pointer is retained. Work-owned slots/poses, model coordinates and loaded
/// clip tables/data must stay live throughout playback, with coordinates and
/// tracks covering every driven index. The collision choice is ignored.
static inline void _reverseWalkApplyAnimationRequest(const Task* task, const AnimationPlayRequest* request)
{
    enum { REVERSE_WALK_FIRST_DRIVEN_SLOT = 1 };
    ReverseWalkWork* work;
    TmdObject*       model;
    s32              slotIndex;

    work  = task->work;
    model = task->extra.tmd;
    // A new bank must select its clip even when the numeric clip ID agrees.
    if (request->source.index != work->model.bank) {
        work->model.bank   = request->source.index;
        work->model.animId = ACTOR_MODEL_STATE_NONE;
        animationInitContext(&work->rig.anim, gActorMotionAnimBanks19[work->model.bank], model, work->rig.poses, work->rig.slots);
    }
    if (request->animationId != work->model.animId) {
        work->model.animId = request->animationId;
        if (request->blend != ANIMATION_BLEND_RESET && work->model.ticking != 0) {
            for (slotIndex = REVERSE_WALK_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationSeekSlotWithBlend(&work->rig.anim, slotIndex, work->model.animId, 0, request->blendFrames);
            }
        } else {
            for (slotIndex = REVERSE_WALK_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
                animationResetSlot(&work->rig.anim, slotIndex, work->model.animId);
            }
        }
        // Seed the pose before the normal frame update resumes ticking.
        for (slotIndex = REVERSE_WALK_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
        work->model.ticking = true;
    }
}

/// Starts a scripted walk to a borrowed destination and final yaw.
///
/// Handles `ACTOR_MESSAGE_WALK_TO` on a live TMD task with `ReverseWalkWork`;
/// the model bank must initially be `ACTOR_MODEL_STATE_NONE`. Destination XYZ
/// uses the root's parent-coordinate units; Euler angles use 4096 per turn.
/// Both payloads are read only and may expire after dispatch; neither may
/// overlap the work. Optional clip IDs select loaded entries 1..4 in bank 0;
/// the start ID narrows to s8 and the queued arrival ID narrows from u8 to s8.
/// NULL selects clip 2 forward or 3 backward, then clip 1 on arrival.
/// Starts a changed clip on slots 1..18 with a five-frame blend if already
/// ticking, or a reset.
/// Restarts the walk at step 0 even when the clip is unchanged. Existing
/// velocity/carry remain until subsequent steps replace them. Ignores the
/// message ID and returns 0.
static s32 _reverseWalkStartWalkMsg(Task* task, s32 messageId, const ActorTransform* destination, const ActorMotionWalkAnim* animations)
{
    enum { REVERSE_WALK_START_BLEND_FRAMES = 5 };
    ReverseWalkWork*     work;
    AnimationPlayRequest request;

    // Copy the target; movement begins on the following walk steps.
    work                    = task->work;
    work->walk.motion       = ACTOR_WALK_MOTION_WALKING;
    work->walk.motionStep   = REVERSE_WALK_STEP_FACE_TARGET;
    work->walk.target.vx    = destination->pos.vx;
    work->walk.target.vy    = destination->pos.vy;
    work->walk.target.vz    = destination->pos.vz;
    work->walk.targetRot.vx = destination->rot.vx;
    work->walk.targetRot.vy = destination->rot.vy;
    work->walk.targetRot.vz = destination->rot.vz;
    request.source.index    = REVERSE_WALK_ANIMATION_BANK;
    if (animations != NULL) {
        request.animationId    = animations->animationId;
        work->model.nextAnimId = animations->nextAnimId;
    } else {
        if (work->walksForward != 0) {
            request.animationId = REVERSE_WALK_ANIMATION_FORWARD;
        } else {
            request.animationId = REVERSE_WALK_ANIMATION_BACKWARD;
        }
        work->model.nextAnimId = REVERSE_WALK_ANIMATION_IDLE;
    }
    request.blend                = ANIMATION_BLEND_INTERPOLATE;
    request.blendFrames          = REVERSE_WALK_START_BLEND_FRAMES;
    request.enableWorldCollision = ANIMATION_WORLD_COLLISION_ENABLE;

    _reverseWalkApplyAnimationRequest(task, &request);
    return 0;
}
