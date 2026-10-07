/* Part of the follow collision library; see follow_collision.h. */

/// Rebuilds the actor-following obstacle at the start of the room collision grid.
///
/// Restores four quad faces from `gFollowCollisionSource`, rotates their four
/// normals and transforms eight vertices into `gFollowCollisionGrid` using
/// `modelRoot->coord`. That local matrix must map the source geometry into room
/// space; ancestors and the composed view matrix are not applied. Matrix rotation
/// and source normals use 4096 for one unit; vertices and translation use game
/// coordinate units. An optional `roomOffset` is added after transformation in
/// room axes, with each sum narrowed back to a signed halfword.
///
/// The source and destination pools must contain at least four normals, four
/// faces and eight vertices in word-aligned vector pools. Remaining room
/// geometry, grid descriptors and cell lists are preserved. Normal pads are
/// untouched; the SDK vertex transform overwrites each vertex pad with the
/// high halfword of its GTE Z result. Inputs are borrowed for the call and
/// retained nowhere; the source pools and destination pools must be disjoint.
/// Changes GTE rotation, translation and arithmetic state; transform flags are
/// discarded. The root must be non-NULL; a NULL offset applies no extra shift.
static void _followCollisionRebuildObstacle(const GfxCoord* modelRoot, const SVECTOR* roomOffset)
{
    enum {
        FOLLOW_COLLISION_OBSTACLE_FACE_COUNT   = 4,
        FOLLOW_COLLISION_OBSTACLE_VERTEX_COUNT = 8,
    };
    MATRIX                    modelToRoom;
    long                      transformFlags;
    s32                       elementIndex;
    SVECTOR*                  destinationVector;
    SVECTOR*                  sourceVector;
    WorldCollisionGrid*       roomGrid       = &gFollowCollisionGrid;
    const WorldCollisionGrid* obstacleSource = &gFollowCollisionSource;

    /// Restores the obstacle's XYZ components and complete faces in the room grid.
    ///
    /// Captures `roomGrid`, `obstacleSource`, `elementIndex` and the two obstacle
    /// counts above; preserves vector pads and leaves the index at the vertex count.
    /// This compound statement has no arguments and is undefined after its use.
#define FOLLOW_COLLISION_RESTORE_OBSTACLE_GEOMETRY()                                                    \
    {                                                                                                   \
        for (elementIndex = 0; elementIndex < FOLLOW_COLLISION_OBSTACLE_FACE_COUNT; elementIndex++) {   \
            roomGrid->normals[elementIndex].vx = obstacleSource->normals[elementIndex].vx;              \
            roomGrid->normals[elementIndex].vy = obstacleSource->normals[elementIndex].vy;              \
            roomGrid->normals[elementIndex].vz = obstacleSource->normals[elementIndex].vz;              \
            roomGrid->faces[elementIndex]      = obstacleSource->faces[elementIndex];                   \
        }                                                                                               \
        for (elementIndex = 0; elementIndex < FOLLOW_COLLISION_OBSTACLE_VERTEX_COUNT; elementIndex++) { \
            roomGrid->vertices[elementIndex].vx = obstacleSource->vertices[elementIndex].vx;            \
            roomGrid->vertices[elementIndex].vy = obstacleSource->vertices[elementIndex].vy;            \
            roomGrid->vertices[elementIndex].vz = obstacleSource->vertices[elementIndex].vz;            \
        }                                                                                               \
    }

    FOLLOW_COLLISION_RESTORE_OBSTACLE_GEOMETRY();
#undef FOLLOW_COLLISION_RESTORE_OBSTACLE_GEOMETRY

    modelToRoom = modelRoot->coord;

    // Normals carry direction only; translation applies solely to the vertices.
    destinationVector = roomGrid->normals;
    sourceVector      = obstacleSource->normals;
    for (elementIndex = 0; elementIndex < FOLLOW_COLLISION_OBSTACLE_FACE_COUNT; elementIndex++) {
        gte_SetRotMatrix(&modelToRoom);
        gte_ldv0(sourceVector);
        sourceVector++;
        gte_rtv0();
        gte_stsv(destinationVector);
        destinationVector++;
    }

    gte_SetRotMatrix(&modelToRoom);
    gte_SetTransMatrix(&modelToRoom);
    destinationVector = roomGrid->vertices;
    sourceVector      = obstacleSource->vertices;
    if (roomOffset != NULL) {
        for (elementIndex = 0; elementIndex < FOLLOW_COLLISION_OBSTACLE_VERTEX_COUNT; elementIndex++) {
            RotTransSV(sourceVector, destinationVector, &transformFlags);
            sourceVector++;
            destinationVector->vx += roomOffset->vx;
            destinationVector->vy += roomOffset->vy;
            destinationVector->vz += roomOffset->vz;
            destinationVector++;
        }
    } else {
        for (elementIndex = 0; elementIndex < FOLLOW_COLLISION_OBSTACLE_VERTEX_COUNT; elementIndex++) {
            RotTransSV(sourceVector++, destinationVector++, &transformFlags);
        }
    }
}
