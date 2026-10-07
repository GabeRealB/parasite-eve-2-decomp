/* Part of the Glutton library; see glutton.h. */

/// Rebuilds one live collision-grid quad across the front of the host.
///
/// `distance` and `drop` use world units along normalized host axes. The wall
/// reaches 5000 units either way along x; vertices 0 and 1 lie `drop` units
/// above 2 and 3. Its face normal is the host's forward axis at unit Q12 length.
/// The active grid must provide face and normal `faceIndex` and vertices
/// `4 * faceIndex` through `4 * faceIndex + 3`; callers use face 6. Surface class
/// is 3 in the dumping-hole area, 2 elsewhere. Requires the host's live TMD
/// coordinate and a writable grid. No pointer is retained.
static void _gluttonBuildWall(Task* task, s16 distance, s16 drop, s16 faceIndex)
{
    enum { GLUTTON_WALL_HALF_WIDTH           = 5000,
           GLUTTON_WALL_DUMPING_HOLE_SURFACE = 3,
           GLUTTON_WALL_DEFAULT_SURFACE      = 2 };
    SVECTOR                 halfWidthOffset;
    WorldCollisionGridFace  face;
    SVECTOR*                wallNormal;
    SVECTOR*                vertices;
    WorldCollisionGridFace* faces;
    SVECTOR*                halfWidthPointer;

    wallNormal = &Gp_GridParams->normals[faceIndex];
    vertices   = Gp_GridParams->vertices;
    faces      = Gp_GridParams->faces;

    face.vertexIndices[0] = faceIndex * 4;
    face.vertexIndices[1] = faceIndex * 4 + 1;
    face.vertexIndices[2] = faceIndex * 4 + 2;
    face.vertexIndices[3] = faceIndex * 4 + 3;
    face.normalIndex      = faceIndex;
    face.surfaceClass     = GLUTTON_WALL_DUMPING_HOLE_SURFACE;

    // Scale the facing axes into the wall's offset and half width.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, wallNormal);
    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &halfWidthOffset);
    halfWidthPointer = &halfWidthOffset;
    VectorNormalSS(halfWidthPointer, halfWidthPointer);
    VectorNormalSS(wallNormal, wallNormal);
    gte_lddp(distance);
    gte_ldsv(wallNormal);
    gte_gpf12();
    gte_stsv(wallNormal);
    gte_lddp(GLUTTON_WALL_HALF_WIDTH);
    gte_ldsv(halfWidthPointer);
    gte_gpf12();
    gte_stsv(halfWidthPointer);

    vertices[faceIndex * 4].vx = vertices[faceIndex * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + halfWidthOffset.vx + wallNormal->vx;
    vertices[faceIndex * 4].vy = vertices[faceIndex * 4 + 2].vy = halfWidthOffset.vy + wallNormal->vy;
    vertices[faceIndex * 4].vz                                  = vertices[faceIndex * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + halfWidthOffset.vz + wallNormal->vz;

    vertices[faceIndex * 4 + 1].vx = vertices[faceIndex * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] - halfWidthOffset.vx + wallNormal->vx;
    vertices[faceIndex * 4 + 1].vy = vertices[faceIndex * 4 + 3].vy = -halfWidthOffset.vy + wallNormal->vy;
    vertices[faceIndex * 4 + 1].vz                                  = vertices[faceIndex * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] - halfWidthOffset.vz + wallNormal->vz;

    vertices[faceIndex * 4].vy     -= drop;
    vertices[faceIndex * 4 + 1].vy -= drop;

    // Restore the forward axis as a unit Q12 face normal.
    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, wallNormal);
    VectorNormalSS(wallNormal, wallNormal);
    gte_lddp(ONE);
    gte_ldsv(wallNormal);
    gte_gpf12();
    gte_stsv(wallNormal);

    if (gGameSession->location.loc.area == GAME_AREA_SHELTER_B3_DUMPING_HOLE) {
        face.surfaceClass = GLUTTON_WALL_DUMPING_HOLE_SURFACE;
    } else {
        face.surfaceClass = GLUTTON_WALL_DEFAULT_SURFACE;
    }

    faces[faceIndex] = face;
}
