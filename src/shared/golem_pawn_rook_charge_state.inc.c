/* Part of the Pawn/Rook GOLEM library; see golem_pawn_rook.h. */

/// The charge, a state handler sharing `step` with the rest of
/// the overlay and measuring the offset to the player through a 0x10-byte
/// scratch stack block. State 0 waits out the wind-up: from frame 0x47 it
/// mirrors `shieldHp` into `shieldRaised` and commits to the charge
/// (`forwardSpeed` = 0x84) unless the player is already 1000 units away, zeroes
/// the damage total `interruptDamage` on frame 0x46, and hands over to state 1 on
/// animation 6 once the animation is past its start frame plus 0x52. State 1
/// drives the charge, aiming `targetYaw` at the player each frame, switching to
/// state 2 on animation 7 within 1500 units and on animation 4 once the heading
/// has drifted more than 0x100 from `yaw`. State 2 raises
/// `strikeBody`'s 0x8000 flag and queues the cue on frame 0xD, drops the flag on frame
/// 0x1E, and from frame 0x3B picks animation 8 (back to state 0) within 3000
/// units or animation 4 otherwise. An `interruptDamage` of 0x4C at any point aborts
/// the whole cycle back to animation 8.
void golemPawnRookChargeState(Task* arg0)
{
    s16                state;
    s16                diff;
    s16                turn;
    void**             scratch;
    s32                dx;
    s32                dz;
    s32                dxAim;
    s32                dzAim;
    s32                dxHold;
    s32                dzHold;
    s32                dist;
    s32                sound;
    s32                pan;
    u8*                head;
    GolemPawnRookWork* work;
    GfxCoord*          self;
    VECTOR*            delta;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - 0x10;
    delta                    = (VECTOR*)(head - 0x10);
    work                     = arg0->work;
    state                    = work->step;
    self                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if ((work->animFrame >= 0x47) && (work->shieldRaised           = (s16)(work->shieldHp > 0),
                                              ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]),
                                              dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2],
                                              delta->vz                    = dz,
                                              dx                           = ((VECTOR*)(head - 0x10))->vx,
                                              ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->forwardSpeed = 0x84;
            } else {
                work->forwardSpeed = 0;
            }
            work->turnRate = 0;
            if (work->animFrame == 0x46) {
                work->interruptDamage = 0;
                work->attackActive    = 1;
            }
            if ((work->animFrame >= 0x47) && (work->interruptDamage >= 0x4C)) {
                work->behavior         = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                work->step             = 0;
                work->forwardSpeed     = 0;
                work->turnRate         = 0;
                work->attackActive     = 0;
                work->strikeBody.flags = (u16)(work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
            }
            if (work->animFrame >= (gGolemPawnRookAnimBlendFrames[work->anim] + 0x52)) {
                work->step = 1;
                work->anim = 6;
            }
            break;
        case 1:
            work->forwardSpeed           = 0x84;
            work->turnRate               = GOLEM_PAWN_ROOK_CHARGE_TURN;
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            delta->vz                    = (s32)(gPlayerStatus.coordMtx->t[2] - self->coord.t[2]);
            work->targetYaw              = (s16)(ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF);
            dxAim                        = ((VECTOR*)(head - 0x10))->vx;
            dzAim                        = delta->vz;
            dist                         = SquareRoot0((dxAim * dxAim) + (dzAim * dzAim));
            if (work->interruptDamage >= 0x4C) {
                goto reset;
            }
            if (dist < 0x5DC) {
                work->step         = 2;
                work->anim         = 7;
                work->forwardSpeed = 0;
            } else {
                diff = (ratan2((s32)(s16)((VECTOR*)(head - 0x10))->vx, (s32)(s16)delta->vz) & 0xFFF) - work->yaw;
                turn = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (turn > 0x100) {
                    work->behavior     = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step         = 2;
                    work->anim         = 4;
                    work->attackActive = 0;
                    work->shieldRaised = 0;
                }
            }
            break;
        case 2:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if ((work->animFrame < 0xD) && (work->interruptDamage >= 0x4C)) {
            reset:
                work->behavior         = GOLEM_PAWN_ROOK_BEHAVIOR_STAGGER;
                work->step             = 0;
                work->forwardSpeed     = 0;
                work->turnRate         = 0;
                work->attackActive     = 0;
                work->shieldRaised     = 0;
                work->strikeBody.flags = (u16)(work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
                break;
            }
            if (work->animFrame == 0xD) {
                work->strikeBody.flags = (u16)(work->strikeBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->strikeBody.key   = Gp_PackPair(gGolemPawnRookAttacks, 1);
                sound                  = gGolemPawnRookSwingCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan                    = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->animFrame == 0x1E) {
                work->strikeBody.flags = (u16)(work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            if (work->animFrame >= 0x3B) {
                work->shieldRaised = 0;
                dxHold             = gPlayerStatus.coordMtx->t[0] - self->coord.t[0];
                delta->vx          = dxHold;
                dzHold             = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
                delta->vz          = dzHold;
                if (SquareRoot0((dxHold * dxHold) + (dzHold * dzHold)) < 0xBB8) {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_SWORD_SWING;
                    work->step     = 0;
                    work->anim     = 8;
                } else {
                    work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                    work->step     = 2;
                    work->anim     = 4;
                }
            }
            break;
    }
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
