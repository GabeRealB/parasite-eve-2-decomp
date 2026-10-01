/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Approach-cycle state machine, entry 6 of `gGolemPawnRookStates` for the
/// second half of the fight. State 0 waits out the opening clip; state 1 backs
/// away while tracking the player and running the aim helper, for `GOLEM_PAWN_ROOK_BACKOFF_FRAMES`;
/// state 2 measures the distance and yaw error to the companion in slot 3 and
/// either breaks off (too close) or commits to the lunge; state 3 keeps facing
/// the companion, counts the strikes in `field_6BC` / `field_6BE` and picks the
/// follow-up clip from them; state 4 fires the effect burst; states 5 and 6
/// hand back to the other handlers. The delta vector and its normal are carved
/// off the scratch stack and released on the way out.
void golemPawnRookCompanionCycle(Task* arg0)
{
    s16                diff;
    s32                mag;
    s16                angle;
    s32                dx;
    s32                dz;
    VECTOR*            delta;
    VECTOR*            normal;
    VECTOR*            normal2;
    GfxCoord*          target;
    GolemPawnRookWork* work;
    GfxCoord*          coord;

    delta = (VECTOR*)SCRATCH_STACK_RESERVE_BYTES(0x20);
    work  = arg0->work;
    coord = arg0->extra.tmd->coords;
    switch (work->field_6A8) {
        case 0:
            if (work->field_698 >= 0x14) {
                work->field_6A8 = 1;
                work->field_694 = 0x1E;
                work->field_6AE = 0;
            }
            break;
        case 1:
            work->field_69C = -0x16;
            work->field_69E = 0x1E;
            work->field_6CE = work->field_6D0 > 0;
            delta->vx       = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz       = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
            golemPawnRookAimLaserSight(arg0);
            work->field_6AE++;
            if (work->field_6AE >= GOLEM_PAWN_ROOK_BACKOFF_FRAMES) {
                work->field_6A8        = 2;
                work->field_6AE        = 0;
                work->field_61C.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            work->field_6CE = work->field_6D0 > 0;
            target          = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
            delta->vx       = target->workm.t[0] - coord->workm.t[0];
            normal          = delta + 1;
            delta->vy       = target->workm.t[1] - coord->workm.t[1];
            delta->vz       = target->workm.t[2] - coord->workm.t[2];
            VectorNormal(delta, normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, delta);
            dx = delta->vx;
            dz = delta->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->field_6A6 = 7;
                work->field_6A8 = 0;
                work->field_694 = 0x10;
                work->field_6CE = 0;
                break;
            }
            diff = (ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF) - work->field_6A2;
            mag  = __builtin_abs(diff);
            if (mag < 0x800) {
                angle = mag;
            } else if (diff > 0) {
                angle = 0x1000 - diff;
            } else {
                angle = diff + 0x1000;
            }
            if (angle >= 0x101) {
                work->field_6A8 = 6;
                work->field_694 = 0xE;
                work->field_6CE = 0;
            } else {
                work->field_6A8 = 3;
                work->field_694 = 0xD;
                work->field_6CC = 1;
                work->field_6BA = 1;
                work->field_6B6 = 0;
                work->field_6BC++;
                work->field_6BE++;
            }
            break;
        case 3:
            work->field_69C = 0;
            work->field_6CE = work->field_6D0 > 0;
            if (work->field_698 < 3) {
                work->field_69E = 0;
            } else {
                target    = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
                delta->vx = target->workm.t[0] - coord->workm.t[0];
                normal2   = delta + 1;
                delta->vy = target->workm.t[1] - coord->workm.t[1];
                delta->vz = target->workm.t[2] - coord->workm.t[2];
                VectorNormal(delta, normal2);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal2, delta);
                work->field_6A4 = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
                work->field_69E = 7;
            }
            if (work->field_6BE != 0 && work->field_698 == gGolemPawnRookAnimBlendFrames[13] - 1) {
                work->field_6BA = 1;
                work->field_6BC++;
                work->field_6BE++;
            }
            if (work->field_6B6 >= 0x29) {
                work->field_6A6        = 8;
                work->field_6A8        = 0;
                work->field_69C        = 0;
                work->field_69E        = 0;
                work->field_6CC        = 0;
                work->field_6CE        = 0;
                work->field_5E4.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (work->field_6BC >= 6) {
                if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                    work->field_6A8                   = 4;
                    work->field_694                   = 0xF;
                    work->field_6BC                   = 0;
                    work->field_6BE                   = 0;
                    gGolemPawnRookAnimBlendFrames[13] = 0;
                    work->field_6CC                   = 0;
                }
            } else if (work->field_6BE < GOLEM_PAWN_ROOK_STRIKE_LIMIT) {
                if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 3) {
                    gGolemPawnRookAnimBlendFrames[13] = 3;
                    work->field_694                   = 0xD;
                    work->field_696                   = 0x1E;
                }
            } else if (work->field_698 >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                work->field_6A8                   = 1;
                work->field_6BE                   = 0;
                work->field_694                   = 0x1E;
                gGolemPawnRookAnimBlendFrames[13] = 0;
                work->field_6CC                   = 0;
            }
            break;
        case 4:
            if (work->field_698 == 0x1A) {
                Gp_SpawnEff(0x6006E, &arg0->extra.tmd->coords[7], 0x6000C, NULL);
            }
            work->field_6CE = 0;
            if (work->field_698 >= 0x87) {
                work->field_6A8 = 5;
            }
            break;
        case 5:
            work->field_6A6 = 2;
            work->field_6A8 = 2;
            work->field_694 = 4;
            break;
        case 6:
            if (work->field_698 >= 0x19) {
                work->field_6A6 = 2;
                work->field_6A8 = 0;
                work->field_694 = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}
