/* Part of the Glutton library; see glutton.h. */

/// Landing state of the dropped enemy, the one after the descent in
/// `D_actor_444000_80131F1C`: each step rebuild the model's rotation about y
/// from its own yaw and flatten it through a 0x34-byte block borrowed from the
/// scratchpad -- scaled (`0x4000, 0x66, 0x4000`) for the first 0xA steps, then
/// (`0x4C00, 0x199, 0x4C00`). The collision object's radius grows with the step
/// counter over the first steps and is then held at 0x380. After 0xC steps the
/// object is unlinked and the task steps on; either way the work block's
/// coordinate keeps tracking the model.
void gluttonRainSplat(Enemy* enemy, Task* task)
{
    Actor403200DropWork* work;

    work = task->work;
    work->timer++;
    if ((s16)work->timer < 0xA) {
        actorRescaleYawY(task->extra.tmd->coords, 0x4000, 0x66);
        work->obj.radius = (s16)work->timer * 0x40 + 0x100;
    } else {
        actorRescaleYawY(task->extra.tmd->coords, 0x4C00, 0x199);
        work->obj.radius = 0x380;
    }

    Gp_ClearRec18Occupied(&work->rec);
    if ((s16)work->timer >= 0xC) {
        Gp_UnlinkObj(&work->obj);
        task->state++;
    }

    work->coord.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    work->coord.coord.t[1]   = task->extra.tmd->coords->coord.t[1];
    work->coord.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    work->coord.composeStamp = GRAPHICS_COORD_DIRTY;
    Gp_UpdateCoord(&work->coord);
}
