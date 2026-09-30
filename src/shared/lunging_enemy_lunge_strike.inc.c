/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Runs the animation's mark events: measures `field_698` against the three
/// frames `gLungerAnimBlendFrames[field_694]` marks out. At the 0x1C mark the
/// body object is packed from `gLungerAttacks` and bit 0x8000 raised,
/// at 0x28 dropped; inside the 0x1C..0x1E window the player's distance decides
/// whether `field_69C` parks at 0x64; and past 0x7A the actor hands over to
/// animation 4. The delta the distance is taken from is left in the scratch
/// vector it is accumulated in.
void lungerLungeStrikeState(Task* arg0)
{
    Actor105600Work* work;
    GfxCoord*        self;
    VECTOR*          delta;
    s16              anim;
    s32              dx;
    s32              dz;
    s32              distance;

    SCRATCH_STACK_RESERVE_BLOCK(VECTOR);
    delta = SCRATCH_STACK_CURSOR(VECTOR);
    work  = (Actor105600Work*)arg0->work;
    anim  = gLungerAnimBlendFrames[work->field_694];
    self  = arg0->extra.tmd->coords;
    if (work->field_698 == anim + 0x1C) {
        work->field_5E4.key    = Gp_PackPair(gLungerAttacks, 4);
        work->field_5E4.flags |= WORLD_COLLISION_BODY_PAIR_ENABLED;
    } else if (work->field_698 == anim + 0x28) {
        work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
    }
    anim = gLungerAnimBlendFrames[work->field_694];
    if ((work->field_698 >= anim + 0x1C) && (anim + 0x1E >= work->field_698)) {
        dx        = gPlayerStatus.coordMtx->t[0] - self->coord.t[0];
        delta->vx = dx;
        dz        = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
        delta->vz = dz;
        distance  = SquareRoot0((delta->vx * delta->vx) + (delta->vz * delta->vz));
        if (distance < 0x3E8) {
            work->field_69C = 0;
        } else {
            work->field_69C = 0x64;
        }
    } else {
        work->field_69C = 0;
    }
    if (work->field_698 >= gLungerAnimBlendFrames[work->field_694] + 0x7A) {
        work->field_6A6 = 2;
        work->field_6A8 = 2;
        work->field_694 = 4;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
