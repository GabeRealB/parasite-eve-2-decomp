/* Part of the factory lift library; see factory_lift.h. */

/// Rebuilds four faces of the stage variant's collision grid from a template
/// moved into the frame of the task's model: the template normals are rotated
/// into grid normals 2..5 and its corners rotated and translated into corners
/// 8..15. `useAltTemplate` picks the second template, and `remapFaces` also
/// copies the template's four face records into faces 2..5, rebased onto those
/// slots. The model's set-up state remaps; the per-frame state passes bit 0 of
/// game flag 0x49 as `useAltTemplate`.
void factoryLiftSyncCollision(Task* task, s32 remapFaces, s32 useAltTemplate)
{
    long          flag;
    GfxCoord*     coord;
    MATRIX*       m;
    GpGridParams* geom;
    GpGridParams* src;
    SVECTOR*      s;
    SVECTOR*      d;
    GpGridFace*   sf;
    GpGridFace*   df;
    u16*          sv;
    u16*          dv;
    s32           i;
    s32           j;

    coord = task->extra.tmd->coords;
    if (gGameSession->location.loc.stage == 2) {
        geom = &gFactoryDayGrid;
    } else {
        geom = &gFactoryNightGrid;
    }
    if (useAltTemplate != 0) {
        src = &gFactoryLiftTurnedTemplate;
    } else {
        src = &gFactoryLiftTemplate;
    }

    m = &coord->coord;
    s = src->field_4;
    d = geom->field_4 + 2;
    for (i = 0; i < 4; i++) {
        gte_SetRotMatrix(m);
        gte_ldv0(s);
        s++;
        gte_rtv0();
        gte_stsv(d);
        d++;
    }

    gte_SetRotMatrix(m);
    gte_SetTransMatrix(m);
    s = src->field_8;
    d = geom->field_8 + 8;
    for (i = 0; i < 8; i++) {
        RotTransSV(s++, d++, &flag);
    }

    if (remapFaces != 0) {
        sf = src->field_C;
        df = geom->field_C + 2;
        for (i = 0; i < 4; i++) {
            j  = 0;
            dv = df->verts;
            sv = sf->verts;
            do {
                *dv++ = *sv++ + 8;
            } while (++j < 4);
            df->normalIndex  = sf->normalIndex + 2;
            df->surfaceClass = sf->surfaceClass;
            df++;
            sf++;
        }
    }
}
