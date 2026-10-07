/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// Approach-cycle state machine, entry 6 of `gGolemPawnRookStates` for the
/// second half of the fight. State 0 waits out the opening clip; state 1 backs
/// away while tracking the player and running the aim helper, for `GOLEM_PAWN_ROOK_BACKOFF_FRAMES`;
/// state 2 measures the distance and yaw error to the companion in slot 3 and
/// either breaks off (too close) or commits to the lunge; state 3 keeps facing
/// the companion, counts the shots in `shotsSinceReload` / `burstShots` and picks the
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
    switch (work->step) {
        case 0:
            if (work->animFrame >= 0x14) {
                work->step  = 1;
                work->anim  = 0x1E;
                work->timer = 0;
            }
            break;
        case 1:
            work->forwardSpeed = -0x16;
            work->turnRate     = 0x1E;
            work->shieldRaised = work->shieldHp > 0;
            delta->vx          = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            delta->vz          = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            work->targetYaw    = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
            golemPawnRookAimLaserSight(arg0);
            work->timer++;
            if (work->timer >= GOLEM_PAWN_ROOK_BACKOFF_FRAMES) {
                work->step             = 2;
                work->timer            = 0;
                work->laserBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            work->shieldRaised = work->shieldHp > 0;
            target             = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
            delta->vx          = target->workm.t[0] - coord->workm.t[0];
            normal             = delta + 1;
            delta->vy          = target->workm.t[1] - coord->workm.t[1];
            delta->vz          = target->workm.t[2] - coord->workm.t[2];
            VectorNormal(delta, normal);
            ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal, delta);
            dx = delta->vx;
            dz = delta->vz;
            if (SquareRoot0((dx * dx) + (dz * dz)) < 0x7D0) {
                work->behavior     = GOLEM_PAWN_ROOK_BEHAVIOR_LAUNCHER_STRIKE;
                work->step         = 0;
                work->anim         = 0x10;
                work->shieldRaised = 0;
                break;
            }
            diff = (ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF) - work->yaw;
            mag  = __builtin_abs(diff);
            if (mag < 0x800) {
                angle = mag;
            } else if (diff > 0) {
                angle = 0x1000 - diff;
            } else {
                angle = diff + 0x1000;
            }
            if (angle >= 0x101) {
                work->step         = 6;
                work->anim         = 0xE;
                work->shieldRaised = 0;
            } else {
                work->step            = 3;
                work->anim            = 0xD;
                work->attackActive    = 1;
                work->fireRequest     = 1;
                work->interruptDamage = 0;
                work->shotsSinceReload++;
                work->burstShots++;
            }
            break;
        case 3:
            work->forwardSpeed = 0;
            work->shieldRaised = work->shieldHp > 0;
            if (work->animFrame < 3) {
                work->turnRate = 0;
            } else {
                target    = &(gameGetTaskSlot(GAME_TASK_SLOT_PLAYER))->extra.tmd->coords[2];
                delta->vx = target->workm.t[0] - coord->workm.t[0];
                normal2   = delta + 1;
                delta->vy = target->workm.t[1] - coord->workm.t[1];
                delta->vz = target->workm.t[2] - coord->workm.t[2];
                VectorNormal(delta, normal2);
                ApplyTransposeMatrixLV(&Gp_GridParams->viewCoord->workm, normal2, delta);
                work->targetYaw = ratan2((s16)delta->vx, (s16)delta->vz) & 0xFFF;
                work->turnRate  = 7;
            }
            if (work->burstShots != 0 && work->animFrame == gGolemPawnRookAnimBlendFrames[13] - 1) {
                work->fireRequest = 1;
                work->shotsSinceReload++;
                work->burstShots++;
            }
            if (work->interruptDamage >= 0x29) {
                work->behavior          = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                work->step              = 0;
                work->forwardSpeed      = 0;
                work->turnRate          = 0;
                work->attackActive      = 0;
                work->shieldRaised      = 0;
                work->strikeBody.flags &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            } else if (work->shotsSinceReload >= 6) {
                if (work->animFrame >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                    work->step                        = 4;
                    work->anim                        = 0xF;
                    work->shotsSinceReload            = 0;
                    work->burstShots                  = 0;
                    gGolemPawnRookAnimBlendFrames[13] = 0;
                    work->attackActive                = 0;
                }
            } else if (work->burstShots < GOLEM_PAWN_ROOK_STRIKE_LIMIT) {
                if (work->animFrame >= gGolemPawnRookAnimBlendFrames[13] + 3) {
                    gGolemPawnRookAnimBlendFrames[13] = 3;
                    work->anim                        = 0xD;
                    work->playingAnim                 = 0x1E;
                }
            } else if (work->animFrame >= gGolemPawnRookAnimBlendFrames[13] + 0x16) {
                work->step                        = 1;
                work->burstShots                  = 0;
                work->anim                        = 0x1E;
                gGolemPawnRookAnimBlendFrames[13] = 0;
                work->attackActive                = 0;
            }
            break;
        case 4:
            if (work->animFrame == 0x1A) {
                effectSpawn(EFFECT_RELOAD_EMITTER, &arg0->extra.tmd->coords[7], 0x6000C, NULL);
            }
            work->shieldRaised = 0;
            if (work->animFrame >= 0x87) {
                work->step = 5;
            }
            break;
        case 5:
            work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
            work->step     = 2;
            work->anim     = 4;
            break;
        case 6:
            if (work->animFrame >= 0x19) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = 0;
                work->anim     = 2;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(0x20);
}
