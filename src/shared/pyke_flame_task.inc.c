/* Part of the Pyke flame library; see pyke_flame.h. */

#ifndef PYKE_FLAME_REDRAW_UPDATES_COORD
#define PYKE_FLAME_REDRAW_UPDATES_COORD 0
#endif

/// Advances a Pyke flame by one parent-frame displacement and refreshes its composed transform.
///
/// `flameWork->move` supplies signed 16-bit XYZ displacements in integer game
/// units for one running tick, already in `flameCoord`'s parent frame. Both
/// pointers must be non-NULL and remain live for this call; neither is retained.
/// The coordinate and its ancestors must stay live and writable in an acyclic
/// parent chain. Local rotation is preserved; `workm` is composed through the complete
/// chain for drawing and collision, clobbering the GTE working registers.
static inline void _pykeFlameAdvanceCoord(GfxCoord* flameCoord, const EffectWork* flameWork)
{
    flameCoord->coord.t[0]  += flameWork->move.vx;
    flameCoord->coord.t[1]  += flameWork->move.vy;
    flameCoord->coord.t[2]  += flameWork->move.vz;
    flameCoord->composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(flameCoord);
}

/// Updates one Pyke flame's colliding flight and spreading drift after a geometry hit.
///
/// Requires a counted effect with an owned coordinate body and an `EffectWork`
/// in `spawnArg2.pointer`, initially age/state zero and work NULL. The low u16
/// of `spawnArg1.value` supplies launch speed in game units per running tick
/// (the carriers pass 0x40..0x180) and the initial size numerator before a 0x180
/// bias. Launch follows the coordinate's rotated positive Y axis, with 0..63
/// units of speed jitter. `move` then holds signed-halfword displacements in
/// the coordinate's parent frame; `scale` sizes the billboard and `angle` uses
/// 4096 units per turn. The collision radius is fixed at half the initial size.
///
/// Owns a primary-heap `PykeFlameBody` in `work` after initialization. An enemy-body
/// contact ends flight; a geometry hit unlinks its sphere and starts drift
/// without resetting age. Expiry is checked at age 21, after drawing; the tick
/// that enters drift returns before that check. Allocation failure retries
/// initialization with age zero. Nonzero control below cancellation redraws
/// without aging; the companion carrier also refreshes the coordinate then.
/// Cancellation or expiry releases the task, coordinate body and owned work blocks.
static inline void _pykeFlameTask(Task* task)
{
    enum {
        PYKE_FLAME_STATE_INIT            = 0,
        PYKE_FLAME_STATE_FLIGHT          = 1,
        PYKE_FLAME_STATE_DRIFT           = 2,
        PYKE_FLAME_LIFETIME_TICKS        = 21,
        PYKE_FLAME_INITIAL_SIZE_BIAS     = 0x180,
        PYKE_FLAME_FLIGHT_SIZE_STEP      = 0x10,
        PYKE_FLAME_DRIFT_SIZE_STEP       = 0x40,
        PYKE_FLAME_FLIGHT_Y_ACCELERATION = 8,
        PYKE_FLAME_LAUNCH_JITTER_MASK    = 0x3F,
        PYKE_FLAME_SPIN_ANGLE_MASK       = ONE - 1,
        PYKE_FLAME_DRIFT_TRIG_SHIFT      = 8
    };

    GfxCoord       groundCoord;
    SVECTOR        flightEnd;
    SVECTOR        flightStart;
    GfxCoord*      coord;
    EffectWork*    work;
    PykeFlameBody* flame;
    s32            effectControl;
    u32            launchRandom;
    u32            spinRandom;
    u32            splashRandom;
    u32            driftRandom;

    flame         = task->work;
    work          = task->spawnArg2.pointer;
    effectControl = gRoomEffectState->effectControl;
    coord         = task->extra.coordBody->coord;
    if (effectControl >= ROOM_EFFECT_CONTROL_CANCEL_MIN) {
        if (task->state != PYKE_FLAME_STATE_INIT) {
            worldCollisionUnlinkBody(&flame->body);
        }
        effectKillTask(work, task);
        return;
    }
    if (effectControl != ROOM_EFFECT_CONTROL_RUNNING) {
#if PYKE_FLAME_REDRAW_UPDATES_COORD
        actorRenderComposeCoord(coord);
#endif
        _pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                           (work->age >> 1) + 1, work->scale, work->angle);
        return;
    }
    work->age = work->age + 1;
    switch (task->state) {
        case PYKE_FLAME_STATE_INIT:
            flame = memCalloc(sizeof(*flame), false);
            if (flame == NULL) {
                work->age = 0;
                return;
            }
            task->exitCallback = _pykeFlameRelease;
            // Rotate the launch displacement into the coordinate's parent frame.
            work->move.vx   = 0;
            launchRandom    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = launchRandom;
            work->move.vy   = (u16)task->spawnArg1.value - ((launchRandom >> 16) & PYKE_FLAME_LAUNCH_JITTER_MASK);
            work->move.vz   = 0;
            gte_SetRotMatrix(&coord->coord);
            gte_ldv0(&work->move);
            gte_rtv0();
            gte_stsv(&work->move);
            work->scale                  = (u16)task->spawnArg1.value + PYKE_FLAME_INITIAL_SIZE_BIAS;
            spinRandom                   = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            work->angle                  = (spinRandom >> 16) & PYKE_FLAME_SPIN_ANGLE_MASK;
            task->state                  = PYKE_FLAME_STATE_FLIGHT;
            task->work                   = flame;
            flame->body.coord            = coord;
            flame->body.context.contacts = flame->contacts;
            flame->body.key              = PYKE_FLAME_KEY;
            flame->body.radius           = work->scale >> 1;
            gRandomLcgState              = spinRandom;
            flame->body.flags            = WORLD_COLLISION_BODY_SPHERE;
            worldCollisionLinkBody(WORLD_COLLISION_LIST_PLAYER_ATTACKS, &flame->body);
            // The allocation already zeroed the entry; LAST terminates the table.
            flame->contacts[0].flags = WORLD_COLLISION_CONTACT_LAST;
            flame->body.flags       |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            /* fallthrough */
        case PYKE_FLAME_STATE_FLIGHT:
            // Keep composed endpoints for the geometry probe; contact tests use the sphere.
            work->scale    = work->scale + PYKE_FLAME_FLIGHT_SIZE_STEP;
            work->move.vy  = work->move.vy + PYKE_FLAME_FLIGHT_Y_ACCELERATION;
            flightStart.vx = coord->workm.t[0];
            flightStart.vy = coord->workm.t[1];
            flightStart.vz = coord->workm.t[2];
            _pykeFlameAdvanceCoord(coord, work);
            flightEnd.vx = coord->workm.t[0];
            flightEnd.vy = coord->workm.t[1];
            flightEnd.vz = coord->workm.t[2];
            _pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                               (work->age >> 1) + 1, work->scale,
                               work->angle);
            splashRandom    = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
            gRandomLcgState = splashRandom;
            if ((u16)((splashRandom >> 16) % 3) == 0 && gRoomEffectState->groundTraceEnabled != 0 &&
                worldCollisionProjectGroundCoord(coord, &groundCoord) == 1) {
                // Only projected XYZ feed the splash; the temporary's rotation stays unspecified.
                _pykeFlameDrawSplash(MATRIX_TRANS(&groundCoord.workm), (s16)((work->scale * 2) / 3));
            }
            if (worldCollisionCountContactsByKind(flame->body.context.contacts, WORLD_COLLISION_CONTACT_ENEMY_BODY) != 0) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            if (worldCollisionProbeGridSegment(&flightEnd, &flightStart, NULL, NULL) == 1) {
                // Drift no longer takes contacts; unsigned trig scaling precedes s16 truncation.
                worldCollisionUnlinkBody(&flame->body);
                task->state     = PYKE_FLAME_STATE_DRIFT;
                work->move.vx   = (u32)rcos(work->angle) >> PYKE_FLAME_DRIFT_TRIG_SHIFT;
                work->move.vy   = (u32)rsin(work->angle) >> PYKE_FLAME_DRIFT_TRIG_SHIFT;
                driftRandom     = gRandomLcgState * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT;
                gRandomLcgState = driftRandom;
                work->move.vz   = (u32)rsin((driftRandom >> 16) & PYKE_FLAME_SPIN_ANGLE_MASK) >> PYKE_FLAME_DRIFT_TRIG_SHIFT;
                return;
            }
            if (work->age >= PYKE_FLAME_LIFETIME_TICKS) {
                worldCollisionUnlinkBody(&flame->body);
                effectKillTask(work, task);
                return;
            }
            worldCollisionClearContacts(flame->contacts);
            return;
        case PYKE_FLAME_STATE_DRIFT:
            work->scale = work->scale + PYKE_FLAME_DRIFT_SIZE_STEP;
            _pykeFlameAdvanceCoord(coord, work);
            _pykeFlameDrawBlob(MATRIX_TRANS(&coord->workm),
                               (work->age >> 1) + 1, work->scale,
                               work->angle);
            if (work->age >= PYKE_FLAME_LIFETIME_TICKS) {
                effectKillTask(work, task);
            }
            break;
    }
}
