#include "common.h"

#include "actors/actor_800200.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include <psyq/abs.h>

#include <psyq/rand.h>

extern void func_8010ABD4();

extern s32 func_80103DD4(VECTOR3*, VECTOR3*);

extern s32 func_8010BC70(GsCOORDINATE2*);

extern void func_8010BD88(GpActorWork*, VECTOR3*);

extern s32 func_8010BCF4(Task*, VECTOR3*);

extern void func_8010BE5C(GpActorWork*, VECTOR3*);

extern GpActorFuncTable4 D_actor_800200_80161EB8;

extern GpActorFuncTable12 D_actor_800200_80161E5C;

extern TaskFuncTable11 D_actor_800200_80161E8C;

extern GpActorFuncTable3 D_actor_800200_80161E34;

extern GpActorFuncTable9 D_actor_800200_80161EC8;

extern GpActorPathStep D_actor_800200_8016A018[];

extern GpActorPathStep D_actor_800200_8016A108[];

extern GpActorPathStep D_actor_800200_8016A128[];

extern GpActorPathStep D_actor_800200_8016A040[];

extern GpActorPathStep D_actor_800200_8016A048[];

extern GpActorPathStep D_actor_800200_8016A058[];

extern GpActorPathStep D_actor_800200_8016A068[];

extern GpActorPathStep D_actor_800200_8016A080[];

extern GpActorPathStep D_actor_800200_8016A090[];

extern GpActorPathStep D_actor_800200_8016A098[];

extern GpActorPathStep D_actor_800200_8016A0B0[];

extern GpActorPathStep D_actor_800200_8016A0E0[];

extern GpActorPathStep D_actor_800200_8016A130[];

