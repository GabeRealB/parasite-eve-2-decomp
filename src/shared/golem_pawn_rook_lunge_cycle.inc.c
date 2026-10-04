/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// The lunge cycle, a state handler run out of a 0x10-byte scratch stack
/// block. State 0 is the wind-up: it holds `forwardSpeed` at 0 until the
/// animation reaches its start frame, aims `targetYaw` at the player and
/// compares it with the enemy's own facing `yaw` - past 0x581 apart it
/// gives up and turns (animation 3, or animation 4 when `turnedAround` says it has
/// already turned once), within 0x80 it raises `sightBody`'s 0xC000 flags
/// and commits as soon as `playerSpotted` reports contact. State 1 picks what to do
/// next: inside `GOLEM_PAWN_ROOK_LUNGE_RANGE` of the player it lunges;
/// otherwise the Rook draws from `gRandomLcgState` through a mask that widens
/// by a bit each cycle and either circles (animation 0xA) or walks in, and the
/// Pawn always walks in. State 2 waits out the recovery and state 3 the turn.
void golemPawnRookLungeCycle(Task* arg0)
{
    s16 yaw;
    s16 yaw2;
    s16 state;
    s16 deltaYaw;
    s16 deltaYaw2;
    s16 speed;
    s32 magnitude;
    s32 magnitude2;
    s16 wrapped;
    s16 wrapped2;
    s16 angle;
    s32 dx;
    s32 dz;
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
    u32 random;
#endif
    u16                flags;
    u16                flags2;
    u8*                head;
    VECTOR*            delta;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    head                     = SCRATCH_STACK_CURSOR(u8);
    delta                    = (VECTOR*)(head - 0x10);
    SCRATCH_STACK_CURSOR(u8) = (u8*)delta;
    work                     = arg0->work;
    state                    = work->step;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->animFrame >= gGolemPawnRookAnimBlendFrames[work->anim]) {
                speed = 0x14;
            }
            work->forwardSpeed = speed;
            work->turnRate     = GOLEM_PAWN_ROOK_WIND_UP_TURN;
            delta->vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw    = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw                = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->yaw          = yaw;
            deltaYaw           = work->targetYaw - yaw;
            magnitude          = __builtin_abs(deltaYaw);
            if (magnitude < 0x800) {
                angle = magnitude;
            } else {
                if (deltaYaw > 0) {
                    wrapped = 0x1000 - deltaYaw;
                } else {
                    wrapped = deltaYaw + 0x1000;
                }
                angle = wrapped;
            }
            if (angle >= 0x581) {
                if (work->turnedAround == 0) {
                    work->anim = 3;
                    work->step = 3;
                } else {
                    work->anim         = 4;
                    work->step         = 1;
                    work->turnedAround = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->sightBody.flags = flags;
                if (work->playerSpotted != 0) {
                    work->sightBody.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->step            = 1;
                    work->turnedAround    = 0;
                }
            }
            break;
        case 1:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            delta->vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz                 = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz          = dz;
            dx                 = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < GOLEM_PAWN_ROOK_LUNGE_RANGE) {
                work->behavior = GOLEM_PAWN_ROOK_LUNGE_STATE;
                work->step     = 0;
                work->anim     = GOLEM_PAWN_ROOK_LUNGE_ANIM;
            } else {
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                if (!((random >> 0x10) & ((1 << (work->screamCount + 1)) - 1)) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_SILENCE) &&
                    work->screamCharges != 0) {
                    work->behavior        = GOLEM_PAWN_ROOK_BEHAVIOR_SCREAM;
                    work->step            = 0;
                    work->anim            = 0xA;
                    work->timer           = 0;
                    work->screamActive    = 1;
                    work->interruptDamage = 0;
                    work->screamCount++;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_WALK_STATE;
                    work->step     = 0;
                    work->anim     = GOLEM_PAWN_ROOK_WALK_ANIM;
                    work->timer    = 0;
                }
#else
                work->behavior = GOLEM_PAWN_ROOK_WALK_STATE;
                work->step     = 0;
                work->anim     = GOLEM_PAWN_ROOK_WALK_ANIM;
                work->timer    = 0;
#endif
            }
            break;
        case 2:
            work->forwardSpeed    = 0;
            work->turnRate        = 0;
            flags2                = work->sightBody.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->sightBody.flags = flags2;
            if (work->playerSpotted != 0) {
                work->sightBody.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->anim            = 2;
                work->step            = 0;
            } else if (work->animFrame >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->targetYaw = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->yaw       = yaw2;
                deltaYaw2       = work->targetYaw - yaw2;
                magnitude2      = __builtin_abs(deltaYaw2);
                if (magnitude2 < 0x800) {
                    angle = magnitude2;
                } else {
                    if (deltaYaw2 > 0) {
                        wrapped2 = 0x1000 - deltaYaw2;
                    } else {
                        wrapped2 = deltaYaw2 + 0x1000;
                    }
                    angle = wrapped2;
                }
                if (angle >= 0x581) {
                    work->anim = 3;
                    work->step = 3;
                } else {
                    work->anim = 2;
                    work->step = 0;
                }
            }
            break;
        case 3:
            work->forwardSpeed = 0;
            work->turnRate     = 0x3B;
            if (work->animFrame >= 0x23) {
                work->anim         = 2;
                work->step         = 0;
                work->turnedAround = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
