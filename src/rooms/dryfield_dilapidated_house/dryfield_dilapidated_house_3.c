#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern TaskDesc   D_dryfield_dilapidated_house_80183EB4;
extern Task*      RoomsShared8017e8a8Task;
extern Task*      D_dryfield_dilapidated_house_80189B7C;
extern s16        D_dryfield_dilapidated_house_80189C98;
extern s32        D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR    D_dryfield_dilapidated_house_80186844[2];

void func_dryfield_dilapidated_house_8017EAB4(Task* arg0)
{
    arg0->field_24 = D_dryfield_dilapidated_house_80183E8C;
    Game_SetPtrSlot(arg0, 7);
    if (Gp_LookupSlot4(1) != 0) {
        RoomsShared8017e8a8Task =
            Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 0, 0, 0);
    }
    D_dryfield_dilapidated_house_80189C98 = 2;
    D_dryfield_dilapidated_house_80189B7C =
        Task_SpawnFromTable(&D_dryfield_dilapidated_house_80183EB4, 2, 0, 0);
    Game_Session->field_69 = 0x83;
    arg0->state           += 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EB60);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EBB8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017EE58);

/// Evaluates a cubic Bezier segment at frame `pos` of `len`: control points
/// `pts[0..2]` and `p3`, with `t` running from 1 (0xFFFF) down to 0 as `pos`
/// reaches `len`. Writes the X/Y/Z result to `out`.
void func_dryfield_dilapidated_house_8017F418(SVECTOR* pts, SVECTOR* p3, s32 len, s32 pos, s32* out)
{
    SVECTOR  coeff[3];
    SVECTOR* p1;
    SVECTOR* p2;
    s32      t;
    s32      i;
    s32*     o;

    if (len != 0) {
        t  = ((len - pos) * 0xFFFF) / len;
        p1 = &pts[1];
        p2 = &pts[2];
        func_dryfield_dilapidated_house_80181290(pts->vx, p1->vx, p2->vx, p3->vx, &coeff[0]);
        func_dryfield_dilapidated_house_80181290(pts->vy, p1->vy, p2->vy, p3->vy, &coeff[1]);
        func_dryfield_dilapidated_house_80181290(pts->vz, p1->vz, p2->vz, p3->vz, &coeff[2]);
        o = out;
        for (i = 0; i < 3; i++) {
            *o++ = ((((((coeff[i].vx * t) >> 16) + coeff[i].vy) * t >> 16) + coeff[i].vz) * t >> 16) + coeff[i].pad;
        }
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017F568);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_8017FAD4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_801803A4);

void func_dryfield_dilapidated_house_80180738(Task* task, SVECTOR* verts)
{
    DdhAngleStep* work;
    DdhCoordWork* src;
    MATRIX*       mtx;
    SVECTOR*      ofs;
    SVECTOR*      ofs2;
    SVECTOR       pos[2];
    SVECTOR*      v0;
    SVECTOR*      v1;
    s16           tx;
    s16           ty;
    s16           tz;
    s32           i;
    s32           ang;
    s32           c;
    s32           s;

    v0 = verts;
    v1 = &verts[16];

    work = (DdhAngleStep*)task->idMap;
    src  = (DdhCoordWork*)((Task*)task->spawnArg2)->idMap;

    ofs       = D_dryfield_dilapidated_house_80186844;
    ofs2      = D_dryfield_dilapidated_house_80186844 + 1;
    pos[0].vx = ofs->vx;
    pos[0].vy = ofs->vy;
    pos[0].vz = ofs->vz;
    pos[1].vx = ofs2->vx;
    pos[1].vy = ofs2->vy;
    pos[1].vz = ofs2->vz;

    mtx = &src->mtx;

    tx = mtx->t[0];
    ty = mtx->t[1];
    tz = mtx->t[2];

    gte_SetRotMatrix(mtx);

    for (i = 0; i < 16; i++) {
        ang = (i << 12) >> 4;
        c   = rcos(ang);
        s   = rsin(ang);

        v0->vx = pos[0].vx + ((c * 0x96) >> 12);
        v0->vy = pos[0].vy + ((s * 0x4B) >> 12);
        v0->vz = pos[0].vz;

        gte_ldv0(v0);
        gte_mvmva_real();
        gte_stsv(v0);

        v0->vx += tx;
        v0->vy += ty;
        v0->vz += tz;
        v0++;

        v1->vx = pos[1].vx + ((c * 0xFA) >> 12);
        v1->vy = pos[1].vy + ((s * 0x7D) >> 12);
        v1->vz = pos[1].vz + ((rsin(work->step[i] >> 2) * 0x64) >> 12);

        work->step[i] = (work->step[i] + D_dryfield_dilapidated_house_80186804[i]) & 0x3FFF;

        gte_ldv0(v1);
        gte_mvmva_real();
        gte_stsv(v1);

        v1->vx += tx;
        v1->vy += ty;
        v1->vz += tz;
        v1++;
    }
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180A0C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180B84);

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180F04);

void func_dryfield_dilapidated_house_80180F5C(Task* arg0)
{
    DdhCoordWork* work;
    s32           temp_v0;

    work = (DdhCoordWork*)arg0->idMap;
    func_dryfield_dilapidated_house_801810F8((TmdObject*)arg0->extra,
                                             (TmdObject*)((Task*)arg0->spawnArg2)->extra);
    func_dryfield_dilapidated_house_80181028(arg0);
    temp_v0       = func_dryfield_dilapidated_house_80180FD8(arg0);
    work->field_0 = temp_v0;
    work->field_8 = temp_v0;
    work->field_4 = temp_v0;
}
