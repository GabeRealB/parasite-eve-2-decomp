/* Part of the Knight/Bishop GOLEM library; see golem_knight_bishop.h. */

/// Runs the actor's approach sequence off the box region it last hit. State 0
/// plants the display object on that region's post (`x`, `z`), faces it along
/// `param.heading` and queues the cue `field_6B8`, parking the state at 1 when
/// the player is within 0xDAC and at 2 otherwise. States 1 and 2 re-aim for `field_6D6` frames; state 2
/// closes in until the player is within 0xA8C (state 3) or turns away by more
/// than 0x180 (state 4). State 3 steps `field_6C8` through the frame table
/// `gGolemKnightBishopFrameSteps` and fires its per-frame events; state 4 counts
/// `field_6D4` down back to state 0.
void golemKnightBishopBoxApproachSeq(Task* arg0)
{
    u8*                             head;
    GolemKnightBishopOffsetScratch* sc;
    s32                             state;
    GolemKnightBishopWork*          work;
    GfxCoord*                       coord;
    s32                             pan;
    s32                             snd;
    s32                             i;
    s16                             diff;
    s16                             dist;
    s32                             adiff;
    s32                             val;
    s16                             timer;

    head                     = SCRATCH_STACK_CURSOR(u8);
    SCRATCH_STACK_CURSOR(u8) = head - sizeof(GolemKnightBishopOffsetScratch);
    sc                       = (GolemKnightBishopOffsetScratch*)(head - sizeof(GolemKnightBishopOffsetScratch));
    work                     = arg0->work;
    state                    = work->field_6CE;
    coord                    = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            coord->coord.t[0] = work->field_6B4[work->field_708].x;
            coord->coord.t[1] = gPlayerStatus.coordMtx->t[1];
            coord->coord.t[2] = work->field_6B4[work->field_708].z;
            sc->in.vx         = 0;
            sc->in.vy         = work->field_6B4[work->field_708].param.heading;
            sc->in.vz         = 0;
            RotMatrix(&sc->in, &coord->coord);
            sc->out.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xDAC) {
                work->field_6C0 = 4;
                work->field_6CE = 1;
                work->field_6D4 = 0x1E;
            } else {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            work->field_6DA = 1;
            work->field_6DC = 0x14;
            work->field_6DE = 0xA;
            work->field_6B8 = gGolemKnightBishopApproachCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
            SndEvt_EnqueueType6(work->field_6B8, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            Gp_ArmStateF0(1);
            if (work->field_6C6 == 0) {
                work->field_49A |= WORLD_COLLISION_BODY_PAIR_ENABLED;
                work->field_494  = work->field_716 | 0x30000;
            }
            work->field_6D6 = GOLEM_KNIGHT_BISHOP_AIM_TIME;
            work->field_70A = 0;
            work->field_6F2 = 1;
            break;
        case 1:
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6C0 = 6;
                work->field_6CE = 2;
            }
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                golemKnightBishopAimFromPart(arg0);
            }
            if (work->field_70A >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 2:
            val = 0;
            if (work->field_6C4 >= 8) {
                val = 0x78;
            }
            work->field_6C8 = val;
            if (work->field_6D6 > 0) {
                work->field_6D6--;
                golemKnightBishopAimFromPart(arg0);
            }
            sc->out.vx = gPlayerStatus.coordMtx->t[0] - coord->coord.t[0];
            sc->out.vz = gPlayerStatus.coordMtx->t[2] - coord->coord.t[2];
            if ((s16)SquareRoot0(sc->out.vx * sc->out.vx + sc->out.vz * sc->out.vz) < 0xA8C) {
                work->field_6C0  = 7;
                work->field_6CE  = 3;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            } else {
                diff = (ratan2((s16)sc->out.vx, (s16)sc->out.vz) & 0xFFF) - work->field_6B4[work->field_708].param.heading;
                dist = (abs(diff) >= 0x800) ? ((diff > 0) ? 0x1000 - diff : diff + 0x1000) : abs(diff);
                if (dist > 0x180) {
                    work->field_6C0  = 4;
                    work->field_6CE  = 4;
                    work->field_6DA  = 3;
                    work->field_6DC  = 0x14;
                    work->field_6DE  = 0xA;
                    work->field_6F2  = 0;
                    work->field_6D4  = work->field_6DC + 0xA;
                    work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
                    work->field_6BC  = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                    SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
                }
            }
            if (work->field_70A >= GOLEM_KNIGHT_BISHOP_HIT_WEIGHT) {
                work->field_6CC  = 4;
                work->field_6CE  = 0;
                work->field_6DA  = 7;
                work->field_6C8  = 0;
                work->field_62A &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ (WORLD_COLLISION_BODY_GRID_ENABLED | WORLD_COLLISION_BODY_PAIR_ENABLED));
            }
            break;
        case 3:
            for (i = 0; work->field_6C4 > gGolemKnightBishopFrameSteps[i].frame; i++) {
            }
            work->field_6C8 = gGolemKnightBishopFrameSteps[i].value;
            if (work->field_6C4 == 0x12) {
                snd = gGolemKnightBishopStrikeCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(snd, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            if (work->field_6C4 == 0x14) {
                work->field_56C  = arg0->extra.tmd->coords;
                work->field_576  = -0x4B0;
                work->field_578  = 0x1F4;
                work->field_574  = 0;
                work->field_580  = 0x3E8;
                work->field_57C  = Gp_PackPair(gGolemKnightBishopAttacks, 2);
                work->field_582 |= WORLD_COLLISION_BODY_PAIR_ENABLED;
            }
            if (work->field_6C4 == 0x20) {
                work->field_6F2  = 0;
                work->field_582 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_PAIR_ENABLED);
            }
            if (work->field_6C4 == 0x5A) {
                work->field_6DA = 3;
                work->field_6DC = 0x14;
                work->field_6DE = 0xA;
                work->field_6CE = 4;
                work->field_6D4 = work->field_6DC + 0xA;
                work->field_6BC = gGolemKnightBishopPainCue | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                SndEvt_EnqueueType6(work->field_6BC, (s8)worldCoordGetOriginAudioPan(coord), (s8)worldCoordGetOriginAudioDepth(coord));
            }
            break;
        case 4:
            work->field_6C8 = 0;
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                work->field_6CC = 0;
                work->field_6CE = 0;
            }
            break;
    }
    SCRATCH_STACK_RELEASE_BYTES(sizeof(GolemKnightBishopOffsetScratch));
}
