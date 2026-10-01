/* Part of the Knight and Bishop GOLEM library; see golem_knight_bishop.h. */

/// Sequence 9, death while standing: collapses on the hit side and fades out,
/// queuing the fall cue on the impact frame. When the timer ends it moves the
/// task to its dead state (2).
void golemKnightBishopCollapseDeathSeq(Task* arg0)
{
    GolemKnightBishopWork* work;
    GfxCoord*              coord;
    s32                    state;
    s32                    snd;
    s32                    pan;
    s32                    frames;
    s16                    timer;

    work  = arg0->work;
    state = work->field_6CE;
    coord = arg0->extra.tmd->coords;
    switch (state) {
        case 0:
            if (work->field_6D2 == 0) {
                work->field_6C0 = 0xD;
                work->field_6CE = 1;
                work->field_6F0 = 1;
                work->field_6D4 = 0x42;
                work->field_490 = -0xA7;
            } else {
                work->field_6C0 = 0x11;
                work->field_6CE = 1;
                work->field_6F0 = 2;
                work->field_6D4 = 0x31;
                work->field_490 = 0x109;
            }
            work->field_498  = 0x15E;
            work->field_714  = 1;
            work->field_6DA  = 1;
            work->field_6DC  = 0x14;
            work->field_6DE  = 0xA;
            work->field_6F2  = 2;
            work->field_6C8  = 0;
            work->field_49A |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->field_502 &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            break;
        case 1:
            if (work->field_714 == state) {
                work->field_714 = 2;
            }
            frames = 0x19;
            if (work->field_6F0 == state) {
                frames = 0x2C;
            }
            if (work->field_6C4 == frames) {
                snd = gGolemKnightBishopAnimCues[work->field_712 + 8] | (((u16)((Enemy*)arg0->spawnArg2.pointer)->placeKey >> ENEMY_PLACE_INDEX_SHIFT) << 8);
                pan = (s8)worldCoordGetOriginAudioPan(coord);
                SndEvt_EnqueueType6(snd, pan, (s8)worldCoordGetOriginAudioDepth(coord));
            }
            timer           = work->field_6D4 - 1;
            work->field_6D4 = timer;
            if (timer <= 0) {
                arg0->state     = 2;
                work->field_6CE = 0;
                work->field_6F2 = 0;
            }
            break;
    }
}
