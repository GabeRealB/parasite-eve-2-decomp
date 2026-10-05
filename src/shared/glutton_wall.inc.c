/* Part of the Glutton library; see glutton.h. */

/// Rebuild quad `index` of the collision grid as a wall across the front of
/// the task's model: its edge runs 0x1388 either way along the model's x axis
/// at `scale` out along its z axis, and vertices 0 and 1 sit `drop` below 2
/// and 3. The quad's grid normal becomes the model's z axis at unit length,
/// and the face record selects surface class 3 in area 0x27, 2 elsewhere.
void gluttonBuildWall(Task* task, s16 scale, s16 drop, s16 index)
{
    SVECTOR                 dir;
    WorldCollisionGridFace  face;
    SVECTOR*                normal;
    SVECTOR*                verts;
    WorldCollisionGridFace* faces;
    SVECTOR*                d;

    normal = &Gp_GridParams->normals[index];
    verts  = Gp_GridParams->vertices;
    faces  = Gp_GridParams->faces;

    face.vertexIndices[0] = index * 4;
    face.vertexIndices[1] = index * 4 + 1;
    face.vertexIndices[2] = index * 4 + 2;
    face.vertexIndices[3] = index * 4 + 3;
    face.normalIndex      = index;
    face.surfaceClass     = 3;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, normal);
    gfxReadMatrixXAxis(&task->extra.tmd->coords->coord, &dir);
    d = &dir;
    VectorNormalSS(d, d);
    VectorNormalSS(normal, normal);
    gte_lddp(scale);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);
    gte_lddp(0x1388);
    gte_ldsv(d);
    gte_gpf12();
    gte_stsv(d);

    verts[index * 4].vx = verts[index * 4 + 2].vx =
        task->extra.tmd->coords->coord.t[0] + dir.vx + normal->vx;
    verts[index * 4].vy = verts[index * 4 + 2].vy = dir.vy + normal->vy;
    verts[index * 4].vz                           = verts[index * 4 + 2].vz =
        task->extra.tmd->coords->coord.t[2] + dir.vz + normal->vz;

    verts[index * 4 + 1].vx = verts[index * 4 + 3].vx =
        task->extra.tmd->coords->coord.t[0] - dir.vx + normal->vx;
    verts[index * 4 + 1].vy = verts[index * 4 + 3].vy = -dir.vy + normal->vy;
    verts[index * 4 + 1].vz                           = verts[index * 4 + 3].vz =
        task->extra.tmd->coords->coord.t[2] - dir.vz + normal->vz;

    verts[index * 4].vy     -= drop;
    verts[index * 4 + 1].vy -= drop;

    gfxReadMatrixZAxis(&task->extra.tmd->coords->coord, normal);
    VectorNormalSS(normal, normal);
    gte_lddp(0x1000);
    gte_ldsv(normal);
    gte_gpf12();
    gte_stsv(normal);

    if (gGameSession->location.loc.area == 0x27) {
        face.surfaceClass = 3;
    } else {
        face.surfaceClass = 2;
    }

    faces[index] = face;
}
