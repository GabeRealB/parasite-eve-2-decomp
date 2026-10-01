/* Part of the Pawn and Rook GOLEM library; see golem_pawn_rook.h. */

/// Per-frame tick for the enemy's lunge cycle, sharing the `field_6A8` state
/// with the rest of the overlay. State 0 measures the offset to the player
/// through a 0x10-byte scratch stack block: over the window from frame 0x22
/// to 0x26 of the current animation the enemy commits to the lunge
/// (`field_69C` = 0x84) unless the player is already 1000 units away, aims
/// `field_6A4` at them every frame, raises the 0x5E4 node's 0x8000 flag on
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
    state                    = work->field_6A8;
    self                     = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            ((VECTOR*)(head - 0x10))->vx = (s32)(gPlayerStatus.coordMtx->t[0] - self->coord.t[0]);
            dz                           = gPlayerStatus.coordMtx->t[2] - self->coord.t[2];
            delta->vz                    = dz;
            startFrame                   = gGolemPawnRookAnimBlendFrames[work->field_694];
            frame                        = work->field_698;
            if ((frame >= (startFrame + 0x22)) && ((startFrame + 0x26) >= frame) && (dx = ((VECTOR*)(head - 0x10))->vx, ((SquareRoot0((dx * dx) + (dz * dz)) < 0x3E8) == 0))) {
                work->field_69C = 0x84;
            } else {
                work->field_69C = 0;
            }
            work->field_69E = 0x14;
            work->field_6A4 = (s16)(ratan2((s32)(s16)delta->vx, (s32)(s16)delta->vz) & 0xFFF);
            if (work->field_698 == (gGolemPawnRookAnimBlendFrames[work->field_694] + 0x20)) {
                work->field_5E4.flags = (u16)(work->field_5E4.flags | WORLD_COLLISION_BODY_PAIR_ENABLED);
                work->field_5E4.key   = Gp_PackPair(gGolemPawnRookAttacks, 0);
            }
            if (work->field_698 == (gGolemPawnRookAnimBlendFrames[work->field_694] + 0x21)) {
                sound = gGolemPawnRookSwingCue.value | ((((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan   = (s8)worldCoordGetOriginAudioPan(self);
                SndEvt_EnqueueType6(sound, (s32)pan, (s32)(s8)worldCoordGetOriginAudioDepth(self));
            }
            if (work->field_698 >= (gGolemPawnRookAnimBlendFrames[work->field_694] + 0x27)) {
                work->field_6A8       = 1;
                work->field_694       = 9;
                work->field_5E4.flags = (u16)(work->field_5E4.flags & (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 1:
            work->field_69C = 0;
            work->field_69E = 0;
            if (work->field_698 >= 0x5E) {
                work->field_6A6 = 2;
                work->field_6A8 = 2;
                work->field_694 = 4;
            }
            break;
    }
    scratch = SCRATCH_HEAD_ADDR;
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}
