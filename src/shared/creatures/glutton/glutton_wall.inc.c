/* Part of the Glutton library; see glutton.h. */

/// Rebuilds one live collision-grid quad across the front of the host.
///
/// `distance` and `drop` use world units along normalized host axes. The wall
/// reaches 5000 units either way along x; vertices 0 and 1 lie `drop` units
/// above 2 and 3. Its face normal is the host's forward axis at unit Q12 length.
/// The active grid must provide face and normal `faceIndex` and vertices
/// `4 * faceIndex` through `4 * faceIndex + 3`; callers use face 6. Surface class
/// is 3 in the dumping-hole area, 2 elsewhere. Requires the host's live TMD
/// coordinate and a writable grid. No pointer is retained.
static void _gluttonBuildWall(Task* task, s16 distance, s16 drop, s16 faceIndex)
{
    enum { GLUTTON_WALL_HALF_WIDTH           = 5000,
           GLUTTON_WALL_DUMPING_HOLE_SURFACE = 3,
           GLUTTON_WALL_DEFAULT_SURFACE      = 2 };
    SVECTOR                 halfWidthOffset;
    WorldCollisionGridFace  face;
    SVECTOR*                wallNormal;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    SVECTOR*                halfWidthPointer;

    wallNormal = &gWorldCollisionActiveGrid->normals[faceIndex];
    vertices   = gWorldCollisionActiveGrid->vertices;
    faces      = gWorldCollisionActiveGrid->faces;

    face.vertexIndices[0] = faceIndex * 4;
    face.vertexIndices[1] = faceIndex * 4 + 1;
    face.vertexIndices[2] = faceIndex * 4 + 2;
    face.vertexIndices[3] = faceIndex * 4 + 3;
    face.normalIndex      = faceIndex;
    face.surfaceClass     = GLUTTON_WALL_DUMPING_HOLE_SURFACE;

    // Scale the facing axes into the wall's offset and half width.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, wallNormal);
    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &halfWidthOffset);
    halfWidthPointer = &halfWidthOffset;
    VectorNormalSS(halfWidthPointer, halfWidthPointer);
    VectorNormalSS(wallNormal, wallNormal);
    gte_lddp(distance);
    gte_ldsv(wallNormal);
    gte_gpf12();
    gte_stsv(wallNormal);
    gte_lddp(GLUTTON_WALL_HALF_WIDTH);
    gte_ldsv(halfWidthPointer);
    gte_gpf12();
    gte_stsv(halfWidthPointer);

    vertices[faceIndex * 4].vx = vertices[faceIndex * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + halfWidthOffset.vx + wallNormal->vx;
    vertices[faceIndex * 4].vy = vertices[faceIndex * 4 + 2].vy = halfWidthOffset.vy + wallNormal->vy;
    vertices[faceIndex * 4].vz                                  = vertices[faceIndex * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + halfWidthOffset.vz + wallNormal->vz;

    vertices[faceIndex * 4 + 1].vx = vertices[faceIndex * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] - halfWidthOffset.vx + wallNormal->vx;
    vertices[faceIndex * 4 + 1].vy = vertices[faceIndex * 4 + 3].vy = -halfWidthOffset.vy + wallNormal->vy;
    vertices[faceIndex * 4 + 1].vz                                  = vertices[faceIndex * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] - halfWidthOffset.vz + wallNormal->vz;

    vertices[faceIndex * 4].vy     -= drop;
    vertices[faceIndex * 4 + 1].vy -= drop;

    // Restore the forward axis as a unit Q12 face normal.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, wallNormal);
    VectorNormalSS(wallNormal, wallNormal);
    gte_lddp(ONE);
    gte_ldsv(wallNormal);
    gte_gpf12();
    gte_stsv(wallNormal);

    if (gGameSession->location.area == GAME_AREA_SHELTER_B3_DUMPING_HOLE) {
        face.surfaceClass = GLUTTON_WALL_DUMPING_HOLE_SURFACE;
    } else {
        face.surfaceClass = GLUTTON_WALL_DEFAULT_SURFACE;
    }

    faces[faceIndex] = face;
}

/// Steps escort 4's non-root limb pitches toward their targets and installs them.
///
/// `work` is a stable writable GluttonWork pointer with a live escort-4 model
/// containing GLUTTON_LIMB_PARTS coordinates. `partIndex` is a distinct writable
/// s16 local; both arguments are evaluated repeatedly. Drives parts 1..6 and
/// leaves the counter at 7. Positive limbPitchStep and both pitch arrays use
/// 4096 units per turn; differences promote to s32 and stored steps narrow to s16.
/// Replaces each driven part's rotation at unit scale, retains its translation,
/// and invalidates composition. Requires gfxRotMatrixX's initialized scratch
/// stack. Captures no locals; expands to one statement and is undefined below.
#define GLUTTON_APPLY_LIMB_PITCH(work, partIndex)                                                                                                                              \
    (void)({                                                                                                                                                                   \
        enum { GLUTTON_LIMB_ESCORT_INDEX = 4 };                                                                                                                                \
        for ((partIndex) = 1; (partIndex) < GLUTTON_LIMB_PARTS; (partIndex)++) {                                                                                               \
            if (abs((work)->limbPitch[(partIndex)] - (work)->limbPitchTarget[(partIndex)]) < (work)->limbPitchStep) {                                                          \
                (work)->limbPitch[(partIndex)] = (work)->limbPitchTarget[(partIndex)];                                                                                         \
            } else if ((work)->limbPitch[(partIndex)] < (work)->limbPitchTarget[(partIndex)]) {                                                                                \
                (work)->limbPitch[(partIndex)] = (work)->limbPitch[(partIndex)] + (work)->limbPitchStep;                                                                       \
            } else {                                                                                                                                                           \
                (work)->limbPitch[(partIndex)] = (work)->limbPitch[(partIndex)] - (work)->limbPitchStep;                                                                       \
            }                                                                                                                                                                  \
            gfxRotMatrixX(&(work)->escorts[GLUTTON_LIMB_ESCORT_INDEX]->task->extra.tmd->coords[(partIndex)].coord, (work)->limbPitch[(partIndex)], GRAPHICS_ROTATION_REPLACE); \
            (work)->escorts[GLUTTON_LIMB_ESCORT_INDEX]->task->extra.tmd->coords[(partIndex)].composeStamp = GRAPHICS_COORD_DIRTY;                                              \
        }                                                                                                                                                                      \
    })

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

    if (_gGluttonLimbReach < GLUTTON_LIMB_MIN_REACH) {
        _gGluttonLimbReach = GLUTTON_LIMB_MIN_REACH;
    }

    // Distribute the reach along the five extending segments.
    for (partIndex = 0; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
        work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[0]     = work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[1] =
            work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[2] = 0;
        if ((u16)partIndex >= 2) {
            work->escorts[4]->task->extra.tmd->coords[partIndex].coord.t[2] = (s16)(_gGluttonLimbReach / 5);
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
            if (_gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (_gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x80;
            } else if (_gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 6; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x60;
                }
            } else if (_gGluttonLimbReach < 0xC80) {
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
            } else if (_gGluttonLimbReach < 0x1900) {
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
            if (_gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (_gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = 0x200;
            } else if (_gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            } else if (_gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0x200;
                }
            } else if (_gGluttonLimbReach < 0x1900) {
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
            if (_gGluttonLimbReach < 0x258) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (_gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (_gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x100;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (_gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (_gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (_gGluttonLimbReach < 0x1900) {
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
            if (_gGluttonLimbReach < 0x400) {
                work->limbPitchTarget[1] = -0x80;
                for (partIndex = 2; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
            } else if (_gGluttonLimbReach < 0x604) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 6; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                work->limbPitchTarget[6] = -0x200;
            } else if (_gGluttonLimbReach < 0x708) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 5; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 5; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (_gGluttonLimbReach < 0xC80) {
                work->limbPitchTarget[1] = 0x80;
                for (partIndex = 2; partIndex < 4; partIndex++) {
                    work->limbPitchTarget[partIndex] = 0;
                }
                for (partIndex = 4; partIndex < GLUTTON_LIMB_PARTS; partIndex++) {
                    work->limbPitchTarget[partIndex] = -0x200;
                }
            } else if (_gGluttonLimbReach < 0x1900) {
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

/// Moves the neck toward a target yaw and refreshes its dependent coordinates.
///
/// `yawTarget` uses 4096 angle units per turn, clamped to +/-0x200. The stored
/// yaw closes by at most 0x71 per call and turns host part 3 in world space.
/// Requires live host parts 3 and 4 and escort 4's root. Their compositions are
/// refreshed after the turn so the pitched neck and attached limb follow it.
static void _gluttonTurnNeck(Task* task, s16 yawTarget)
{
    enum { GLUTTON_NECK_YAW_LIMIT = 0x200,
           GLUTTON_NECK_YAW_STEP  = 0x71 };
    GluttonWork* work = task->work;
    s16          clampedYaw;

    clampedYaw = yawTarget;
    if (yawTarget > GLUTTON_NECK_YAW_LIMIT) {
        clampedYaw = GLUTTON_NECK_YAW_LIMIT;
    }
    if (yawTarget < -GLUTTON_NECK_YAW_LIMIT) {
        clampedYaw = -GLUTTON_NECK_YAW_LIMIT;
    }

    if (work->neckYaw < clampedYaw) {
        if (clampedYaw - work->neckYaw >= GLUTTON_NECK_YAW_STEP + 1) {
            work->neckYaw = work->neckYaw + GLUTTON_NECK_YAW_STEP;
        } else {
            work->neckYaw = clampedYaw;
        }
    } else if (clampedYaw < work->neckYaw) {
        if (abs(work->neckYaw - clampedYaw) >= GLUTTON_NECK_YAW_STEP + 1) {
            work->neckYaw = work->neckYaw - GLUTTON_NECK_YAW_STEP;
        } else {
            work->neckYaw = clampedYaw;
        }
    }

    // Compose the animated joint before applying yaw in world space.
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    _actorRenderYawJointInWorld(&task->extra.tmd->coords[3], work->neckYaw);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[3]);
    work->escorts[4]->task->extra.tmd->coords[0].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->escorts[4]->task->extra.tmd->coords[0]);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&task->extra.tmd->coords[4]);
}

/// Moves the neck toward a target pitch and applies it to the two neck parts.
///
/// `pitchTarget` uses 4096 angle units per turn, clamped to 0..0x500. The stored
/// pitch closes by at most 0x10 per call. Host parts 3 and 4 must be live; their
/// local X rotations become half the pitch and its negation respectively,
/// relative to the current animation pose. Both composition caches are dirtied.
static void _gluttonPitchNeck(Task* task, s16 pitchTarget)
{
    enum { GLUTTON_NECK_PITCH_LIMIT = 0x500,
           GLUTTON_NECK_PITCH_STEP  = 0x10 };
    GluttonWork* work = task->work;
    s16          clampedPitch;
    s16          tipPitch;
    s16          basePitch;

    clampedPitch = pitchTarget;
    if (pitchTarget > GLUTTON_NECK_PITCH_LIMIT) {
        clampedPitch = GLUTTON_NECK_PITCH_LIMIT;
    }
    if (pitchTarget < 0) {
        clampedPitch = 0;
    }

    if (work->neckPitch < clampedPitch) {
        if (clampedPitch - work->neckPitch >= GLUTTON_NECK_PITCH_STEP + 1) {
            work->neckPitch = work->neckPitch + GLUTTON_NECK_PITCH_STEP;
        } else {
            work->neckPitch = clampedPitch;
        }
    } else if (clampedPitch < work->neckPitch) {
        if (abs(work->neckPitch - clampedPitch) >= GLUTTON_NECK_PITCH_STEP + 1) {
            work->neckPitch = work->neckPitch - GLUTTON_NECK_PITCH_STEP;
        } else {
            work->neckPitch = clampedPitch;
        }
    }

    // Replace the animated X pitches without disturbing the other axes.
    tipPitch  = -ratan2(task->extra.tmd->coords[4].coord.m[1][2],
                        task->extra.tmd->coords[4].coord.m[2][2]);
    basePitch = -ratan2(task->extra.tmd->coords[3].coord.m[1][2],
                        task->extra.tmd->coords[3].coord.m[2][2]);

    gfxRotMatrixX(&task->extra.tmd->coords[3].coord, work->neckPitch / 2 - basePitch, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[3].composeStamp = GRAPHICS_COORD_DIRTY;
    gfxRotMatrixX(&task->extra.tmd->coords[4].coord, -work->neckPitch - tipPitch, GRAPHICS_ROTATION_COMPOSE);
    task->extra.tmd->coords[4].composeStamp = GRAPHICS_COORD_DIRTY;
}

/// Starts the secondary clip used to blend the host and its first two escorts.
///
/// Requires initialized driving and secondary rigs in the host's work and a valid
/// `blendAnimId` in all three animation tables. Sets the blend rate to three
/// normal frames per tick and the driving-pose weight to one half. Host slots
/// 1..7 and escort slots 1..3 have their driving rate set and secondary clip
/// restarted; escort slot 0 is deliberately left alone.
static void _gluttonSeedBlend(Task* task)
{
    GluttonWork* work;
    s32          slotIndex;

    work              = task->work;
    work->blendRate   = 3 * ANIMATION_RATE_ONE;
    work->blendWeight = ONE / 2;
    slotIndex         = 1;
    do {
        work->hostRig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->hostBlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->hostRig.slots));
    slotIndex = 1;
    do {
        work->escort0Rig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->escort0BlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->escort0Rig.slots));
    slotIndex = 1;
    do {
        work->escort1Rig.slots[slotIndex].rate = work->blendRate;
        animationResetSlot(&work->escort1BlendRig.anim, slotIndex, work->blendAnimId);
        slotIndex++;
    } while (slotIndex < ARRAY_SIZE(work->escort1Rig.slots));
}

/// Blends the three driving rigs into the requested clip when it changes.
///
/// Requires initialized host and escort-0/1 rigs and both clip ids in the
/// transition table's domain (0..44); the requested clip must also exist in all
/// three rigs' animation tables. Host slots 1..7 and escort slots 0..3 seek the
/// requested clip's start at `animRate`, using the table's blend duration in
/// frames. The applied id is latched after all slots are reseeded. An unchanged
/// id leaves their playback and rates alone.
static void _gluttonSwitchAnim(Task* task)
{
    GluttonWork* work = task->work;
    s32          slotIndex;

    if (work->appliedAnimId != work->animId) {
        for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->hostRig.slots); slotIndex++) {
            work->hostRig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->hostRig.anim, slotIndex, work->animId, 0,
                                       _gGluttonAnimBlendFrames[work->appliedAnimId][work->animId]);
        }
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort0Rig.slots); slotIndex++) {
            work->escort0Rig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort0Rig.anim, slotIndex, work->animId, 0,
                                       _gGluttonAnimBlendFrames[work->appliedAnimId][work->animId]);
        }
        for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort1Rig.slots); slotIndex++) {
            work->escort1Rig.slots[slotIndex].rate = work->animRate;
            animationSeekSlotWithBlend(&work->escort1Rig.anim, slotIndex, work->animId, 0,
                                       _gGluttonAnimBlendFrames[work->appliedAnimId][work->animId]);
        }
        work->appliedAnimId = work->animId;
    }
}

