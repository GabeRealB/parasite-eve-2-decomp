/* Part of the Odd Stranger library; see odd_stranger.h. */

/// Returns the sound event the current clip (`field_89E`) has reached at its
/// frame (the second slot's cue index), once per frame: the frame is latched in `field_8B4`, and
/// a frame already latched, or one that carries no event, returns 0.
s32 oddStrangerAnimEvent(OddStrangerWork* work)
{
    s32 id;
    s32 prev;

    switch (work->field_89E) {
        case 20:
        case 21:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 7) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0010;
                }
                work->field_8B4 = id;
            } else if (id == 0x10) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0011;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 3:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x1A) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
                work->field_8B4 = id;
            } else if (id == 0x13) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0003;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 2:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == ODD_STRANGER_CLIP2_STEP_A) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
                work->field_8B4 = id;
            } else if (id == ODD_STRANGER_CLIP2_STEP_B) {
                prev = work->field_8B4;
                if (prev != id) {
                    work->field_8B4 = id;
                    return 0x400A0001;
                }
                work->field_8B4 = prev;
            } else {
                work->field_8B4 = 0;
            }
            break;
        case 9:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0006;
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 11:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 4 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 12:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 7 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0005;
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 4:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0xA) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0004;
                }
            }
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x12) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A0002;
                }
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 5:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 9 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A000D;
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 7:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x16 && work->field_8B4 != id) {
                work->field_8B4 = id;
                return 0x400A0003;
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
        case 6:
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 9) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000D;
                }
            }
            id = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            if (id == 0x13) {
                if (work->field_8B4 != id) {
                    work->field_8B4 = id;
                    return 0x400A000C;
                }
            }
            prev            = work->rig.slots[1].currentPose.indices.recordIndex & ANIMATION_POSE_CUE_INDEX_MASK;
            work->field_8B4 = prev;
            break;
    }
    return 0;
}
