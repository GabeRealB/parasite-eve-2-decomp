/* Part of the factory lift library; see factory_lift.h. */

/// Rebuilds the lift's reserved collision geometry in the active room grid.
///
/// Requires a live TMD root whose local matrix maps the lift into room space.
/// Daytime Dryfield selects the day grid; every other stage selects night.
/// `useTurnedTemplate` nonzero selects the turned footprint in model space.
/// Both templates supply four normals/faces and eight vertices. The room
/// reserves normals/faces 2..5 and vertices 8..15 for their transformed copies.
/// `remapFaces` nonzero also rebases all four face indices into those pools;
/// later updates retain that topology, shared by both templates. Cell lists
/// and other geometry stay intact. Normal pad words are preserved; the SDK
/// vertex transform overwrites each vertex's pad with the sign extension of Z.
/// No storage is owned here.
static void _factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useTurnedTemplate)
{
    enum { FACTORY_LIFT_COLLISION_FIRST_NORMAL = 2,
           FACTORY_LIFT_COLLISION_FIRST_FACE   = 2,
           FACTORY_LIFT_COLLISION_FACE_COUNT   = 4,
           FACTORY_LIFT_COLLISION_FIRST_VERTEX = 8,
           FACTORY_LIFT_COLLISION_VERTEX_COUNT = 8 };
    long                          gteFlags;
    GfxCoord*                     liftCoord;
    MATRIX*                       modelToRoom;
    WorldCollisionGrid*           roomGrid;
    const WorldCollisionGrid*     templateGrid;
    SVECTOR*                      sourceVector;
    SVECTOR*                      roomVector;
    const WorldCollisionGridFace* sourceFace;
    WorldCollisionGridFace*       destFace;
    const u16*                    sourceIndices;
    u16*                          destIndices;
    s32                           recordIndex;
    s32                           cornerIndex;

    liftCoord = task->extra.tmd->coords;
    if (gGameSession->location.loc.stage == GAME_STAGE_DRYFIELD) {
        roomGrid = &gFactoryDayGrid;
    } else {
        roomGrid = &gFactoryNightGrid;
    }
    if (useTurnedTemplate != 0) {
        templateGrid = &gFactoryLiftTurnedTemplate;
    } else {
        templateGrid = &gFactoryLiftTemplate;
    }

    // Normals rotate only; vertices also receive the lift translation.
    modelToRoom  = &liftCoord->coord;
    sourceVector = templateGrid->normals;
    roomVector   = roomGrid->normals + FACTORY_LIFT_COLLISION_FIRST_NORMAL;
    for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_FACE_COUNT; recordIndex++) {
        gte_SetRotMatrix(modelToRoom);
        gte_ldv0(sourceVector);
        sourceVector++;
        gte_rtv0();
        gte_stsv(roomVector);
        roomVector++;
    }

    gte_SetRotMatrix(modelToRoom);
    gte_SetTransMatrix(modelToRoom);
    sourceVector = templateGrid->vertices;
    roomVector   = roomGrid->vertices + FACTORY_LIFT_COLLISION_FIRST_VERTEX;
    for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_VERTEX_COUNT; recordIndex++) {
        RotTransSV(sourceVector++, roomVector++, &gteFlags);
    }

    // Face topology is installed once, then shared by both moving footprints.
    if (remapFaces != 0) {
        sourceFace = templateGrid->faces;
        destFace   = roomGrid->faces + FACTORY_LIFT_COLLISION_FIRST_FACE;
        for (recordIndex = 0; recordIndex < FACTORY_LIFT_COLLISION_FACE_COUNT; recordIndex++) {
            cornerIndex   = 0;
            destIndices   = destFace->vertexIndices;
            sourceIndices = sourceFace->vertexIndices;
            do {
                *destIndices++ = *sourceIndices++ + FACTORY_LIFT_COLLISION_FIRST_VERTEX;
            } while (++cornerIndex < ARRAY_SIZE(destFace->vertexIndices));
            destFace->normalIndex  = sourceFace->normalIndex + FACTORY_LIFT_COLLISION_FIRST_NORMAL;
            destFace->surfaceClass = sourceFace->surfaceClass;
            destFace++;
            sourceFace++;
        }
    }
}
