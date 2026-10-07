/* Part of the Glutton library; see glutton.h. */

/// Installs the selected limb pitches, walking each by its per-tick step.
///
/// Function-local binding: work must be a side-effect-free GluttonWork pointer;
/// partIndex is a simple writable s16 loop counter. Both arguments are used
/// repeatedly.
/// Invoke as a standalone statement; the binding is undefined after this function.
#define GLUTTON_APPLY_LIMB_PITCH(work, partIndex)                                                                                                      \
    {                                                                                                                                                  \
        for ((partIndex) = 1; (partIndex) < GLUTTON_LIMB_PARTS; (partIndex)++) {                                                                       \
            if (abs((work)->limbPitch[(partIndex)] - (work)->limbPitchTarget[(partIndex)]) < (work)->limbPitchStep) {                                  \
                (work)->limbPitch[(partIndex)] = (work)->limbPitchTarget[(partIndex)];                                                                 \
            } else if ((work)->limbPitch[(partIndex)] < (work)->limbPitchTarget[(partIndex)]) {                                                        \
                (work)->limbPitch[(partIndex)] = (work)->limbPitch[(partIndex)] + (work)->limbPitchStep;                                               \
            } else {                                                                                                                                   \
                (work)->limbPitch[(partIndex)] = (work)->limbPitch[(partIndex)] - (work)->limbPitchStep;                                               \
            }                                                                                                                                          \
            gfxRotMatrixX(&(work)->escorts[4]->task->extra.tmd->coords[(partIndex)].coord, (work)->limbPitch[(partIndex)], GRAPHICS_ROTATION_REPLACE); \
            (work)->escorts[4]->task->extra.tmd->coords[(partIndex)].composeStamp = GRAPHICS_COORD_DIRTY;                                              \
        }                                                                                                                                              \
    }

/// Extends and curls escort 4's seven-part limb for the requested pose.
///
/// Requires a live escort-4 task, seven coordinates and the host's work. A missing
/// model buffer skips the update. Reach is floored at 460; coordinates 2..6 each
/// receive one fifth of it along local Z, narrowed to s16. Pose 2 preserves the
/// tip's target pitches, pose 4 folds the base, and poses 0, 1, 3 and 5 choose
/// reach-dependent curves. Unknown pose selectors follow pose 5. Pitches use
/// 4096 units per turn and approach their targets by the pose's step per call.
static void _gluttonPoseLimb(Task* task)
{
    enum {
        GLUTTON_LIMB_POSE_DOWNWARD      = 0,
        GLUTTON_LIMB_POSE_UPWARD        = 1,
        GLUTTON_LIMB_POSE_HOLD_TIP      = 2,
        GLUTTON_LIMB_POSE_SHALLOW_CURVE = 3,
        GLUTTON_LIMB_POSE_FOLD_BASE     = 4,
        GLUTTON_LIMB_POSE_FAST_DOWNWARD = 5,
        GLUTTON_LIMB_MIN_REACH          = 460
    };
    GluttonWork* work = task->work;
    s16          partIndex;

    if (work->escorts[4]->task->extra.tmd->buffer == NULL) {
        return;
    }

    if (gGluttonLimbReach < GLUTTON_LIMB_MIN_REACH) {
        gGluttonLimbReach = GLUTTON_LIMB_MIN_REACH;
    }

    // Distribute the reach along the five extending segments.
    for (partIndex = 0; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
        work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[0]     = work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[1] =
            work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[2] = 0;
        if ((u16)partIndex >= 2) {
            work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[2] = (s16)(gGluttonLimbReach / 5);
        }
        work->escorts[4]->task->extra.tmd->coords[partIndex].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->escorts[4]->task->extra.tmd->coords[partIndex]);
    }

    // Choose target curvature without snapping the current joint angles.
    switch (work->limbPose) {
        case GLUTTON_LIMB_POSE_FOLD_BASE:
            work->limbPitchStep      = 2;
            work->limbPitchTarget[1] = 0x180;
            work->limbPitchTarget[2] = 0x20;
            work->limbPitchTarget[3] = 0;
            work->limbPitchTarget[4] = 0;
            work->limbPitchTarget[5] = 0;
            work->limbPitchTarget[6] = 0;
            break;

        case GLUTTON_LIMB_POSE_SHALLOW_CURVE:
            work->limbPitchStep = 8;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x80;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 6; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x60;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x40;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x190;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x40;
                work->limbPitchTarget[2] = 0;
                for (partIndex = 3; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x60;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x190;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x40;
                }
            }
            break;

        case GLUTTON_LIMB_POSE_HOLD_TIP:
            work->limbPitchStep      = 0x10;
            work->limbPitchTarget[1] = 0xC0;
            work->limbPitchTarget[2] = 0x60;
            work->limbPitchTarget[3] = 0x20;
            work->limbPitchTarget[4] = work->limbPitch[4];
            work->limbPitchTarget[5] = work->limbPitch[5];
            work->limbPitchTarget[6] = work->limbPitch[6];
            break;

        case GLUTTON_LIMB_POSE_UPWARD:
            work->limbPitchStep = 0x59;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = 0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (partIndex = 3; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            }
            break;

        case GLUTTON_LIMB_POSE_DOWNWARD:
            work->limbPitchStep = 0x10;
            if (gGluttonLimbReach < 0x258) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x100;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (partIndex = 3; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            }
            break;

        case GLUTTON_LIMB_POSE_FAST_DOWNWARD:
        default:
            work->limbPitchStep = 0x54;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (partIndex = 3; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x17C;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x17C;
                }
            }
            break;
    }

    GLUTTON_APPLY_LIMB_PITCH(work, partIndex);
}

#undef GLUTTON_APPLY_LIMB_PITCH
