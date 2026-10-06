/* Part of the cap dialogue library; see cap_dialogue.h. */

/// Runs cap command `spawnArg1` and waits for it to finish. When the cap
/// reports event key 0xF it sends `Gp_MsgPlayerWeapon(1)`, undoing the
/// `Gp_MsgPlayerWeapon(0)` its spawner sent, and kills itself; any other key
/// runs the command again.
void capDialogueLoopTask(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_RunCapCmd1(task->spawnArg1.value);
            task->state += 1;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state += 1;
            break;
        case 2:
            if (Gp_GetCapEventKey() == 0xF) {
                Gp_MsgPlayerWeapon(1);
                taskKill(task);
            } else {
                task->state = 0;
            }
            break;
    }
}
