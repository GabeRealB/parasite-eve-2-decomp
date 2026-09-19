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

void func_acropolis_cafeteria_80181E3C(Task* arg0);

extern SVECTOR RoomsShared8017d830Delta;

extern MATRIX D_acropolis_cafeteria_8018D5A0;
extern MATRIX D_acropolis_cafeteria_8018D5C0;
extern MATRIX D_acropolis_cafeteria_8018D5E0;
extern MATRIX D_acropolis_cafeteria_8018D600;
extern MATRIX D_acropolis_cafeteria_8018D620;
extern MATRIX D_acropolis_cafeteria_8018D640;
extern MATRIX D_acropolis_cafeteria_8018D660;
extern MATRIX D_acropolis_cafeteria_8018D680;

void func_acropolis_cafeteria_801827C4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;

    obj = (GpItemObj8*)task->spawnArg2;
    tmd = (TmdObject*)task->extra;
    if (Gp_GetCurBit2Flag(obj->field_8) != 2) {
        tmd->lightMtx = &D_acropolis_cafeteria_8018D5C0;
        tmd->colorMtx = &D_acropolis_cafeteria_8018D5A0;
        tmd->flags    = 0;
    } else {
        tmd->flags |= 0x80;
    }
    switch (Gp_GetViewIndex() & 0xFF) {
        case 0xC:
            tmd->otOffset = 7;
            break;
        case 0x18:
            tmd->otOffset = 4;
            break;
        default:
            tmd->otOffset = -2;
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
        tmd->flags = 0x80;
        return;
    }
    if (obj->field_8 == 0xA) {
        Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, 0x400, 1);
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D600;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D5E0;
    if (flag == 2) {
        tmd->flags &= 0xFFF7;
        Task_CallExit(task);
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
void func_acropolis_cafeteria_80182954(Task* task)
{
    TmdObject* tmd;

    tmd = (TmdObject*)task->extra;
    if ((Gp_GetViewIndex() & 0xFF) != 9) {
        tmd->flags = 0x80;
        return;
    }
    tmd->lightMtx = &D_acropolis_cafeteria_8018D640;
    tmd->colorMtx = &D_acropolis_cafeteria_8018D620;
    if (Gp_GetCurBit2Flag(0xA) == 2) {
        tmd->flags |= 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
    Gfx_RotMatrixX(&((TmdObject*)task->extra)->coords->coord, 0x400, 1);
}
void func_acropolis_cafeteria_80182A08(Task* task)
{
    TmdObject* tmd;

    tmd = (TmdObject*)task->extra;
    switch (Gp_GetViewIndex() & 0xFF) {
        case 6:
        case 7:
        case 0xA:
            tmd->flags    = 8;
            tmd->lightMtx = &D_acropolis_cafeteria_8018D680;
            tmd->colorMtx = &D_acropolis_cafeteria_8018D660;
            break;
        default:
            tmd->flags |= 0x80;
            return;
    }
    if (Gp_GetCurBit2Flag(0xB) == 2) {
        tmd->flags |= 0x80;
    } else {
        tmd->flags    = 8;
        tmd->otOffset = 0;
        Tmd_AllocBuffers(tmd);
    }
}
