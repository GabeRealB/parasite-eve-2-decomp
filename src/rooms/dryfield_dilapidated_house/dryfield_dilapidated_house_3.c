#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/dryfield_dilapidated_house.h"
#include "main/tmd.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#define gte_mvmva_real() __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")

extern GpMsgEntry D_dryfield_dilapidated_house_80183E8C[];
extern TaskDesc   D_dryfield_dilapidated_house_80183EB4;
extern Task*      RoomsShared8017e8a8Task;
extern Task*      D_dryfield_dilapidated_house_80189B7C;
extern s16        D_dryfield_dilapidated_house_80189C98;
extern s32        D_dryfield_dilapidated_house_80186804[16];
extern SVECTOR    D_dryfield_dilapidated_house_80186844[2];
extern DdhRoomRec D_dryfield_dilapidated_house_8018669C;
extern SVECTOR    D_dryfield_dilapidated_house_801866B4[];
extern TaskDesc   D_dryfield_dilapidated_house_80186854;
extern void       Room_Script16(Task* task);

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

/// Projects the eight local-space markers at
/// `D_dryfield_dilapidated_house_801866B4` through the parent task's
/// `DdhCoordWork` matrix and `Gfx_ViewWorldMtx`, then queues two red
/// `LINE_F2`s as an X at each screen point in `Gpu_CurrentOt[10]`.
void func_dryfield_dilapidated_house_8017EBB8(Task* task)
{
    struct {
        SVECTOR vec;
        s32     sxy;
        s32     dp;
        s32     flag;
        s32     otz;
    } sc;
    MATRIX*  mtx;
    LINE_F2* line;
    u16      sx;
    s32      sy;
    s16      x0;
    s16      y0;
    s16      x1;
    s16      y1;
    s32      i;

    i   = 0;
    mtx = &((DdhCoordWork*)((Task*)task->spawnArg2)->idMap)->mtx;
    do {
        sc.vec.vx = D_dryfield_dilapidated_house_801866B4[i].vx;
        sc.vec.vy = D_dryfield_dilapidated_house_801866B4[i].vy;
        sc.vec.vz = D_dryfield_dilapidated_house_801866B4[i].vz;
        gte_SetRotMatrix(mtx);
        gte_ldv0(&sc.vec);
        gte_mvmva_real();
        gte_stsv(&sc.vec);
        sc.vec.vx = *(u16*)&sc.vec.vx + *(u16*)&mtx->t[0];
        sc.vec.vy = *(u16*)&sc.vec.vy + *(u16*)&mtx->t[1];
        sc.vec.vz = *(u16*)&sc.vec.vz + *(u16*)&mtx->t[2];
        gte_SetRotMatrix(&Gfx_ViewWorldMtx);
        gte_SetTransMatrix(&Gfx_ViewWorldMtx);
        gte_ldv0(&sc.vec);
        gte_rtps_real();
        gte_stsxy(&sc.sxy);
        gte_stdp(&sc.dp);
        gte_stflg(&sc.flag);
        gte_stszotz(&sc.otz);
        line           = (LINE_F2*)Gpu_PrimCursor;
        sx             = sc.sxy;
        sy             = sc.sxy >> 16;
        Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
        x0             = sx - 5;
        y0             = sy - 5;
        x1             = sx + 5;
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        y1       = sy + 5;
        line->x0 = x0;
        line->y0 = y0;
        line->x1 = x1;
        line->y1 = y1;
        addPrim(Gpu_CurrentOt + 10, line);

        line           = (LINE_F2*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(line + 1);
        setLineF2(line);
        setRGB0(line, 0xFF, 0, 0);
        i++;
        line->x0 = x1;
        line->y0 = y0;
        line->x1 = x0;
        line->y1 = y1;
        addPrim(Gpu_CurrentOt + 10, line);
    } while (i < 8);
}

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

void func_dryfield_dilapidated_house_80180B84(Task* task)
{
    Task*          parent;
    TmdObject*     obj;
    TmdObject*     parentObj;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* parentCoord;
    DdhCoordWork*  work;
    DdhRoomRec*    rec;
    TmdSource*     source;
    SVECTOR*       dst;
    SVECTOR*       dst2;
    SVECTOR*       src2;
    SVECTOR*       verts;
    TaskDesc*      table;
    Task*          spawned;
    GsCOORDINATE2* childCoord;
    u16            flags;
    s32            i;

    parent      = (Task*)task->spawnArg2;
    obj         = (TmdObject*)task->extra;
    parentObj   = (TmdObject*)parent->extra;
    coord       = obj->field_8;
    parentCoord = parentObj->field_8;
    work        = (DdhCoordWork*)Mem_Malloc(0x6C, false);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap   = (TaskIdMap*)work;
    work->field_0 = 0;
    flags         = obj->field_C | 0x80;
    obj->field_C  = flags;
    if (!(parentObj->field_C & 0x80)) {
        obj->field_C = flags & 0xFF7F;
    }
    obj->field_E  = 4;
    obj->field_C |= 2;
    parentCoord  += task->spawnArg1;
    coord->flg    = 0;
    coord->sub    = parentCoord;
    obj->field_1C = parentObj->field_1C;
    obj->field_20 = parentObj->field_20;
    Task_Reparent(parent, task);

    rec    = &D_dryfield_dilapidated_house_8018669C;
    source = ((TmdObject*)task->extra)->field_10;
    dst    = rec->field_8;
    dst2   = (SVECTOR*)rec->field_C;
    verts  = (SVECTOR*)source->field_14;
    for (i = 0; i < rec->field_10; i++) {
        dst[i].vx = verts[i].vx;
        dst[i].vy = verts[i].vy;
        dst[i].vz = verts[i].vz;
    }
    if (rec->field_4 != 0) {
        src2 = (SVECTOR*)source->field_18;
        for (i = 0; i < rec->field_12; i++) {
            dst2[i].vx = src2[i].vx;
            dst2[i].vy = src2[i].vy;
            dst2[i].vz = src2[i].vz;
        }
    }

    func_dryfield_dilapidated_house_80180FD8(task);

    table   = &D_dryfield_dilapidated_house_80186854;
    spawned = Task_SpawnFromTable(table, 3, 9, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->field_8;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 3, 0x11, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->field_8;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 0, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->field_8;
        childCoord->coord = work->mtx;
    }
    spawned = Task_SpawnFromTable(table, 2, 1, (s32)task);
    if (spawned != NULL) {
        childCoord        = ((TmdObject*)spawned->extra)->field_8;
        childCoord->coord = work->mtx;
    }

    task->exitCallback = Room_Script16;
    task->state       += 1;
}

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
