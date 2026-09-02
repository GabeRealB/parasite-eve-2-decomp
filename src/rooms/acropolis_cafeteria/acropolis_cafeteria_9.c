#include "common.h"

#include "gameplay/268.h"
#include "gameplay/4CC.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/gfx.h"
#include "main/task.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/tmd.h"
#include "rooms/acropolis_cafeteria.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

extern GsCOORDINATE2 Gfx_ViewCoord;

void func_acropolis_cafeteria_80181E3C(Task* arg0);

extern SVECTOR D_acropolis_cafeteria_8018D6AC;

extern MATRIX D_acropolis_cafeteria_8018D5A0;
extern MATRIX D_acropolis_cafeteria_8018D5C0;
extern MATRIX D_acropolis_cafeteria_8018D5E0;
extern MATRIX D_acropolis_cafeteria_8018D600;
extern MATRIX D_acropolis_cafeteria_8018D620;
extern MATRIX D_acropolis_cafeteria_8018D640;

void func_acropolis_cafeteria_801818DC(Task* task)
{
    TmdObject*                obj;
    GsCOORDINATE2*            coord;
    AcropolisCafeteriaDebris* work;
    GsCOORDINATE2*            player;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    work  = Mem_Calloc(0xD8, 0);
    if (work == NULL) {
        Task_Kill(task);
        return;
    }
    task->idMap        = (TaskIdMap*)work;
    task->exitCallback = func_acropolis_cafeteria_80181E3C;
    task->state        = task->state + 1;
    Mem_Set(work, 0, 0xD8);
    coord->sub   = &Gfx_ViewCoord;
    coord->flg   = 0;
    obj->field_C = 0;
    RotMatrix(&work->field_C4, &coord->coord);
    work->field_B0     = (rand() & 0xFFF) + 0x3000;
    player             = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    coord->coord.t[0]  = player->coord.t[0];
    coord->coord.t[1]  = player->coord.t[1] - 0x800;
    coord->coord.t[2]  = player->coord.t[2] + 0x800;
    work->obj.field_C  = work->slots;
    work->obj.field_18 = 0x50000;
    work->obj.field_1C = 0xFA;
    work->obj.field_8  = coord;
    work->obj.field_10 = 0;
    work->obj.field_12 = 0;
    work->obj.field_14 = 0;
    work->obj.flags    = 1;
    Gp_LinkObj(4, &work->obj);
    Gp_InitRec18Table(work->obj.field_C, 6, 0);
    work->obj.flags |= 0x8000;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80181A3C);

void func_acropolis_cafeteria_80181E30(Task* arg0)
{
    arg0->state = 3;
}

void func_acropolis_cafeteria_80181E3C(Task* arg0)
{
    Gp_UnlinkObj(arg0->idMap);
    Task_Kill(arg0);
}

extern TaskFuncTable4 D_acropolis_cafeteria_8017D69C;

/// Per-frame entry point: runs the task's current state. The table is a local,
/// so GCC copies it from `.rodata` onto the stack every frame.
void func_acropolis_cafeteria_80181E70(Task* task)
{
    TaskFuncTable4 states;

    states = D_acropolis_cafeteria_8017D69C;
    states.funcs[task->state](task);
}
s32 func_acropolis_cafeteria_80181ED4(GsCOORDINATE2* coord, GpRec18* rec, s16 arg2)
{
    void**                   scratch;
    u8*                      head;
    AcropolisCafeteriaDelta* s;
    register void*           p asm("v1");
    s32                      val;

    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    p           = head - 0x14;
    s           = p;
    *scratch    = p;
    s->field_10 = 0;
    if (func_800E0C10(rec, &s->delta, arg2, NULL) != 0) {
        coord->coord.t[0]                += ((AcropolisCafeteriaDelta*)(head - 0x14))->delta.vx.h.hi;
        coord->coord.t[2]                += s->delta.vz.h.hi;
        D_acropolis_cafeteria_8018D6AC.vx = ((AcropolisCafeteriaDelta*)(head - 0x14))->delta.vx.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vy = s->delta.vy.w >> 16;
        D_acropolis_cafeteria_8018D6AC.vz = s->delta.vz.w >> 16;
        val                               = ((AcropolisCafeteriaDelta*)(head - 0x14))->delta.vx.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[0]++;
                D_acropolis_cafeteria_8018D6AC.vx++;
            } else {
                coord->coord.t[0]--;
                D_acropolis_cafeteria_8018D6AC.vx--;
            }
        }
        val = s->delta.vz.w;
        if ((val & 0xFFFF) != 0) {
            if (val > 0) {
                coord->coord.t[2]++;
                D_acropolis_cafeteria_8018D6AC.vz++;
            } else {
                coord->coord.t[2]--;
                D_acropolis_cafeteria_8018D6AC.vz--;
            }
        }
    }
    if (s->delta.vx.w != 0 || s->delta.vz.w != 0) {
        s->field_10 = 1;
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    return s->field_10;
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182078);

void func_acropolis_cafeteria_801827C4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;

    obj = (GpItemObj8*)task->spawnArg2;
    tmd = (TmdObject*)task->extra;
    if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
        tmd->field_1C = &D_acropolis_cafeteria_8018D5C0;
        tmd->field_20 = &D_acropolis_cafeteria_8018D5A0;
        tmd->field_C  = 0;
    } else {
        tmd->field_C |= 0x80;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 0xC:
            tmd->field_E = 7;
            break;
        case 0x18:
            tmd->field_E = 4;
            break;
        default:
            tmd->field_E = -2;
            break;
    }
}
void func_acropolis_cafeteria_8018286C(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj  = (GpItemObj8*)task->spawnArg2;
    tmd  = (TmdObject*)task->extra;
    flag = Gp_GetCurBit2Flag(obj->field_8);
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->field_C = 0x80;
        return;
    }
    if (obj->field_8 == 0xA) {
        Gfx_RotMatrixX(&((TmdObject*)task->extra)->field_8->coord, 0x400, 1);
    }
    tmd->field_1C = &D_acropolis_cafeteria_8018D600;
    tmd->field_20 = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->field_C &= 0xFFF7;
        Task_CallExit(task);
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
        Tmd_AllocBuffers(tmd);
    }
}
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182954);
INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria_9", func_acropolis_cafeteria_80182A08);
