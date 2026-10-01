/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// The lunge cycle, a state handler run out of a 0x10-byte scratch stack
/// block. State 0 is the wind-up: it holds `field_69C` at 0 until the
/// animation reaches its start frame, aims `field_6A4` at the player and
/// compares it with the enemy's own facing `field_6A2` - past 0x581 apart it
/// gives up and turns (animation 3, or animation 4 when `field_6DC` says it has
/// already turned once), within 0x80 it raises the body node's 0xC000 flags
/// and commits as soon as `field_6B2` reports contact. State 1 picks what to do
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
    state                    = work->field_6A8;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            speed = 0;
            if (work->field_698 >= gGolemPawnRookAnimBlendFrames[work->field_694]) {
                speed = 0x14;
            }
            work->field_69C = speed;
            work->field_69E = GOLEM_PAWN_ROOK_WIND_UP_TURN;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
            yaw             = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
            work->field_6A2 = yaw;
            deltaYaw        = work->field_6A4 - yaw;
            magnitude       = __builtin_abs(deltaYaw);
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
                if (work->field_6DC == 0) {
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 4;
                    work->field_6A8 = 1;
                    work->field_6DC = 0;
                }
            }
            if (angle < 0x80) {
                flags                 = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_47C.flags = flags;
                if (work->field_6B2 != 0) {
                    work->field_47C.flags = (u16)(flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                    work->field_6A8       = 1;
                    work->field_6DC       = 0;
                }
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            dz              = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            delta->vz       = dz;
            dx              = delta->vx;
            if (SquareRoot0((dx * dx) + (dz * dz)) < GOLEM_PAWN_ROOK_LUNGE_RANGE) {
                work->field_6A6 = GOLEM_PAWN_ROOK_LUNGE_STATE;
                work->field_6A8 = 0;
                work->field_694 = GOLEM_PAWN_ROOK_LUNGE_ANIM;
            } else {
#if GOLEM_PAWN_ROOK_TYPE == GOLEM_ROOK
                random          = (gRandomLcgState * RANDOM_LCG_MULTIPLIER) + RANDOM_LCG_INCREMENT;
                gRandomLcgState = random;
                if (!((random >> 0x10) & ((1 << (work->field_6C0 + 1)) - 1)) && !(gPlayerStatus.statusFlags & PLAYER_STATUS_SILENCE) &&
                    work->field_6C4 != 0) {
                    work->field_6A6 = 5;
                    work->field_6A8 = 0;
                    work->field_694 = 0xA;
                    work->field_6AE = 0;
                    work->field_6C2 = 1;
                    work->field_6B6 = 0;
                    work->field_6C0++;
                } else {
                    work->field_6A6 = GOLEM_PAWN_ROOK_WALK_STATE;
                    work->field_6A8 = 0;
                    work->field_694 = GOLEM_PAWN_ROOK_WALK_ANIM;
                    work->field_6AE = 0;
                }
#else
                work->field_6A6 = GOLEM_PAWN_ROOK_WALK_STATE;
                work->field_6A8 = 0;
                work->field_694 = GOLEM_PAWN_ROOK_WALK_ANIM;
                work->field_6AE = 0;
#endif
            }
            break;
        case 2:
            work->field_69C       = 0;
            work->field_69E       = 0;
            flags2                = work->field_47C.flags | (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED);
            work->field_47C.flags = flags2;
            if (work->field_6B2 != 0) {
                work->field_47C.flags = (u16)(flags2 & (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED)));
                work->field_694       = 2;
                work->field_6A8       = 0;
            } else if (work->field_698 >= 0x60) {
                delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
                delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
                work->field_6A4 = (u16)(ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF);
                yaw2            = ratan2(coord->coord.m[0][2], coord->coord.m[2][2]) & 0xFFF;
                work->field_6A2 = yaw2;
                deltaYaw2       = work->field_6A4 - yaw2;
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
                    work->field_694 = 3;
                    work->field_6A8 = 3;
                } else {
                    work->field_694 = 2;
                    work->field_6A8 = 0;
                }
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_69E = 0x3B;
            if (work->field_698 >= 0x23) {
                work->field_694 = 2;
                work->field_6A8 = 0;
                work->field_6DC = 1;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x10);
}
