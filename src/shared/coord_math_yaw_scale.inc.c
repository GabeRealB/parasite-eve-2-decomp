/* Part of the coord math library; see coord_math.h. */

/// Replaces a coordinate's rotation with its current yaw at a uniform scale.
///
/// `coord` must be live, writable and word-aligned. Heading is extracted from
/// its local horizontal Z-axis terms, in 4096 units per turn. `uniformScale`
/// is signed with 12 fractional bits (`ONE` is 1.0). Pitch, roll and previous
/// scale are discarded; zero collapses the basis and negative scale reverses
/// its axes. Translation, parent and stored Euler angles stay intact, and the
/// composition cache is marked dirty.
///
/// Requires an initialized scratch stack with 0x58 free aligned bytes for one
/// `ActorScaleRotScratch` and the nested yaw-matrix workspace. Both reservations
/// are released before return; no pointer is retained.
static void _actorRenderSetYawScale(GfxCoord* coord, s16 uniformScale)
{
    ActorScaleRotScratch* yawScratch;
    s16                   yaw;

    yawScratch = SCRATCH_STACK_RESERVE_BLOCK(ActorScaleRotScratch);

    // Rebuild from the heading so the requested scale replaces the old scale.
    yaw             = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    yawScratch->yaw = yaw;
    gfxRotMatrixY(&yawScratch->rotation, yaw, GRAPHICS_ROTATION_REPLACE);
    yawScratch->scale.vz = uniformScale;
    yawScratch->scale.vy = uniformScale;
    yawScratch->scale.vx = uniformScale;
    ScaleMatrix(&yawScratch->rotation, &yawScratch->scale);

    // Install only rotation coefficients, preserving the coordinate's translation.
    _actorRenderCopyRotation(coord, &yawScratch->rotation);
    SCRATCH_STACK_RELEASE_BLOCK(ActorScaleRotScratch);
    coord->composeStamp = GRAPHICS_COORD_DIRTY;
}