/// Ticks both pose sources and mixes their rotations onto the three driving rigs.
///
/// Requires initialized host and escort-0/1 rigs with loaded clip data. Host slots
/// 1..7 and escort slots 0..3 capture driving and secondary poses at rates
/// `animRate - 3` and `blendRate`, in sixteenths of a frame per tick. Translation
/// comes from the driving pose; rotation weights are `blendWeight` for the driving
/// pose and `ONE - blendWeight` for the secondary pose. Weights normally lie in
/// 0..ONE. Pose capture, blending and nested conversions require initialized
/// scratch and overwrite GTE state; no pose pointer is retained.
static void _gluttonTickBlended(Task* task)
{
    enum { GLUTTON_HOST_BLENDED_SLOT_END = 11 };
    AnimationPose drivingPose;
    AnimationPose secondaryPose;
    GluttonWork*  work            = task->work;
    s32           drivingWeight   = work->blendWeight;
    s32           secondaryWeight = ONE - drivingWeight;
    s16           slotIndex;

    for (slotIndex = 1; slotIndex < ARRAY_SIZE(work->hostRig.slots); slotIndex++) {
        // All host slots use the blended side of this ten-slot split.
        if (slotIndex < GLUTTON_HOST_BLENDED_SLOT_END) {
            work->hostBlendRig.slots[slotIndex].rate = work->blendRate;
            work->hostRig.slots[slotIndex].rate      = work->animRate - 3;
            animationTickSlotPose(&work->hostRig.anim, slotIndex, &drivingPose, NULL);
            animationTickSlotPose(&work->hostBlendRig.anim, slotIndex, &secondaryPose, NULL);
            animationApplyPoseWithBlendedRotation(&work->hostRig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
        } else {
            work->hostRig.slots[slotIndex].rate = work->animRate - 3;
            animationTickSlot(&work->hostRig.anim, slotIndex);
        }
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort0Rig.slots); slotIndex++) {
        work->escort0BlendRig.slots[slotIndex].rate = work->blendRate;
        work->escort0Rig.slots[slotIndex].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort0Rig.anim, slotIndex, &drivingPose, NULL);
        animationTickSlotPose(&work->escort0BlendRig.anim, slotIndex, &secondaryPose, NULL);
        animationApplyPoseWithBlendedRotation(&work->escort0Rig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
    }

    for (slotIndex = 0; slotIndex < ARRAY_SIZE(work->escort1Rig.slots); slotIndex++) {
        work->escort1BlendRig.slots[slotIndex].rate = work->blendRate;
        work->escort1Rig.slots[slotIndex].rate      = work->animRate - 3;
        animationTickSlotPose(&work->escort1Rig.anim, slotIndex, &drivingPose, NULL);
        animationTickSlotPose(&work->escort1BlendRig.anim, slotIndex, &secondaryPose, NULL);
        animationApplyPoseWithBlendedRotation(&work->escort1Rig.anim, slotIndex, &drivingPose, &secondaryPose, drivingWeight, secondaryWeight);
    }
}

