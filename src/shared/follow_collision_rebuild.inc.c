/* Part of the follow collision library; see follow_collision.h. */

/// Rebuilds the working collision mesh from its source under `coord`: its four
/// normals are rotated only, its eight vertices rotated and translated and,
/// when `offset` is non-NULL, shifted by it afterwards.
void followCollisionRebuild(GfxCoord* coord, SVECTOR* offset)
{
    MATRIX              m;
    long                flag;
    s32                 i;
    SVECTOR*            d;
    SVECTOR*            s;
    WorldCollisionGrid* dst = &gFollowCollisionGrid;
    WorldCollisionGrid* src = &gFollowCollisionSource;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx = src->normals[i].vx;
        dst->normals[i].vy = src->normals[i].vy;
        dst->normals[i].vz = src->normals[i].vz;
        dst->faces[i]      = src->faces[i];
    }

    for (i = 0; i < 8; i++) {
        dst->vertices[i].vx = src->vertices[i].vx;
        dst->vertices[i].vy = src->vertices[i].vy;
        dst->vertices[i].vz = src->vertices[i].vz;
    }

    m = coord->coord;

    d = dst->normals;
    s = src->normals;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(&m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(&m);
    gte_SetTransMatrix(&m);
    d = dst->vertices;
    s = src->vertices;
    if (offset != NULL) {
        for (i = 0; i < 8; i++) {
            RotTransSV(s, d, &flag);
            s++;
            d->vx += offset->vx;
            d->vy += offset->vy;
            d->vz += offset->vz;
            d++;
        }
    } else {
        for (i = 0; i < 8; i++) {
            RotTransSV(s++, d++, &flag);
        }
    }
}
