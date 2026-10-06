/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame tick for the enemy's lunge cycle, sharing the `step` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte scratch stack block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`forwardSpeed` = 0x84) unless the player is already 1000 units away, aims
/// `targetYaw` at them every frame, raises `strikeBody`'s 0x8000 flag on
/// frame 0x20 and queues the cue on frame 0x21, then hands over to state 1 on
/// animation 9 once the animation is past frame 0x27. State 1 waits for frame
/// 0x5E and moves on to state 2 on animation 4.
void golemPawnRookBeamSwingState(Task* arg0)
{
    s16                startFrame;
    s16                state;
    s16                frame;
    void**             scratch;
    s32                dz;
    s32                sound;
    s32                dx;
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
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = gGolemPawnRookAnimBlendFrames[work->anim];
            frame                        = work->animFrame;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->forwardSpeed = 0x84;
            } else {
                work->forwardSpeed = 0;
            }
            work->turnRate  = 0x14;
            work->targetYaw = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->animFrame == (gGolemPawnRookAnimBlendFrames[work->anim] + 0x20)) {
                work->strikeBody.flags = (u16)(work->strikeBody.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->strikeBody.key   = damagePackAttackKey(gGolemPawnRookAttacks, 0);
            }
            if (work->animFrame == (gGolemPawnRookAnimBlendFrames[work->anim] + 0x21)) {
                sound = gGolemPawnRookSwingCue | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan   = (s8)worldCoordGetOriginAudioPan(self);
                sndEvtRequestScriptStart(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->animFrame >= (gGolemPawnRookAnimBlendFrames[work->anim] + 0x27)) {
                work->step             = 1;
                work->anim             = 9;
                work->strikeBody.flags = (u16)(work->strikeBody.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 1:
            work->forwardSpeed = 0;
            work->turnRate     = 0;
            if (work->animFrame >= 0x5E) {
                work->behavior = GOLEM_PAWN_ROOK_BEHAVIOR_ENGAGE;
                work->step     = 2;
                work->anim     = 4;
            }
            break;
    }
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
