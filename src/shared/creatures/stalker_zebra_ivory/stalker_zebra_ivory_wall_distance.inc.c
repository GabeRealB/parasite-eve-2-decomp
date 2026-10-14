/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Reads the first probe-grid contact and clears the capsule's contact table.
///
/// Returns 0 without a grid hit, or 1 in hit-only mode. X/Z and X/Y modes
/// measure from the root's cached translation in the contact's view space,
/// narrowing deltas and the square root to s16; zero distance becomes 1.
/// The squared delta sum must fit s32 before the square-root call.
/// The root cache and eight-entry contact table must be current. The table
/// must keep its initialized LAST terminator; non-grid keys are skipped.
static s32 _stalkerZebraIvoryWallDistance(Task* task)
{
    enum { STALKER_ZEBRA_IVORY_WALL_PROBE_HIT_ONLY = 0,
           STALKER_ZEBRA_IVORY_WALL_PROBE_XZ       = 1,
           STALKER_ZEBRA_IVORY_WALL_PROBE_XY       = 2 };
    StalkerZebraIvoryWork* work;
    GfxCoord*              rootCoord;
    SVECTOR                contactDelta;
    s16                    distance;
    s32                    contactIndex;

    distance  = 0;
    work      = (StalkerZebraIvoryWork*)task->work;
    rootCoord = task->extra.tmd->coords;
    for (contactIndex = 0; contactIndex < (s32)ARRAY_SIZE(work->capsuleContacts); contactIndex++) {
        if ((work->capsuleContacts[contactIndex].key.value & WORLD_COLLISION_CONTACT_KIND_MASK) != WORLD_COLLISION_CONTACT_GRID) {
            distance = 0;
        } else {
            if (work->distanceMode == STALKER_ZEBRA_IVORY_WALL_PROBE_HIT_ONLY) {
                distance = 1;
            } else if (work->distanceMode == STALKER_ZEBRA_IVORY_WALL_PROBE_XZ) {
                contactDelta.vx = work->capsuleContacts[contactIndex].point.vx - rootCoord->workm.t[0];
                contactDelta.vy = 0;
                contactDelta.vz = work->capsuleContacts[contactIndex].point.vz - rootCoord->workm.t[2];
                distance        = SquareRoot0(contactDelta.vx * contactDelta.vx + contactDelta.vz * contactDelta.vz);
                if (distance == 0) {
                    distance = 1;
                }
            } else if (work->distanceMode == STALKER_ZEBRA_IVORY_WALL_PROBE_XY) {
                contactDelta.vx = work->capsuleContacts[contactIndex].point.vx - rootCoord->workm.t[0];
                contactDelta.vy = work->capsuleContacts[contactIndex].point.vy - rootCoord->workm.t[1];
                contactDelta.vz = 0;
                distance        = SquareRoot0(contactDelta.vx * contactDelta.vx + contactDelta.vy * contactDelta.vy);
                if (distance == 0) {
                    distance = 1;
                }
            }
            break;
        }
    }
    worldCollisionClearContacts(work->capsuleContacts);
    return distance;
}
