/* Part of the factory lift library; see factory_lift.h. */

void factoryBarrierCollision(Task* task)
{
    enum { FACTORY_BARRIER_STATE_RESTORE     = 0,
           FACTORY_BARRIER_STATE_WAIT        = 1,
           FACTORY_BARRIER_FACE_COUNT        = 2,
           FACTORY_BARRIER_VERTICES_PER_FACE = 4,
           FACTORY_BARRIER_VERTEX_COUNT      = FACTORY_BARRIER_FACE_COUNT * FACTORY_BARRIER_VERTICES_PER_FACE,
           FACTORY_BARRIER_CLEAR_X_OFFSET    = 2000 };
    WorldCollisionGrid* templateGrid = &gFactoryBarrierTemplate;
    WorldCollisionGrid* roomGrid;
    s32                 recordIndex;

    /// Moves the eight reserved vertices to the cleared barrier position.
    ///
    /// Captures roomGrid and recordIndex; adds the fixed X offset with
    /// halfword truncation. Requires eight writable vertices and a braced call
    /// site. Takes no arguments and is undefined before leaving this function.
#define FACTORY_BARRIER_SHIFT_VERTICES()                                               \
    for (recordIndex = 0; recordIndex < FACTORY_BARRIER_VERTEX_COUNT; recordIndex++) { \
        roomGrid->vertices[recordIndex].vx += FACTORY_BARRIER_CLEAR_X_OFFSET;          \
    }

    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        roomGrid = &gFactoryDayGrid;
    } else {
        roomGrid = &gFactoryNightGrid;
    }
    switch (task->state) {
        case FACTORY_BARRIER_STATE_RESTORE:
            // Restore only the reserved barrier geometry, preserving SVECTOR pad words.
            for (recordIndex = 0; recordIndex < FACTORY_BARRIER_FACE_COUNT; recordIndex++) {
                roomGrid->normals[recordIndex].vx                                          = templateGrid->normals[recordIndex].vx;
                roomGrid->normals[recordIndex].vy                                          = templateGrid->normals[recordIndex].vy;
                roomGrid->normals[recordIndex].vz                                          = templateGrid->normals[recordIndex].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 0].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 1].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 2].vz;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vx = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vx;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vy = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vy;
                roomGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vz = templateGrid->vertices[recordIndex * FACTORY_BARRIER_VERTICES_PER_FACE + 3].vz;
                roomGrid->faces[recordIndex]                                               = templateGrid->faces[recordIndex];
            }
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) != 0) {
                FACTORY_BARRIER_SHIFT_VERTICES();
                taskKill(task);
                return;
            }
            task->state++;
            break;
        case FACTORY_BARRIER_STATE_WAIT:
            if (gameFlagGetNibble(GAME_FLAG_FACTORY_BARRIER_CLEARED) > 0) {
                FACTORY_BARRIER_SHIFT_VERTICES();
                task->state++;
            }
            break;
        default:
            taskKill(task);
            break;
    }
#undef FACTORY_BARRIER_SHIFT_VERTICES
}
