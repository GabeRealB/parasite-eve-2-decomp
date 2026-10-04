/* Part of the Glutton library; see glutton.h. */

/// Pose the fifth escort -- the seven-part model whose coordinate array hangs
/// off `escorts[4]` -- for the pose `limbPose` names. Every part is
/// reset to the same spot (the whole body dropped by a fifth of the fight's
/// progress counter, which is floored at 0x1CC here), then the pose picks a
/// target pitch per part in `limbPitchTarget` and each `limbPitch` walks toward its
/// target by at most `limbPitchStep`, which is what drives the part rotations.
/// Pose 2 targets the angles parts 4 to 6 are already at, so it holds what it
/// was handed there; anything past the six it knows poses like the 0x54-step pose.
void gluttonPoseLimb(Task* task)
{
    GluttonWork* work = task->work;
    s16          i;

    if (work->escorts[4]->task->extra.tmd->buffer == NULL) {
        return;
    }

    if (gGluttonLimbReach < 0x1CC) {
        gGluttonLimbReach = 0x1CC;
    }

    for (i = 0; i < GLUTTON_LIMB_PARTS; i++) {
        work->escorts[4]->task->extra.tmd->coords[i].coord.t[0]     = work->escorts[4]->task->extra.tmd->coords[i].coord.t[1] =
            work->escorts[4]->task->extra.tmd->coords[i].coord.t[2] = 0;
        if ((u16)i >= 2) {
            work->escorts[4]->task->extra.tmd->coords[i].coord.t[2] = (s16)(gGluttonLimbReach / 5);
        }
        work->escorts[4]->task->extra.tmd->coords[i].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&work->escorts[4]->task->extra.tmd->coords[i]);
    }

    switch (work->limbPose) {
        case 4:
            work->limbPitchStep      = 2;
            work->limbPitchTarget[1] = 0x180;
            work->limbPitchTarget[2] = 0x20;
            work->limbPitchTarget[3] = 0;
            work->limbPitchTarget[4] = 0;
            work->limbPitchTarget[5] = 0;
            work->limbPitchTarget[6] = 0;
            break;

        case 3:
            work->limbPitchStep = 8;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                work->limbPitchTarget[6] = -0x80;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 6; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x60;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 4; i < 5; i++) {
                    work->limbPitchTarget[i] = -0x40;
                }
                for (i = 5; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x190;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x40;
                work->limbPitchTarget[2] = 0;
                for (i = 3; i < 4; i++) {
                    work->limbPitchTarget[i] = -0x60;
                }
                for (i = 4; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x190;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x40;
                }
            }
            break;

        case 2:
            work->limbPitchStep      = 0x10;
            work->limbPitchTarget[1] = 0xC0;
            work->limbPitchTarget[2] = 0x60;
            work->limbPitchTarget[3] = 0x20;
            work->limbPitchTarget[4] = work->limbPitch[4];
            work->limbPitchTarget[5] = work->limbPitch[5];
            work->limbPitchTarget[6] = work->limbPitch[6];
            break;

        case 1:
            work->limbPitchStep = 0x59;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                work->limbPitchTarget[6] = 0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 5; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 4; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (i = 3; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x200;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0x200;
                }
            }
            break;

        case 0:
            work->limbPitchStep = 0x10;
            if (gGluttonLimbReach < 0x258) {
                work->limbPitchTarget[1] = -0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0;
                }
            } else if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x100;
                for (i = 2; i < 6; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 5; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 4; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (i = 3; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            }
            break;

        case 5:
        default:
            work->limbPitchStep = 0x54;
            if (gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = 0;
                }
            } else if (gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 6; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 5; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 5; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            } else if (gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < 4; i++) {
                    work->limbPitchTarget[i] = 0;
                }
                for (i = 4; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x200;
                }
            } else if (gGluttonLimbReach < 0x1900) {
                work->limbPitchTarget[1] = 0x80;
                work->limbPitchTarget[2] = 0;
                for (i = 3; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x17C;
                }
            } else {
                work->limbPitchTarget[1] = 0x80;
                for (i = 2; i < GLUTTON_LIMB_PARTS; i++) {
                    work->limbPitchTarget[i] = -0x17C;
                }
            }
            break;
    }

    for (i = 1; i < GLUTTON_LIMB_PARTS; i++) {
        if (abs(work->limbPitch[i] - work->limbPitchTarget[i]) < work->limbPitchStep) {
            work->limbPitch[i] = work->limbPitchTarget[i];
        } else if (work->limbPitch[i] < work->limbPitchTarget[i]) {
            work->limbPitch[i] = work->limbPitch[i] + work->limbPitchStep;
        } else {
            work->limbPitch[i] = work->limbPitch[i] - work->limbPitchStep;
        }
        gfxRotMatrixX(&work->escorts[4]->task->extra.tmd->coords[i].coord, work->limbPitch[i], GRAPHICS_ROTATION_REPLACE);
        work->escorts[4]->task->extra.tmd->coords[i].composeStamp = GRAPHICS_COORD_DIRTY;
    }
}
