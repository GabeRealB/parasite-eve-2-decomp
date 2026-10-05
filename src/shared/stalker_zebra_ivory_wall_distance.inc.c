/* Part of the Ivory/Zebra Stalker library; see stalker_zebra_ivory.h. */

/// Distance to the first grid-face contact of the capsule, measured as the
/// `distanceMode` asks (1: in X/Z, 2: in X/Y, 0: just 1 for a hit); 0 when
/// there is none. Clears the contact table's occupancy.
s32 stalkerZebraIvoryWallDistance(Task* arg0)
{
    StalkerZebraIvoryWork* work;
    GfxCoord*              coord;
    SVECTOR                v;
    s16                    dist;
    s32                    i;

    dist  = 0;
    work  = (StalkerZebraIvoryWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    for (i = 0; i < 8; i++) {
        if ((work->capsuleContacts[i].key.value & 0xFFFF0000) != 0x100000) {
            dist = 0;
        } else {
            if (work->distanceMode == 0) {
                dist = 1;
            } else if (work->distanceMode == 1) {
                v.vx = work->capsuleContacts[i].point.vx - coord->workm.t[0];
                v.vy = 0;
                v.vz = work->capsuleContacts[i].point.vz - coord->workm.t[2];
                dist = SquareRoot0(v.vx * v.vx + v.vz * v.vz);
                if (dist == 0) {
                    dist = 1;
                }
            } else if (work->distanceMode == 2) {
                v.vx = work->capsuleContacts[i].point.vx - coord->workm.t[0];
                v.vy = work->capsuleContacts[i].point.vy - coord->workm.t[1];
                v.vz = 0;
                dist = SquareRoot0(v.vx * v.vx + v.vy * v.vy);
                if (dist == 0) {
                    dist = 1;
                }
            }
            break;
        }
    }
    worldCollisionClearContacts(work->capsuleContacts);
    return dist;
}