void func_actor_800200_80163044(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A048[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A048[1].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A048[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A048[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 1) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

void func_actor_800200_80163180(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            delay;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A058[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A058[1].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A058[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A058[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 1) {
                arrived:
                    d4->field_D0 = 1;
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
                d4->field_CE++;
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x801) {
                d4->field_CE++;
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

void func_actor_800200_8016337C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A068[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A068[2].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A068[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A068[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 2) {
                arrived:
                    d4->field_D0 = 1;
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
                d4->field_CE++;
                return;
            }
            mode = 6;
            if (d4->field_CE == 2) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 2:
            if (func_8010BC70(coord) < 0x901) {
                d4->field_CE++;
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

void func_actor_800200_80163584(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            mode;
    s32            delay;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_960 = 1;
            actor->field_20  = D_actor_800200_8016A080[1].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A080[1].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_960++;
            func_actor_800200_80165534(arg0);
            return;
        case 2:
            actor->field_20 = D_actor_800200_8016A080[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A080[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 1) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                if (func_8010BC70(coord) >= 0xC00) {
                    actor->field_960 = 3;
                    actor->field_934 = 0;
                    actor->field_90C = NULL;
                    func_actor_800200_801653A0(arg0);
                    return;
                }
                d4->field_CE++;
                return;
            }
            mode = 6;
            if (d4->field_CE == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        case 3:
            if (func_8010BC70(coord) < 0x901) {
                d4->field_CE++;
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

void func_actor_800200_801637B4(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;
    s32            mode;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A098[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A098[2].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A098[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A098[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 2) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (d4->field_CE == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

void func_actor_800200_8016390C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0B0[2].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0B0[2].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A0B0[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0B0[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 2) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_80163A54);

void func_actor_800200_80163B90(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A0E0[4].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A0E0[4].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
            func_actor_800200_80165534(arg0);
            return;
        case 1:
            actor->field_20 = D_actor_800200_8016A0E0[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A0E0[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 4) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

void func_actor_800200_80163CCC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A108[3].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A108[3].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A108[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A108[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 3) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_801654EC(arg0, 0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        default:
            return;
    }
}

void func_actor_800200_80163E14(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u16            state;
    s32            flag;
    s32            mode;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            flag             = 1;
            actor->field_960 = flag;
            actor->field_20  = D_actor_800200_8016A130[4].field_0;
            actor->field_24  = coord->coord.t[1];
            actor->field_28  = D_actor_800200_8016A130[4].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                goto arrived;
            }
        case 1:
            actor->field_20 = D_actor_800200_8016A130[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A130[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x201) {
                if (d4->field_CE == 4) {
                arrived:
                    d4->field_D0 = 1;
                    func_actor_800200_80165534(arg0);
                    return;
                }
                d4->field_CE++;
                func_actor_800200_80165534(arg0);
                return;
            }
            mode = 6;
            if (d4->field_CE == 1) {
                mode = 5;
            }
            func_actor_800200_80165408(arg0, mode);
            return;
        default:
            return;
    }
}

void func_actor_800200_80163F5C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;
    VECTOR3*       vec;
    GameActor*     hit;
    s32            mode;
    s32            dist;
    s32            angle;

    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    actor  = arg0->actor;
    dist   = func_actor_800200_801660E8(coord, &actor->field_910->field_A0, NULL);
    if (dist != 0 && dist < 0x301) {
        hit            = arg0->actor;
        d4             = hit->field_910;
        hit->field_956 = 0xA;
        hit->field_95A = 2;
        hit->field_954 = 0;
        hit->field_95C = 0;
        hit->field_95E = 0;
        hit->field_973 = 0;
        hit->field_975 = 0;
        d4->field_CA   = -1;
        d4->field_C6   = 0;
        Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
        return;
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_934 = 0;
            if (func_8010BC70(coord) >= 0xE00) {
                mode             = 4;
                actor->field_95E = 2;
                actor->field_958 = 6;
            } else {
            resume:
                if (actor->field_95E != 3) {
                    actor->field_95E = 1;
                }
                actor->field_958 = 5;
                mode             = 2;
            }
            actor->field_973 = 1;
            Gp_AnimPlayChildSlotsEx(arg0, mode, 0, 5);
        case 1:
        case 2:
        case 3:
            dist = func_8010BC70(coord);
            if (dist < 0x301) {
                Gp_ResetActorMove(arg0, 0);
                break;
            }
            if (actor->field_95E == 3) {
                break;
            }
            actor->field_934++;
            if (actor->field_934 == 0xF0) {
                actor->field_95E = 3;
                goto resume;
            }
            angle = rand() & 0x3FF;
            if ((0x800 - angle) < dist) {
                goto in_range;
            }
            if (actor->field_95E == 2) {
                goto reset;
            }
        in_range:
            if (dist < angle + 0xC00) {
                break;
            }
            if (actor->field_95E != 1) {
                break;
            }
        reset:
            actor->field_95E = 0;
            break;
        default:
            break;
    }
    vec = (VECTOR3*)&target->coord.t[0];
    func_8010BD88(arg0, vec);
    func_8010BE5C(arg0, vec);
}

void func_actor_800200_80164180(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* target;
    GpLinkNode*    node;
    u8*            head;
    u8*            tmp;
    VECTOR3*       vec;
    GameActor*     actor2;
    s32            dist;
    s32            anim;
    u16            flag;

    actor             = arg0->actor;
    d4                = actor->field_910;
    target            = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    head              = *(u8**)0x1F8003FC;
    tmp               = head - 0x10;
    *(u8**)0x1F8003FC = tmp;
    vec               = (VECTOR3*)tmp;
    node              = actor->field_90C;
    if (node != NULL) {
        if (!(node->field_4 & 1)) {
            Gp_GetLockPos((GpLockPos*)node, vec);
        } else {
            actor->field_95E = 2;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    switch (actor->field_95E) {
        case 0:
            actor->field_95E = 1;
            actor->field_958 = 5;
            actor->field_973 = 1;
            if (func_8010BCF4((Task*)arg0, vec) < 0) {
                actor->field_975 = -1;
                anim             = 5;
            } else {
                actor->field_975 = 1;
                anim             = 6;
            }
            Gp_AnimPlayChildSlotsEx(arg0, anim, 1, 5);
        case 1:
        case 2:
            dist = func_8010BCF4((Task*)arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist < 0x101) || (actor->field_95E == 2)) {
                if ((s8)d4->field_CC > 0) {
                    flag              = actor->field_90C != 0;
                    actor2            = arg0->actor;
                    actor2->field_954 = 0;
                    actor2->field_956 = 4;
                    actor2->field_958 = 0;
                    actor2->field_95A = 0;
                    actor2->field_95C = 0;
                    actor2->field_95E = 0;
                    actor2->field_940 = flag;
                } else {
                    Gp_ResetActorMove(arg0, 0);
                }
            }
            break;
        default:
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)&target->coord.t[0]);
    *(u32*)0x1F8003FC += 0x10;
}

void func_actor_800200_8016436C(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* target;
    GpLinkNode*    node;
    u8*            tmp;
    GsCOORDINATE2* coord;
    VECTOR3*       vec;
    u8*            head;
    s32*           scratch;
    s8             count;
    s32            pan;
    s32            dist;
    u16            state;
    s32            next = 1;
    GameActor*     actor2;

    actor             = arg0->actor;
    d4                = actor->field_910;
    target            = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    head              = *(u8**)0x1F8003FC;
    tmp               = head - 0x10;
    *(u8**)0x1F8003FC = tmp;
    vec               = (VECTOR3*)tmp;
    coord             = arg0->extra->field_8;
    if (actor->field_90C != NULL) {
        node             = Gp_FindLockNode(arg0);
        actor->field_90C = node;
        if ((node != NULL) && !(node->field_4 & 1)) {
            Gp_GetLockPos((GpLockPos*)node, vec);
        } else {
            d4->field_CC = 1;
        }
    } else {
        ((VECTOR3*)(head - 0x10))->vx = target->coord.t[0];
        vec->vy                       = target->coord.t[1];
        vec->vz                       = target->coord.t[2];
    }
    state = actor->field_95E;
    if (state != 0) {
        if (state != 1) {
            scratch = (s32*)0x1F8003FC;
        } else {
            goto tick;
        }
    } else {
        actor->field_95E = next;
        Gp_AnimPlayChildSlotsEx(arg0, actor->field_940 + 0xA, 0, 4);
        pan = (s8)Gp_GetObjPan((GpObj38*)coord);
        SndEvt_EnqueueType6(actor->field_940 + 0x40720009, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
    tick:
        if (func_80105894(arg0, 1, 0, 0) == 0) {
            dist = func_8010BCF4((Task*)arg0, vec);
            if (dist < 0) {
                dist = -dist;
            }
            if ((dist >= 0x281) && (func_80103DD4((VECTOR3*)coord->coord.t, vec) >= 0x201)) {
                actor2            = arg0->actor;
                actor2->field_954 = 0;
                actor2->field_956 = 2;
                actor2->field_95A = 2;
                actor2->field_95C = 0;
                actor2->field_95E = 0;
            } else {
                count        = d4->field_CC - 1;
                d4->field_CC = count;
                if (count <= 0) {
                    Gp_ResetActorMove(arg0, 0);
                } else {
                    actor->field_95E = 0;
                }
            }
        }
        scratch = (s32*)0x1F8003FC;
    }
    *scratch += 0x10;
}

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_80164598);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_801647A8);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_801649D8);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_80164C54);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_80164EBC);

INCLUDE_RODATA("actors/nonmatchings/actor_800200/actor_800200_3", D_actor_800200_80161E5C);

INCLUDE_RODATA("actors/nonmatchings/actor_800200/actor_800200_3", D_actor_800200_80161E8C);

INCLUDE_RODATA("actors/nonmatchings/actor_800200/actor_800200_3", D_actor_800200_80161EB8);

INCLUDE_RODATA("actors/nonmatchings/actor_800200/actor_800200_3", D_actor_800200_80161EC8);

INCLUDE_ASM("actors/nonmatchings/actor_800200/actor_800200_3", func_actor_800200_80165104);

void func_actor_800200_801652EC(GpActorWork* arg0)
{
    GameActor*        actor;
    GpActorFuncTable3 sp;

    sp    = D_actor_800200_80161E34;
    actor = arg0->actor;
    if ((s8)actor->field_97A > 0) {
        actor->field_97A--;
    }
    sp.funcs[actor->field_954](arg0);
    func_actor_800200_80165104(arg0);
    actor->field_986 = 0;
}

void func_actor_800200_80165380(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 1;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

void func_actor_800200_801653A0(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 2;
    actor->field_95A = 2;
    actor->field_95C = 0;
    actor->field_95E = 0;
}

void func_actor_800200_801653C0(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 7;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 7, 0, 3);
}

void func_actor_800200_80165408(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_956 = 8;
    actor->field_954 = 0;
    actor->field_958 = 5;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_934 = arg1;
}

void func_actor_800200_80165434(GpActorWork* arg0, s16 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 4;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    actor->field_940 = arg1;
}

void func_actor_800200_8016545C(GpActorWork* arg0, s8 arg1)
{
    GameActor* actor = arg0->actor;
    GameActor* actor2;
    u16        flag;

    actor->field_910->field_CC = arg1;
    if (Gp_StateF0.field_0 == 1) {
        actor->field_90C = Gp_FindLockNode(arg0);
    } else {
        actor->field_90C = 0;
    }
    flag              = actor->field_90C != 0;
    actor2            = arg0->actor;
    actor2->field_954 = 0;
    actor2->field_956 = 4;
    actor2->field_958 = 0;
    actor2->field_95A = 0;
    actor2->field_95C = 0;
    actor2->field_95E = 0;
    actor2->field_940 = flag;
}

void func_actor_800200_801654EC(GpActorWork* arg0, s32 arg1)
{
    GameActor* actor = arg0->actor;

    actor->field_954 = 0;
    actor->field_956 = 9;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 0;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 1, 0, 3);
}

void func_actor_800200_80165534(GpActorWork* arg0)
{
    GameActor* actor = arg0->actor;

    actor->field_956 = 0xB;
    actor->field_954 = 0;
    actor->field_958 = 0;
    actor->field_95A = 0;
    actor->field_95C = 7;
    actor->field_95E = 0;
    Gp_AnimPlayChildSlotsEx(arg0, 0xE, 0, 3);
}

void func_actor_800200_80165580(GpActorWork* arg0)
{
    u8 temp_v1;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = Game_Session->field_6;
    switch (temp_v1) {
        case 26:
            func_actor_800200_80162990();
            return;
        case 24:
            func_actor_800200_80165814();
            return;
        case 23:
            func_actor_800200_80162BFC(arg0);
            return;
        case 25:
            func_actor_800200_801658E0();
            return;
    }
}

void func_actor_800200_80165644(GpActorWork* arg0)
{
    u8 temp_v1;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v1 = Game_Session->field_6;
    switch (temp_v1) {
        case 25:
            func_actor_800200_8016599C();
            return;
        case 23:
            func_actor_800200_80163044(arg0);
            return;
        case 22:
            func_actor_800200_80163180(arg0);
            return;
        case 20:
            func_actor_800200_8016337C(arg0);
            return;
    }
}

void func_actor_800200_80165708(GpActorWork* arg0)
{
    u8 temp_v0;

    if (arg0->actor->field_910->field_D0 == 1) {
        func_actor_800200_801654EC(arg0, 0);
        return;
    }
    temp_v0 = Game_Session->field_6;
    switch (temp_v0) {
        case 1:
            func_actor_800200_80163A54();
            return;
        case 2:
            func_actor_800200_801637B4(arg0);
            return;
        case 3:
            func_actor_800200_801659CC();
            return;
        case 4:
            func_actor_800200_80163584(arg0);
            return;
        case 5:
            func_actor_800200_8016390C(arg0);
            return;
        case 15:
            func_actor_800200_80163E14(arg0);
            return;
        case 19:
            func_actor_800200_80163CCC(arg0);
            return;
        case 20:
            func_actor_800200_80163B90(arg0);
            return;
        case 24:
            func_actor_800200_80165ACC();
            return;
    }
}

void func_actor_800200_80165814(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    s32            arg;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A018[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A018[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        arg = 6;
        if (d4->field_CE == 2) {
            arg = 5;
        }
        func_actor_800200_80165408(arg0, arg);
    }
}

void func_actor_800200_801658E0(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A040[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A040[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_801654EC(arg0, 0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

void func_actor_800200_8016599C(GpActorWork* arg0)
{
    arg0->actor->field_910->field_D0 = 1;
    func_actor_800200_801654EC(arg0, 0);
}

void func_actor_800200_801659CC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;
    u32            state;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    state = actor->field_960;
    d4    = actor->field_910;
    switch (state) {
        case 0:
            actor->field_20 = D_actor_800200_8016A090[d4->field_CE].field_0;
            actor->field_24 = coord->coord.t[1];
            actor->field_28 = D_actor_800200_8016A090[d4->field_CE].field_4;
            if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
                actor->field_960++;
                if (d4->field_D0 != 1) {
                    func_actor_800200_80165534(arg0);
                }
                return;
            }
            func_actor_800200_80165408(arg0, 6);
            return;
        case 1:
            d4->field_D0 = state;
            func_actor_800200_801654EC(arg0, 0);
            break;
    }
}

void func_actor_800200_80165ACC(GpActorWork* arg0)
{
    GameActor*     actor;
    GpActorD4*     d4;
    GsCOORDINATE2* coord;

    actor = arg0->actor;
    coord = arg0->extra->field_8;
    d4    = actor->field_910;
    if (actor->field_960 == 0) {
        actor->field_20 = D_actor_800200_8016A128[d4->field_CE].field_0;
        actor->field_24 = coord->coord.t[1];
        actor->field_28 = D_actor_800200_8016A128[d4->field_CE].field_4;
        if (func_80103DD4((VECTOR3*)coord->coord.t, (VECTOR3*)&actor->field_20) < 0x401) {
            d4->field_D0 = 1;
            func_actor_800200_80165534(arg0);
            return;
        }
        func_actor_800200_80165408(arg0, 6);
    }
}

void func_actor_800200_80165B84(GpActorWork* arg0)
{
    GameActor*         actor;
    GpActorD4*         d4;
    GsCOORDINATE2*     coord;
    GpActorFuncTable12 sp;
    s32                pan;

    sp    = D_actor_800200_80161E5C;
    actor = arg0->actor;
    d4    = actor->field_910;
    coord = arg0->extra->field_8;
    if (d4->field_C4 > 0) {
        d4->field_C4--;
    }
    sp.funcs[actor->field_956](arg0);
    if ((s8)actor->field_97A == 0) {
        func_80109BB4(arg0, actor->field_17C);
        if ((u16)actor->field_96C != 0) {
            func_8010B9A4(arg0);
            pan = (s8)Gp_GetObjPan((GpObj38*)coord);
            SndEvt_EnqueueType6(0x4072000A, pan, (s8)Gp_GetObjDepth((GpObj38*)coord));
        }
    }
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800200_80165CB4(Task* arg0)
{
    TaskFuncTable11 sp;

    sp = D_actor_800200_80161E8C;
    sp.funcs[arg0->spawnArg1 & 0xF](arg0);
}

void func_actor_800200_80165D44(GpActorWork* arg0)
{
    GameActor*     actor;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* target;

    coord  = arg0->extra->field_8;
    target = ((TmdObject*)((Task*)Game_GetPtrSlot(3))->extra)->field_8;
    actor  = arg0->actor;
    switch (actor->field_95E) {
        case 1:
            actor->field_95C  = 0;
            actor->field_95E += 1;
            Gp_AnimResetChildSlots(arg0, 9);
        case 2:
            if ((func_8010BC70(coord) >= 0x500) || (Gp_StateF0.field_0 == 1)) {
                actor->field_95C  = 7;
                actor->field_95E += 1;
                Gp_AnimPlayChildSlotsEx(arg0, 8, 0, 3);
            }
            break;
        case 4:
            Gp_ResetActorMove(arg0, 0);
            break;
        default:
        case 0:
        case 3:
            break;
    }
    func_8010BE5C(arg0, (VECTOR3*)target->coord.t);
}

void func_actor_800200_80165E50(GpActorWork* arg0)
{
    u16 state = arg0->actor->field_95E;

    if (state != 0) {
        if (state == 1) {
            Gp_ResetActorMove(arg0, 0);
        }
    }
}

void func_actor_800200_80165E90(GpActorWork* arg0)
{
    GpActorFuncTable4 sp;
    GameActor*        actor;

    sp    = D_actor_800200_80161EB8;
    actor = arg0->actor;
    Gp_TickActorAnimState(arg0);
    Gp_AnimTickChildSlots(arg0);
    sp.funcs[(u16)actor->field_96C](arg0);
    Gp_TurnPlayer(arg0);
    Gp_StepPlayerMove(arg0);
}

void func_actor_800200_80165F28(void)
{
    func_8010ABD4();
}

void func_actor_800200_80165F48(void)
{
}

void func_actor_800200_80165F50(GpActorWork* arg0)
{
    GpActorFuncTable9 sp;
    GameActor*        actor;
    GsCOORDINATE2*    coord;

    sp    = D_actor_800200_80161EC8;
    actor = arg0->actor;
    coord = arg0->extra->field_8;
    sp.funcs[actor->field_956](arg0);
    RotMatrix((SVECTOR*)&actor->field_50, &coord->coord);
}

void func_actor_800200_80165FF0(GpActorWork* arg0)
{
    GameActor* actor;
    s16        cur;
    s16        tgt;
    u16        raw;
    s32        temp;
    s32        wrap;
    s32        delta;
    s32        flag;

    actor = arg0->actor;
    cur   = actor->field_52;
    tgt   = actor->field_82;
    raw   = actor->field_82;
    temp  = cur - tgt;
    if (temp < 0) {
        temp = -temp;
    }
    if (temp < 0x31 || (wrap = tgt - 0x1000, temp = cur - wrap, temp = ABS(temp), temp < 0x31)) {
        flag             = 1;
        actor->field_52  = raw;
        actor->field_982 = 0;
        actor->field_956 = flag;
        Gp_AnimPlayChildSlotsEx(arg0, flag, 0, 5);
    } else {
        delta = func_80103E7C(cur, tgt);
        if (delta > 0x30) {
            delta = 0x30;
        } else if (delta < -0x30) {
            delta = -0x30;
        }
        actor->field_958 = 5;
        actor->field_973 = 1;
        actor->field_52  = ((u16)actor->field_52 + delta) & 0xFFF;
    }
    Gp_AnimTickChildSlots(arg0);
}

s32 func_actor_800200_801660E8(GsCOORDINATE2* arg0, GpRec18* arg1, GpRec18* arg2)
{
    s32 dist;

    if (arg1->field_4 != 0) {
        dist = func_80103D8C(arg0->workm.t[0] - arg1->field_8, arg0->workm.t[2] - arg1->field_C);
        if (arg2 != NULL) {
            arg2->field_0 = arg1->field_8;
            arg2->field_2 = arg1->field_A;
            arg2->field_0 = arg1->field_C;
        }
    } else {
        dist = 0;
    }
    return dist;
}