/// Restarts the host's driving clip and the matching clips on escorts 0 and 1.
///
/// Requires live host work and initialized driving rigs. Applies `animId` and
/// `animRate` (signed sixteenths of a frame per tick) to host slots 1..7 and
/// escort slots 0..3, narrowing each rate to s8, then records `appliedAnimId`.
/// Host slot 0 is untouched;
/// blend rigs, clip cues and elapsed ticks are managed by the caller.
static inline void _gluttonRestartDrivingRigs(Task* task)
{
    GluttonWork* rigWork = task->work;
    s32          slotIndex;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(rigWork->hostRig.slots); slotIndex++) {
        rigWork->hostRig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->hostRig.anim, slotIndex, rigWork->animId);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort0Rig.slots); slotIndex++) {
        rigWork->escort0Rig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->escort0Rig.anim, slotIndex, rigWork->animId);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort1Rig.slots); slotIndex++) {
        rigWork->escort1Rig.slots[slotIndex].rate = rigWork->animRate;
        animationResetSlot(&rigWork->escort1Rig.anim, slotIndex, rigWork->animId);
    }
    rigWork->appliedAnimId = rigWork->animId;
}

/// Advances the host's driving clip and the clips on escorts 0 and 1 once.
///
/// Requires live host work and initialized driving rigs. Refreshes `animRate`
/// (signed sixteenths of a frame per tick) before ticking host slots 1..7 and
/// escort slots 0..3, narrowing each rate to s8. Host slot 0 and all blend
/// rigs remain untouched.
static inline void _gluttonAdvanceDrivingRigs(Task* task)
{
    GluttonWork* rigWork = task->work;
    s32          slotIndex;
    for (slotIndex = 1; slotIndex < ARRAY_SIZE(rigWork->hostRig.slots); slotIndex++) {
        rigWork->hostRig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->hostRig.anim, slotIndex);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort0Rig.slots); slotIndex++) {
        rigWork->escort0Rig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->escort0Rig.anim, slotIndex);
    }
    for (slotIndex = 0; slotIndex < ARRAY_SIZE(rigWork->escort1Rig.slots); slotIndex++) {
        rigWork->escort1Rig.slots[slotIndex].rate = rigWork->animRate;
        animationTickSlot(&rigWork->escort1Rig.anim, slotIndex);
    }
}

