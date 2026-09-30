/* Part of the factory lift library; see factory_lift.h. */

/// Restores two faces of the stage variant's collision grid -- normals,
/// corners and face records -- from a template, then slides their eight
/// corners 2000 units along x once game flag 0x47 is set: at once in state 0,
/// which then kills the task, or from state 1 when the flag turns positive
/// later.
void factoryBarrierCollision(Task* task)
{
    GpGridParams* src = &gFactoryBarrierTemplate;
    GpGridParams* geom;
    s32           i;

    if (gGameSession->location.loc.stage == 2) {
        geom = &gFactoryDayGrid;
    } else {
        geom = &gFactoryNightGrid;
    }
    switch (task->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                geom->field_4[i].vx         = src->field_4[i].vx;
                geom->field_4[i].vy         = src->field_4[i].vy;
                geom->field_4[i].vz         = src->field_4[i].vz;
                geom->field_8[i * 4 + 0].vx = src->field_8[i * 4 + 0].vx;
                geom->field_8[i * 4 + 0].vy = src->field_8[i * 4 + 0].vy;
                geom->field_8[i * 4 + 0].vz = src->field_8[i * 4 + 0].vz;
                geom->field_8[i * 4 + 1].vx = src->field_8[i * 4 + 1].vx;
                geom->field_8[i * 4 + 1].vy = src->field_8[i * 4 + 1].vy;
                geom->field_8[i * 4 + 1].vz = src->field_8[i * 4 + 1].vz;
                geom->field_8[i * 4 + 2].vx = src->field_8[i * 4 + 2].vx;
                geom->field_8[i * 4 + 2].vy = src->field_8[i * 4 + 2].vy;
                geom->field_8[i * 4 + 2].vz = src->field_8[i * 4 + 2].vz;
                geom->field_8[i * 4 + 3].vx = src->field_8[i * 4 + 3].vx;
                geom->field_8[i * 4 + 3].vy = src->field_8[i * 4 + 3].vy;
                geom->field_8[i * 4 + 3].vz = src->field_8[i * 4 + 3].vz;
                geom->field_C[i]            = src->field_C[i];
            }
            if (GameFlag_GetNibble(0x47) != 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case 1:
            if (GameFlag_GetNibble(0x47) > 0) {
                for (i = 0; i < 8; i++) {
                    geom->field_8[i].vx += 2000;
                }
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
}
