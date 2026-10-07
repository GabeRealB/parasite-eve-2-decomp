/* Part of the reversing walker library; see reversing_walker.h. */

/// Advances the reversing walker's root while preserving fractional displacement.
///
/// `walk` is a writable `ActorWalkState` lvalue; `rootCoord` points to a separate
/// writable model-root `GfxCoord`. The walk lvalue is evaluated repeatedly and
/// must be a stable expression without side effects; the root pointer is
/// evaluated once. The storage is borrowed. Invoke as a standalone statement.
/// Velocity is signed 16.16 parent-coordinate units per update; the three carry
/// words enter with only fractional residues (zero on spawn).
/// Each accumulated signed integer half advances its XYZ translation, then
/// only the unsigned fractional half remains. Negative sums round down to
/// whole units and leave a nonnegative residue.
/// Invalidates the root's cached composition even when velocity is zero.
/// No pointer is retained; velocity and root rotation remain unchanged.
#define REVERSE_WALK_INTEGRATE_VELOCITY(walk, rootCoord)                \
    {                                                                   \
        GfxCoord* integratedRoot     = (rootCoord);                     \
        (walk).carry[0].word        += (walk).velocity.vx;              \
        (walk).carry[1].word        += (walk).velocity.vy;              \
        (walk).carry[2].word        += (walk).velocity.vz;              \
        integratedRoot->coord.t[0]  += (walk).carry[0].halves.integer;  \
        integratedRoot->coord.t[1]  += (walk).carry[1].halves.integer;  \
        integratedRoot->coord.t[2]  += (walk).carry[2].halves.integer;  \
        integratedRoot->composeStamp = GRAPHICS_COORD_DIRTY;            \
        (walk).carry[0].word         = (walk).carry[0].halves.fraction; \
        (walk).carry[1].word         = (walk).carry[1].halves.fraction; \
        (walk).carry[2].word         = (walk).carry[2].halves.fraction; \
    }

/// Advances the reversing walker's motion, animation and model presentation.
///
/// Requires initialized `ReverseWalkWork`, model coordinates 0..18 and motion
/// 0 (idle) or 1 (walking); a walk also requires motionStep 0..3. Indices are
/// unchecked. A ticking rig must already be bound to live slots, poses and clips.
/// Runs one motion phase, integrates signed 16.16 parent-frame velocity, then
/// ticks animation slots 1..18, leaving the root slot alone.
/// When active drawing is enabled, the cached view-space position of part 1
/// supplies a ground shadow with a 512-unit half-side. The part is recomposed
/// afterwards, and its refreshed position supplies the model lighting query.
/// Hidden models still advance motion, animation and the buffer countdown.
/// A nonnegative countdown decreases once per update; entering at zero frees
/// only the primitive buffer and leaves -1, disabling further release.
static void _reverseWalkUpdate(Task* task)
{
    enum {
        REVERSE_WALK_FIRST_DRIVEN_SLOT = 1,
        REVERSE_WALK_PRESENTATION_PART = 1,
        REVERSE_WALK_SHADOW_HALF_SIZE  = 0x200,
        REVERSE_WALK_FIRST_LIGHT       = 0,
        REVERSE_WALK_LIGHT_COUNT       = 3,
    };
    TmdObject*       model            = task->extra.tmd;
    ReverseWalkWork* work             = task->work;
    TaskFunc         motionHandlers[] = { _reverseWalkIdle, _reverseWalkRunStep };
    VECTOR3          groundPoint;
    GfxCoord*        rootCoord;
    s32              slotIndex;

    // Run the walk phase before consuming its velocity for this update.
    motionHandlers[work->walk.motion](task);
    rootCoord = task->extra.tmd->coords;
    REVERSE_WALK_INTEGRATE_VELOCITY(work->walk, rootCoord);
    if (work->model.ticking != 0) {
        for (slotIndex = REVERSE_WALK_FIRST_DRIVEN_SLOT; slotIndex < (s32)ARRAY_SIZE(work->rig.slots); slotIndex++) {
            animationTickSlot(&work->rig.anim, slotIndex);
        }
    }
    // Use the cached position for the shadow before refreshing the part and lights.
    if (!(model->flags & TMD_OBJECT_SKIP_ACTIVE_DRAW)) {
        if (worldCollisionProjectGroundPoint(MATRIX_TRANS(&task->extra.tmd->coords[REVERSE_WALK_PRESENTATION_PART].workm), &groundPoint) != 0) {
            effectDrawGroundShadow(&groundPoint, REVERSE_WALK_SHADOW_HALF_SIZE, gRoomEffectState->groundShadowShade);
        }
        task->extra.tmd->coords[REVERSE_WALK_PRESENTATION_PART].composeStamp = GRAPHICS_COORD_DIRTY;
        actorRenderComposeCoord(&task->extra.tmd->coords[REVERSE_WALK_PRESENTATION_PART]);
        worldCoordSetModelLighting(model, task->extra.tmd->coords[REVERSE_WALK_PRESENTATION_PART].workm.t, REVERSE_WALK_FIRST_LIGHT, REVERSE_WALK_LIGHT_COUNT);
    }
    // Run the pending primitive-buffer release independently of visibility.
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            tmdFreePrimitiveBuffer(model);
        }
        work->freeCountdown--;
    }
}

#undef REVERSE_WALK_INTEGRATE_VELOCITY
