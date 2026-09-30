/* Part of the power plant pod library; see power_plant_pod.h. */

/// Teardown handler of the part task (its state 2), ticking only while the
/// gameplay mode `Gp_StateF0.field_4` is 0. The first tick unlinks the enemy's lock-on
/// node and the part's collision object, drops the enemy's `recs`, sends the
/// main task's sound id `field_31C` a type-7 event, and undoes what the spawn
/// did for this sub-state (chosen by `field_46`): the same room call with 0
/// instead of 1, and game flag 0x147 or 0x148 set to 1 where the spawn set it
/// to 0. The enemy is destroyed once the counter `field_42` reaches 0x3D.
void podWeakPointTeardown(Enemy* arg0, Task* arg1)
{
    Actor05300Part* part;
    Actor05300Work* parentWork;
    u16             timer;

    part       = (Actor05300Part*)arg1->work;
    parentWork = (Actor05300Work*)arg1->parent->work;
    if (Gp_StateF0.field_4 == 0) {
        timer          = part->field_42 + 1;
        part->field_42 = timer;
        if ((s16)timer == 1) {
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&part->obj);
            arg0->recs = 0;
            SndEvt_EnqueueType7(parentWork->field_31C, 1);
            if (part->field_46 == 0) {
                func_neo_ark_power_plant_2_8017FD88(0);
                GameFlag_SetNibble(0x147, 1);
            } else {
                func_neo_ark_power_plant_1_8017E524(0);
                GameFlag_SetNibble(0x148, 1);
            }
        }
        if ((s16)part->field_42 >= 0x3D) {
            Gp_DestroyEnemy(arg0, arg1);
        }
    }
}
