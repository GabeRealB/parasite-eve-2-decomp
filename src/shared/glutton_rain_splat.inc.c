/* Part of the Glutton library; see glutton.h. */

/// Landing state of the dropped enemy, the one after the descent in
/// `D_actor_444000_80131F1C`: each step rebuild the model's rotation about y
/// from its own yaw and flatten it through a 0x34-byte block borrowed from the
/// scratchpad -- scaled (`0x4000, 0x66, 0x4000`) for the first 0xA steps, then
/// (`0x4C00, 0x199, 0x4C00`). The radius of `attackBody` grows with
/// `stateTicks` over the first steps and is then held at 0x380. After 0xC steps
/// the body is unlinked and the task steps on; either way `bodyCoord` keeps
/// tracking the model.
void gluttonRainSplat(Enemy* enemy, Task* task)
{
    GluttonProjectileWork* work;

    work = task->work;
    work->stateTicks++;
    if (work->stateTicks < 0xA) {
        actorRescaleYawY(task->extra.tmd->coords, 0x4000, 0x66);
        work->attackBody.radius = work->stateTicks * 0x40 + 0x100;
    } else {
        actorRescaleYawY(task->extra.tmd->coords, 0x4C00, 0x199);
        work->attackBody.radius = 0x380;
    }

    worldCollisionClearContacts(work->attackContacts);
    if (work->stateTicks >= 0xC) {
        worldCollisionUnlinkBody(&work->attackBody);
        task->state++;
    }

    work->bodyCoord.node.coord.t[0]   = task->extra.tmd->coords->coord.t[0];
    work->bodyCoord.node.coord.t[1]   = task->extra.tmd->coords->coord.t[1];
    work->bodyCoord.node.coord.t[2]   = task->extra.tmd->coords->coord.t[2];
    work->bodyCoord.node.composeStamp = GRAPHICS_COORD_DIRTY;
    actorRenderComposeCoord(&work->bodyCoord.node);
}
