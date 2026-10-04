/* Part of the footstep walk library; see footstep_walk.h. */

/// Plays a step sound whenever animation slot 1 rolls onto a new record whose
/// flags nibble is 0x10 or 0x20 - the two feet - panned and attenuated from
/// the second coordinate of the task's model. The record is latched in
/// `stepRecord` so each one fires once.
void footstepWalkPlaySteps(Task* task)
{
    FootstepWalkWork*      work;
    GfxCoord*              obj;
    const AnimationRecord* rec;
    s32                    cueBits;
    s32                    id;
    s32                    pan;

    work = task->work;
    obj  = task->extra.tmd->coords + 1;
    rec  = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
    if (rec == NULL || rec == work->stepRecord) {
        return;
    }
    work->stepRecord = rec;
    cueBits          = rec->flags & ANIMATION_RECORD_CUE_MASK;
    if (cueBits != ANIMATION_RECORD_CUE_1 && cueBits != ANIMATION_RECORD_CUE_2) {
        return;
    }
    id = 0x1000000F;
    if (cueBits == ANIMATION_RECORD_CUE_1) {
        id = 0x10000010;
    }
    id += 0x64;
    pan = (s8)worldCoordGetOriginAudioPan(obj);
    sndEvtRequestScriptStart(id, pan, (s8)worldCoordGetOriginAudioDepth(obj));
}
