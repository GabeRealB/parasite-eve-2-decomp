#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern TaskDesc   D_dryfield_dilapidated_house_80183EB4;
extern Task*      RoomsShared8017e8a8Task;
extern Task*      D_dryfield_dilapidated_house_80189B7C;
extern s16        D_dryfield_dilapidated_house_80189C98;

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

INCLUDE_ASM("rooms/nonmatchings/dryfield_dilapidated_house/dryfield_dilapidated_house_3", func_dryfield_dilapidated_house_80180738);

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
