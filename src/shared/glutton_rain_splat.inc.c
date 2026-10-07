/* Part of the Glutton library; see glutton.h. */

/// Composes the unscaled attack coordinate at the splatted blob's translation.
///
/// Borrows live arguments for this call; no pointer is retained.
static __inline__ void _gluttonComposeSplatAttackCoord(GluttonProjectileWork* work, Task* task)
{
    work->bodyCoord.coord.t[0]   = task->extra.coordBody->coord->coord.t[0];
    work->bodyCoord.coord.t[1]   = task->extra.coordBody->coord->coord.t[1];
    work->bodyCoord.coord.t[2]   = task->extra.coordBody->coord->coord.t[2];
    work->bodyCoord.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord);
}

/// Flattens a landed rain blob and expands its attack sphere before teardown.
///
/// Requires a live coordinate body and initialized projectile work with a
/// linked attack sphere. Ticks 1..9 use the initial Q12 shape and grow the
/// radius by 64 world units per tick; ticks 10..12 use the expanded shape
/// and radius 896. Tick 12 unlinks the sphere and advances to teardown.
/// Each tick clears contacts and composes the separate attack coordinate
/// at the blob's translation. The enemy argument is unused.
static void _gluttonRainSplat(Enemy* enemy, Task* task)
{
    // Signed Q12 factors for the initial and expanded splat shapes.
    enum {
        GLUTTON_SPLAT_INITIAL_HORIZONTAL_SCALE  = 0x4000,
        GLUTTON_SPLAT_INITIAL_VERTICAL_SCALE    = 0x66,
        GLUTTON_SPLAT_EXPANDED_HORIZONTAL_SCALE = 0x4C00,
        GLUTTON_SPLAT_EXPANDED_VERTICAL_SCALE   = 0x199,
        GLUTTON_SPLAT_EXPAND_TICK               = 10,
        GLUTTON_SPLAT_END_TICK                  = 12,
        GLUTTON_SPLAT_INITIAL_RADIUS            = 256,
        GLUTTON_SPLAT_RADIUS_STEP               = 64,
        GLUTTON_SPLAT_EXPANDED_RADIUS           = 896,
    };
    GluttonProjectileWork* work;

    work = task->work;
    work->stateTicks++;
    if (work->stateTicks < GLUTTON_SPLAT_EXPAND_TICK) {
        _actorRenderRescaleYawY(task->extra.coordBody->coord, GLUTTON_SPLAT_INITIAL_HORIZONTAL_SCALE, GLUTTON_SPLAT_INITIAL_VERTICAL_SCALE);
        work->attackBody.radius = work->stateTicks * GLUTTON_SPLAT_RADIUS_STEP + GLUTTON_SPLAT_INITIAL_RADIUS;
    } else {
        _actorRenderRescaleYawY(task->extra.coordBody->coord, GLUTTON_SPLAT_EXPANDED_HORIZONTAL_SCALE, GLUTTON_SPLAT_EXPANDED_VERTICAL_SCALE);
        work->attackBody.radius = GLUTTON_SPLAT_EXPANDED_RADIUS;
    }

    worldCollisionClearContacts(work->attackContacts);
    if (work->stateTicks >= GLUTTON_SPLAT_END_TICK) {
        worldCollisionUnlinkBody(&work->attackBody);
        task->state++;
    }

    _gluttonComposeSplatAttackCoord(work, task);
}
