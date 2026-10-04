/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Returns the sound event the current clip (`animId`) has reached at its
/// frame (the second slot's cue index), once per frame: the frame is latched in `lastCueFrame`, and
/// a frame already latched, or one that carries no event, returns 0.
s32 oddStrangerAnimEvent(OddStrangerWork* work)
{
    s32 id;
    s32 prev;

    switch (work->animId) {
        case 20:
        case 21:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 7) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0010;
                }
                work->lastCueFrame = id;
            } else if (id == 0x10) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0011;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 3:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x1A) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0004;
                }
                work->lastCueFrame = id;
            } else if (id == 0x13) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0003;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 2:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == ODD_STRANGER_CLIP2_STEP_A) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0002;
                }
                work->lastCueFrame = id;
            } else if (id == ODD_STRANGER_CLIP2_STEP_B) {
                prev = work->lastCueFrame;
                if (prev != id) {
                    work->lastCueFrame = id;
                    return 0x400A0001;
                }
                work->lastCueFrame = prev;
            } else {
                work->lastCueFrame = 0;
            }
            break;
        case 9:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 4 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0006;
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 11:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 4 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0005;
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 12:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 7 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0005;
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 4:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0xA) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0004;
                }
            }
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x12) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A0002;
                }
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 5:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 9 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A000D;
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 7:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x16 && work->lastCueFrame != id) {
                work->lastCueFrame = id;
                return 0x400A0003;
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
        case 6:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 9) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A000D;
                }
            }
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x13) {
                if (work->lastCueFrame != id) {
                    work->lastCueFrame = id;
                    return 0x400A000C;
                }
            }
            prev               = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->lastCueFrame = prev;
            break;
    }
    return 0;
}
