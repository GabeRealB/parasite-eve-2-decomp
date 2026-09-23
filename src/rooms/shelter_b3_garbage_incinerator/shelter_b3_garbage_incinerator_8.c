#include "common.h"

#include "gameplay/3A34.h"

extern void func_shelter_b3_garbage_incinerator_80184EEC(void);
extern void func_shelter_b3_garbage_incinerator_80185220(void);

/// Four halfwords per entry, read as the two floor-level end points of a wall
/// edge: `[0]`/`[1]` for the first corner and `[2]`/`[3]` for the second.
extern u16 D_shelter_b3_garbage_incinerator_8018FBFC[][4];

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_8", func_shelter_b3_garbage_incinerator_80184EEC);

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_8", func_shelter_b3_garbage_incinerator_8018507C);

void func_shelter_b3_garbage_incinerator_80185220(void)
{
    SVECTOR     normal;
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    SVECTOR*    np;

    i       = 0;
    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    np      = &normal;
    func_shelter_b3_garbage_incinerator_80184EEC();
    do {
        verts[i * 4].vx = verts[i * 4 + 2].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][0];
        verts[i * 4].vy = verts[i * 4 + 2].vy = 0;
        verts[i * 4].vz = verts[i * 4 + 2].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        verts[i * 4 + 1].vx = verts[i * 4 + 3].vx = D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        verts[i * 4 + 1].vy = verts[i * 4 + 3].vy = 0;
        verts[i * 4 + 1].vz = verts[i * 4 + 3].vz = D_shelter_b3_garbage_incinerator_8018FBFC[i][3];
        verts[i * 4].vy                          -= 400;
        verts[i * 4 + 1].vy                      -= 400;
        faces[i].verts[1]                         = i * 4 + 1;
        faces[i].verts[0]                         = i * 4;
        faces[i].verts[2]                         = i * 4 + 2;
        faces[i].verts[3]                         = i * 4 + 3;
        faces[i].field_8                          = i;
        faces[i].field_A                          = 1;
        normal.vx                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][3] - D_shelter_b3_garbage_incinerator_8018FBFC[i][1];
        normal.vy                                 = 0;
        normal.vz                                 = D_shelter_b3_garbage_incinerator_8018FBFC[i][0] - D_shelter_b3_garbage_incinerator_8018FBFC[i][2];
        VectorNormalSS(np, np);
        normals[i] = normal;
        i++;
    } while (i < 6);
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_garbage_incinerator/shelter_b3_garbage_incinerator_8", func_shelter_b3_garbage_incinerator_801853C4);

void func_shelter_b3_garbage_incinerator_80185574(void)
{
    func_shelter_b3_garbage_incinerator_80185220();
}