/// Advances the Glutton's three driving rigs and applies enabled joint overrides.
///
/// Requires the host's live work, TMD body, escorts 0, 1 and 4, and initialized
/// rigs with loaded animation resources. BLEND seeks a changed clip, RESTART
/// resets it unconditionally; both clear the clip cue-cache block and elapsed
/// ticks. A requested secondary clip is seeded before playback. Plain ticks
/// advance host slots 1..7 and escort slots 0..3 at `animRate`; blended ticks use
/// both pose sources and end blending when secondary host slot 1 reaches a
/// boundary. Pitch, yaw and limb overrides follow playback in that order.
static void _gluttonTickAnim(Task* task)
{
    enum { GLUTTON_BLEND_STEP_RESTART = 2,
           GLUTTON_BLEND_STEP_PLAYING = 3 };
    GluttonWork* work = task->work;

    if (work->animStep == GLUTTON_ANIM_STEP_BLEND) {
        _gluttonSwitchAnim(task);
        work->animStep  = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    } else if (work->animStep == GLUTTON_ANIM_STEP_RESTART) {
        _gluttonRestartDrivingRigs(task);
        work->animStep  = GLUTTON_ANIM_STEP_PLAYING;
        work->animTicks = 0;
        memFillBytes(&work->clip, 0, sizeof(work->clip));
    }

    if (work->blendStep == GLUTTON_BLEND_STEP_RESTART) {
        _gluttonSeedBlend(task);
        work->blendStep = GLUTTON_BLEND_STEP_PLAYING;
    }

    work->animTicks++;

    if (work->blending == 0) {
        _gluttonAdvanceDrivingRigs(task);
    } else {
        _gluttonTickBlended(task);
        if (work->hostBlendRig.slots[1].status.fields.flags & ANIMATION_SLOT_REACHED_BOUNDARY) {
            work->blending = 0;
        }
    }

    // Apply procedural joints after the clips have written their poses.
    if (work->neckPitchEnabled != 0) {
        _gluttonPitchNeck(task, work->neckPitchTarget);
    }
    if (work->neckYawEnabled != 0) {
        _gluttonTurnNeck(task, work->neckYawTarget);
    }
    if (work->limbPoseEnabled != 0) {
        _gluttonPoseLimb(task);
    }
}

