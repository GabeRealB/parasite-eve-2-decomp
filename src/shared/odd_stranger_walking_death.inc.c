/* Part of the Odd Stranger library; see odd_stranger.h. */

/* Where the effect offsets are built: the work block in the build that keeps
 * one, a stack vector otherwise. */
#if ODD_STRANGER_HIT_FX_OFFSET
#define ODD_STRANGER_FX_OFFSET work->field_8C0
#else
#define ODD_STRANGER_FX_OFFSET vec
#endif

/// State-2 clip body and its 0x1A successor, as in the Horned Stranger's
/// `func_actor_401300_8013BB30` and `Actor01900_Fn0892C`. On the live-actor flag
/// it arms the effect node, seeds the 0x8C0 spawn offset and the animation
/// slots, and spawns clip 0x60030. `field_6` then counts up under `field_89E`:
/// the state-2 arm waits 0x10 frames on `flags_68` bit 2 before switching to
/// 0x1A, runs the `0x12C`/0xA range probe and the `field_A30` obstacle slide,
/// and spawns the three tinted key-frame effects at counts 3, 5 and 6; the
/// state-0x1A arm gates on `flags_68` bit 0x100, dispatches the one-shot actions
/// off `field_6 - 0x19`, and from 0x1A on rebuilds the root coordinate through
/// `ratan2` at scale `0x1194 - (field_6 - 0x14) * 0xB`. Both arms end in
/// `oddStrangerDrive` and `actorResetYaw` on nodes 2..10.
void oddStrangerWalkingDeath(Task* arg0)
{
#if !ODD_STRANGER_HIT_FX_OFFSET
    SVECTOR vec;
#endif
    OddStrangerWork* work;
    Enemy*           enemy;
    u16              next;
    s16              cur;

    work  = arg0->work;
    enemy = arg0->spawnArg2.pointer;
    if (work->field_4 != 0) {
        work->field_8D0.radius        = ODD_STRANGER_BODY_RADIUS;
        work->field_A10.flags        |= WORLD_COLLISION_BODY_GRID_ENABLED;
        enemy->node.state.parts.flags = WORLD_TARGET_NOT_LOCKABLE;
        work->field_8AE               = 0;
        work->field_6                 = 0;
        ODD_STRANGER_FX_OFFSET.vx     = 0x64;
        ODD_STRANGER_FX_OFFSET.vz     = 0;
        ODD_STRANGER_FX_OFFSET.vy     = 0;
        work->field_89E               = 2;
        work->field_898               = 1;
        work->field_8A2               = 0x10;
        Gp_SpawnEff(EFFECT_030, arg0->extra.tmd->coords + 1, 0x10300, &ODD_STRANGER_FX_OFFSET);
        work->field_6 = 0;
    }
    next          = work->field_6 + 1;
    work->field_6 = next;
    switch (work->field_89E) {
        case 2:
            if ((s16)next >= 0x10 && (work->flags_68.half & 2)) {
                work->field_89E = 0x1A;
                work->field_898 = 2;
                work->field_8A2 = 0x10;
                work->field_89A = 0;
            }
            if ((s16)detectPlayerOutOfReach(arg0->extra.tmd->coords, 0x12C, ODD_STRANGER_WALK_STEP) != 0) {
                actorMoveForward(arg0->extra.tmd->coords, ODD_STRANGER_WALK_STEP);
            }
            ActorContact_PushContact(arg0->extra.tmd->coords, work->field_A30, 0xC);
            if ((s16)work->field_6 == 3) {
                D_80114B34[5].data.model  = &gOddStrangerBurstModelA;
                ODD_STRANGER_FX_OFFSET.vz = 0x64;
                ODD_STRANGER_FX_OFFSET.vy = 0;
                ODD_STRANGER_FX_OFFSET.vx = 0;
                actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 9, 0x200, &ODD_STRANGER_FX_OFFSET), enemy);
            }
            if ((s16)work->field_6 == 5) {
                D_80114B34[5].data.model = ODD_STRANGER_BURST_MODEL_5;
                actorTintEffect(Gp_SpawnEff(0xA0000 | 5, arg0->extra.tmd->coords + 1, 0x200, NULL), enemy);
            }
            if ((s16)work->field_6 == 6) {
                D_80114B34[5].data.model = &gOddStrangerBurstModelC;
                actorTintEffect(Gp_SpawnEff(EFFECT_BURST_BODY_PART_BANK10, arg0->extra.tmd->coords + 3, 0x200, NULL), enemy);
            }
            break;
        case 0x1A:
            if (!(work->flags_68.half & ANIMATION_SLOT_SETTLED)) {
                work->field_6 = 0;
            }
            switch ((s16)(work->field_6 - 0x19)) {
                case 0:
                    Gp_ReleaseStateF0Add(arg0, 0xA);
                    break;
                case 5:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_WEIGHTED);
                    Gp_SpawnEff(EFFECT_CORPSE_BURN, arg0->extra.tmd->coords + 2, 2, NULL);
                    break;
                case 23:
                    arg0->extra.tmd->flags = TMD_OBJECT_SEMI_TRANS;
                    break;
                case 17:
                    Gp_SetLightMode(enemy, ENEMY_COLOR_BLACK);
                    break;
                case 39:
                    arg0->extra.tmd->flags = TMD_OBJECT_SKIP_ACTIVE_DRAW;
                    work->field_0          = 0;
                    break;
            }
            cur = work->field_6;
            if (cur >= 0x1A) {
                actorRescaleYawY(arg0->extra.tmd->coords, 0x1194, 0x1194 - (cur - 0x14) * 0xB);
            }
            break;
    }
    oddStrangerDrive(arg0);
    actorResetYaw(arg0->extra.tmd->coords + 2);
    actorResetYaw(arg0->extra.tmd->coords + 3);
    actorResetYaw(arg0->extra.tmd->coords + 4);
    actorResetYaw(arg0->extra.tmd->coords + 5);
    actorResetYaw(arg0->extra.tmd->coords + 6);
    actorResetYaw(arg0->extra.tmd->coords + 7);
    actorResetYaw(arg0->extra.tmd->coords + 8);
    actorResetYaw(arg0->extra.tmd->coords + 9);
    actorResetYaw(arg0->extra.tmd->coords + 10);
}

#undef ODD_STRANGER_FX_OFFSET
