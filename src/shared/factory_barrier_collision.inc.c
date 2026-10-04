/* Part of the factory lift library; see factory_lift.h. */

/// Restores two faces of the stage variant's collision grid -- normals,
/// corners and face records -- from a template, then slides their eight
/// corners 2000 units along x once game flag 0x47 is set: at once in state 0,
/// which then kills the task, or from state 1 when the flag turns positive
/// later.
void factoryBarrierCollision(Task* task)
{
    WorldCollisionGrid* src = &gFactoryBarrierTemplate;
    WorldCollisionGrid* geom;
    s32                 i;

    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        geom = &gFactoryDayGrid;
    } else {
        geom = &gFactoryNightGrid;
    }
    switch (task->state) {
        case 0:
            for (i = 0; i < 2; i++) {
                geom->normals[i].vx          = src->normals[i].vx;
                geom->normals[i].vy          = src->normals[i].vy;
                geom->normals[i].vz          = src->normals[i].vz;
                geom->vertices[i * 4 + 0].vx = src->vertices[i * 4 + 0].vx;
                geom->vertices[i * 4 + 0].vy = src->vertices[i * 4 + 0].vy;
                geom->vertices[i * 4 + 0].vz = src->vertices[i * 4 + 0].vz;
                geom->vertices[i * 4 + 1].vx = src->vertices[i * 4 + 1].vx;
                geom->vertices[i * 4 + 1].vy = src->vertices[i * 4 + 1].vy;
                geom->vertices[i * 4 + 1].vz = src->vertices[i * 4 + 1].vz;
                geom->vertices[i * 4 + 2].vx = src->vertices[i * 4 + 2].vx;
                geom->vertices[i * 4 + 2].vy = src->vertices[i * 4 + 2].vy;
                geom->vertices[i * 4 + 2].vz = src->vertices[i * 4 + 2].vz;
                geom->vertices[i * 4 + 3].vx = src->vertices[i * 4 + 3].vx;
                geom->vertices[i * 4 + 3].vy = src->vertices[i * 4 + 3].vy;
                geom->vertices[i * 4 + 3].vz = src->vertices[i * 4 + 3].vz;
                geom->faces[i]               = src->faces[i];
            }
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                for (i = 0; i < 8; i++) {
                    geom->vertices[i].vx += 2000;
                }
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case 1:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) > 0) {
                for (i = 0; i < 8; i++) {
                    geom->vertices[i].vx += 2000;
                }
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
}
