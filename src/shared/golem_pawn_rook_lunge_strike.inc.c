/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Runs the animation's mark events: measures `animFrame` against the three
/// frames `gGolemPawnRookAnimBlendFrames[anim]` marks out. At the 0x1C mark the
/// `strikeBody` key is packed from `gGolemPawnRookAttacks` and bit 0x8000 raised,
/// at 0x28 dropped; inside the 0x1C..0x1E window the player's distance decides
/// whether `forwardSpeed` parks at 0x64; and past 0x7A the actor hands over to
/// animation 4. The delta the distance is taken from is left in the scratch
/// vector it is accumulated in.
void golemPawnRookLungeStrikeState(Task* arg0)
{
    GolemPawnRookWork* work;
    GfxCoord*          self;
    VECTOR*            delta;
    s16                anim;
    s32                dx;
    s32                dz;
    s32                distance;

    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    delta = SCRATCH_STACK_CURSOR(VECTOR);
    work  = arg0->work;
    anim  = gGolemPawnRookAnimBlendFrames[work->anim];
    self  = arg0->extra.tmd->coords;
    if (work->animFrame == anim + 0x1C) {
        work->strikeBody.key    = damagePackAttackKey(gGolemPawnRookAttacks, 4);
        work->strikeBody.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else if (work->animFrame == anim + 0x28) {
        work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    anim = gGolemPawnRookAnimBlendFrames[work->anim];
    if ((work->animFrame >= anim + 0x1C) && (anim + 0x1E >= work->animFrame)) {
        dx        = gPlayerStatus.coordMtx->t[0] - self->coord.t[0];
        delta->vx = dx;
        dz        = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
        delta->vz = dz;
        distance  = SquareRoot0((delta->vx * delta->vx) + (delta->vz * delta->vz));
        if (distance < 0x3E8) {
            work->forwardSpeed = 0;
        } else {
            work->forwardSpeed = 0x64;
        }
    } else {
        work->forwardSpeed = 0;
    }
    if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim] + 0x7A) {
        work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
        work->step     = 2;
        work->anim     = 4;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
