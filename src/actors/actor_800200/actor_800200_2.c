#include "common.h"

#include "actors/actor_800200.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/rand.h>

extern void func_80103C74(GsCOORDINATE2*, VECTOR3*, VECTOR3*);

extern s32 func_80103DD4(VECTOR3*, VECTOR3*);

extern s32 func_8010BC70(GsCOORDINATE2*);

extern s32 func_8010BCF4(Task*, VECTOR3*);

extern void func_actor_800200_801653A0(GpActorWork*);

extern GpActorPathStep D_actor_800200_80169FE0[];

extern GpActorPathStep D_actor_800200_8016A020[];

extern GpActorPathStep D_actor_800200_80169FF8[];

extern u8* D_actor_800200_80169FD0[4];

void func_actor_800200_80162750(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    u8*            head;
    VECTOR3*       vec;
    void*          lock;
    u32            state;
    u8*            tbl;
    s32            dist;
    s32            diff;

    coord             = arg0->extra->coords;
    target            = (GsCOORDINATE2*)((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
    head              = *(u8**)0x1F8003FC;
    vec               = (VECTOR3*)(head - 0x10);
    *(u8**)0x1F8003FC = head - 0x10;
    actor             = arg0->actor;
    actor->field_93E += 1;
    d4                = actor->field_910;
    if (Gp_StateF0.field_0 == 1) {
        state            = 0;
        lock             = Gp_FindLockNode(arg0);
        actor->field_90C = lock;
        if (lock != NULL) {
            Gp_GetLockPos(lock, vec);
            func_80103C74(coord, vec, vec);
            dist  = func_80103D8C(((VECTOR3*)(head - 0x10))->vx, vec->vz);
            dist /= 1024;
            if (dist >= 4) {
                dist = 3;
            }
            tbl   = D_actor_800200_80169FD0[dist];
            state = *(u8*)(tbl + (rand() & 0xF));
        }
        switch (state) {
            case 0:
                break;
            case 1:
                Gp_GetLockPos((GpLockPos*)actor->field_90C, (VECTOR3*)&actor->field_20);
                func_actor_800200_80165408(arg0, 6);
                break;
            case 2:
                d4->repeatCount = (rand() & 3) + 1;
                func_actor_800200_80165434(arg0, 1);
                break;
            case 3:
                goto do_65380;
        }
    } else {
        if (func_8010BC70(coord) >= 0x600) {
        do_65380:
            func_actor_800200_80165380(arg0);
        } else {
            diff = func_8010BCF4(arg0, (VECTOR3*)target->coord.t);
            if (diff < 0) {
                diff = -diff;
            }
            if (diff >= 0x200) {
                actor->field_90C = NULL;
                func_actor_800200_801653A0(arg0);
            } else if (actor->field_93E >= ((rand() & 0x7F) + 0x96)) {
                func_actor_800200_801653C0(arg0);
            }
        }
    }
    *(u8**)0x1F8003FC += 0x10;
}

void func_actor_800200_80162990(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor = arg0->actor;
    coord = arg0->extra->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_80169FF8[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_80169FF8[3].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_80169FF8[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_80169FF8[d4->pathStep].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 3) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00 || (d4->pathStep == 2 && Gp_HasCollectedBit(0x114) == 0)) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if ((func_8010BC70(coord) < 0xC01 && d4->pathStep < 2) || (d4->pathStep == state && Gp_HasCollectedBit(0x114) != 0)) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

void func_actor_800200_80162BFC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor = arg0->actor;
    coord = arg0->extra->coords;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A020[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A020[3].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A020[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A020[d4->pathStep].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 3) {
                arrived:
                    d4->pathDone = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 2;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
                return;
            }
            mode = 6;
            if (d4->pathStep == 3) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0xB01 && d4->pathStep > 0) {
                d4->pathStep++;
                actor->field_960 = 1;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_8016545C(arg0, 1);
            }
            return;
    }
}

void func_actor_800200_80162E0C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    s32            delay;

    coord  = arg0->extra->coords;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->coords;
    actor  = arg0->actor;
    d4     = actor->field_910;
    switch (actor->field_960) {
        case 0:
            actor->field_20 = D_actor_800200_80169FE0[d4->pathStep].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_80169FE0[d4->pathStep].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->pathStep == 2) {
                    actor->field_960 = 3;
                    actor->field_95E = 0;
                    actor->field_95C = 7;
                    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
                    func_actor_800200_80165408(arg0, 6);
                    return;
                }
                if (func_8010BC70(coord) >= 0xE00) {
                    actor->field_960 = 1;
                    actor->field_934 = 0;
                    actor->field_90C = 0;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->pathStep++;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            if ((func_8010BC70(coord) < 0xA01) || (coord->coord.t[0] < target->coord.t[0])) {
                d4->pathStep++;
                actor->field_960 = 0;
                return;
            }
            delay            = actor->field_934 - 1;
            actor->field_934 = delay;
            if (delay <= 0) {
                d4->repeatCount  = 1;
                actor->field_934 = rand() & 0x7F;
                func_actor_800200_80165434(arg0, 0);
            }
            return;
        case 3:
            if (actor->field_95E != 0) {
                actor->field_960++;
            }
            return;
        case 4:
            actor->field_95C = 0;
            actor->field_960++;
            Gp_AnimResetChildSlots(arg0, 9);
            return;
        default:
        case 2:
        case 5:
            return;
    }
}
