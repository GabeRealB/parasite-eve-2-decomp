/* Part of the lunging enemy library; see lunging_enemy.h. */

/// Entry 0xD of the `field_6A6` table. State 0 arms the reaction: `field_6AA`
/// picks animation 0x16 with a 0x42-frame budget or 0x1A with 0x31 frames,
/// the second body node's pose and radius are parked and its 0x4000 flag
/// raised, the third node's dropped, and the enemy's `reactionFlags` cleared.
/// State 1 plays the voice cues of the animation `field_6B8` selects at its
/// frame marks and, when the `field_6AE` budget runs out, hands the task over
/// to state 2.
void lungerCollapseState(Task* arg0)
{
    Actor105600Work* work;
    GfxCoord*        self;
    s32              snd;
    s16              state;

    work  = arg0->work;
    self  = arg0->extra.tmd->coords;
    state = work->field_6A8;

    switch (state) {
        case 0:
            if (work->field_6AA == 0) {
                work->field_694        = 0x16;
                work->field_6A8        = 1;
                work->field_6B8        = 1;
                work->field_6AE        = 0x42;
                work->field_4CC.pos.vz = -0xA7;
            } else {
                work->field_694        = 0x1A;
                work->field_6A8        = 1;
                work->field_6B8        = 2;
                work->field_6AE        = 0x31;
                work->field_4CC.pos.vz = 0x109;
            }
            work->field_4CC.radius                             = 0x15E;
            work->field_69C                                    = 0;
            work->field_69E                                    = 0;
            work->field_6DE                                    = 1;
            work->field_4CC.flags                             |= WORLD_COLLISION_BODY_GRID_ENABLED;
            work->field_564.flags                             &= (WORLD_COLLISION_BODY_FLAGS_MASK ^ WORLD_COLLISION_BODY_GRID_ENABLED);
            ((GpEnemy*)arg0->spawnArg2.pointer)->reactionFlags = 0;
            work->field_6D4                                    = 1;
            break;
        case 1:
            if (work->field_6DE == 1) {
                work->field_6DE = 2;
            }
            if (work->field_6B8 == 1) {
                if (work->field_698 == 0x14) {
                    s32 pan;

                    snd = gLungerVoiceCues[work->field_6D6 + 0xC] |
                          ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
                if (work->field_698 == 0x2C) {
                    s32 pan;

                    snd = gLungerVoiceCues[work->field_6D6 + 8] |
                          ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                    pan = (s8)Gp_GetObjPan(self);

                    SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
                }
            } else if (work->field_698 == 0x19) {
                s32 pan;

                snd = gLungerVoiceCues[work->field_6D6 + 8] |
                      ((((GpEnemy*)arg0->spawnArg2.pointer)->placeKey >> 0xC) << 8);
                pan = (s8)Gp_GetObjPan(self);

                SndEvt_EnqueueType6(snd, pan, (s8)gpGetObjDepth(self));
            }
            work->field_6AE--;
            if (work->field_6AE <= 0) {
                arg0->state     = 2;
                work->field_6A8 = 0;
                work->field_6D4 = 0;
            }
            break;
    }
}
