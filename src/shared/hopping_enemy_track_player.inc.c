/* Part of the hopping enemy library; see hopping_enemy.h. */

/// Aims at the nearer of the two player actors: saves the root position in
/// `field_60`, stores the offset to that actor in `field_88`..`field_8C` and
/// its horizontal distance in `field_43A`, and its heading relative to
/// `field_7A` in `field_444`. Nothing but the position is updated while
/// player slot 0 is empty.
void hopperTrackPlayer(Task* arg0)
{
    Actor341700Work* work;
    GfxCoord*        coord;
    GfxCoord*        other;
    Task*            player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist;
    s32              dist2;

    work              = (Actor341700Work*)arg0->work;
    coord             = arg0->extra.tmd->coords;
    player            = Gp_ActorSlots[0];
    work->field_60.vx = coord->coord.t[0];
    work->field_60.vy = coord->coord.t[1];
    work->field_60.vz = coord->coord.t[2];
    if (player != NULL) {
        other = player->extra.tmd->coords;
        d0.vx = other->coord.t[0] - coord->coord.t[0];
        d0.vy = other->coord.t[1] - coord->coord.t[1];
        d0.vz = other->coord.t[2] - coord->coord.t[2];
        dist  = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] != NULL) {
            other = Gp_ActorSlots[1]->extra.tmd->coords;
            d1.vx = other->coord.t[0] - coord->coord.t[0];
            d1.vy = other->coord.t[1] - coord->coord.t[1];
            d1.vz = other->coord.t[2] - coord->coord.t[2];
            dist2 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist2 < dist) {
                dist  = dist2;
                d0.vx = d1.vx;
                d0.vy = d1.vy;
                d0.vz = d1.vz;
            }
        }
        // The loop notes keep VectorNormalSS's argument setup below these stores.
        do {
            work->field_88  = d0.vx;
            work->field_8A  = d0.vy;
            work->field_8C  = d0.vz;
            work->field_43A = dist;
        } while (0);
        VectorNormalSS(&d0, &d0);
        work->field_444 = (ratan2(d0.vx, d0.vz) - work->field_7A) & 0xFFF;
    }
}