#include "main/random.h"

#include "gameplay/room_effects.h"

/* Part of the Glutton library; see glutton.h. */

/// Scratch-stack block holding the two records a Glutton hit effect is spawned from.
///
/// Both are arguments of the id-dispatched effect spawner. Reserve one
/// complete block and release it once the effect is spawned; nothing in it is
/// meant to outlive the call.
typedef struct {
    SVECTOR        offset;    // Where the effect appears: an offset from the struck body's coordinate, along that coordinate's axes
    EffectSpawnArg effectArg; // Spawner's argument record: the struck body's coordinate, with spawn-argument halves 0x500 and 3
} _GluttonHitEffectScratch;
STATIC_ASSERT_SIZEOF(_GluttonHitEffectScratch, 0x10);

/// Spawns a player attack's hit effect relative to the struck coordinate.
///
/// `attackKey` must select a valid weapon or attachment attack row; category bits
/// are ignored. Reactions 2, 4, 6 and 7 use a fixed offset; other values choose
/// one of three offsets with one LCG advance. Offsets use model units along
/// `coord`'s local axes. Requires a live coordinate and initialized scratch with
/// one `_GluttonHitEffectScratch` plus the spawner's nested capacity. The records
/// are released before return; the effects must not rely on a live offset pointer
/// into that released storage.
static void _gluttonHitEffect(GfxCoord* coord, s32 attackKey)
{
    enum { GLUTTON_HIT_EFFECT_SIZE  = 0x500,
           GLUTTON_HIT_EFFECT_COUNT = 3 };
    _GluttonHitEffectScratch* scratch = SCRATCH_STACK_RESERVE_BLOCK(_GluttonHitEffectScratch);

    scratch->effectArg.spawnArgLo = GLUTTON_HIT_EFFECT_SIZE;
    scratch->effectArg.coord      = coord;
    scratch->effectArg.spawnArgHi = GLUTTON_HIT_EFFECT_COUNT;

    switch (damageGetPlayerAttackReaction(attackKey) & 0xFFFF) {
        case DAMAGE_PLAYER_REACTION_BUILDUP:
        case 4:
        case DAMAGE_PLAYER_REACTION_EXPLOSION:
        case DAMAGE_PLAYER_REACTION_INCENDIARY:
            scratch->offset.vx = 0;
            scratch->offset.vy = -0x190;
            scratch->offset.vz = 0x258;
            effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
            break;
        case DAMAGE_PLAYER_REACTION_NONE:
        case DAMAGE_PLAYER_REACTION_STAGGER:
        case DAMAGE_PLAYER_REACTION_POISON:
        case 5:
        case 8:
        case 9:
        default:
            RANDOM_LCG_ADVANCE();
            switch ((u16)(RANDOM_LCG_DRAW() % 3U)) {
                case 0:
                    scratch->offset.vy = 0;
                    scratch->offset.vx = 0;
                    scratch->offset.vz = 0x384;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 1:
                    scratch->offset.vx = 0x258;
                    scratch->offset.vy = -0xC8;
                    scratch->offset.vz = 0x2BC;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
                case 2:
                    scratch->offset.vx = -0x12C;
                    scratch->offset.vy = -0x320;
                    scratch->offset.vz = 0x320;
                    effectSpawnHit(damageGetPlayerAttackEffectId(attackKey), coord, &scratch->offset, &scratch->effectArg);
                    break;
            }
            break;
    }

    SCRATCH_STACK_RELEASE_BLOCK(_GluttonHitEffectScratch);
}
