/* Part of the Generator library; see generator.h. */

/// Teardown handler of the part task (its state 2), ticking only while the
/// gameplay mode `gSceneCombatState.actorControl` is 0. The first tick unlinks the enemy's lock-on
/// node and the part's collision object, drops the enemy's `recs`, sends the
/// main task's sound id `runningSoundId` a type-7 event, and undoes what the spawn
/// did for this sub-state (chosen by `field_46`): the same room call with 0
/// instead of 1, and game flag 0x147 or 0x148 set to 1 where the spawn set it
/// to 0. The enemy is destroyed once the counter `field_42` reaches 0x3D.
void generatorLifeSupportTeardown(Enemy* arg0, Task* arg1)
{
    GeneratorPart* part;
    GeneratorWork* parentWork;
    u16            timer;

    part       = (GeneratorPart*)arg1->work;
    parentWork = arg1->parent->work;
    if (gSceneCombatState.actorControl == SCENE_COMBAT_ACTORS_RUNNING) {
        timer          = part->field_42 + 1;
        part->field_42 = timer;
        if ((s16)timer == 1) {
            worldTargetUnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            arg0->recs = 0;
            SndEvt_EnqueueType7(parentWork->runningSoundId, 1);
            if (part->field_46 == 0) {
                func_neo_ark_power_plant_2_8017FD88(0);
                GameFlag_SetNibble(GAME_FLAG_POWER_PLANT_2_GENERATOR_PART_DOWN, 1);
            } else {
                func_neo_ark_power_plant_1_8017E524(0);
                GameFlag_SetNibble(GAME_FLAG_POWER_PLANT_1_GENERATOR_PART_DOWN, 1);
            }
        }
        if ((s16)part->field_42 >= 0x3D) {
            enemyDestroy(arg0, arg1);
        }
    }
}
