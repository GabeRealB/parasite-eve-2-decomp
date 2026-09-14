#include "common.h"
#include "gameplay/D4.h"
#include "main/tmd.h"

extern MATRIX* D_80073B8C;

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801339FC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80133B80);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80133DD8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134044);

s32 func_actor_403200_801341E8(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if (view == 0x21) {
                value = 0x20;
                flag  = dist < 0x189D;
                if (flag) {
                    value = 0x21;
                }
                return value;
            }
            value = 0x21;
            flag  = dist < 0x1770;
            if (!flag) {
                value = 0x20;
            }
            return value;
        case 1:
            if ((view != 9) && (view != 10)) {
                value = 9;
                flag  = dist < 0x27D8;
            } else {
                flag = view;
                if (flag == 9) {
                    value = 0xA;
                    flag  = dist < 0x27D9;
                    if (flag) {
                        value = 9;
                    }
                    return value;
                }
                if (flag == 10) {
                    value = 9;
                    flag  = dist < 0x24EA;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xA;
            }
            return value;
        case 2:
            return 0x1B;
    }
    return 1;
}

s32 func_actor_403200_80134374(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
        case 1:
            if ((view != 7) && (view != 8)) {
                value = 7;
                flag  = dist < 0x26AC;
            } else {
                flag = view;
                if (flag == 7) {
                    value = 8;
                    flag  = dist < 0x26AD;
                    if (flag) {
                        value = 7;
                    }
                    return value;
                }
                if (flag == 8) {
                    value = 7;
                    flag  = dist < 0x2328;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 8;
            }
            return value;
        case 2:
            return 0x1A;
    }
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801344C4);

s32 func_actor_403200_80134748(Task* arg0, s16 arg1)
{
    SVECTOR        vec;
    SVECTOR*       vp;
    GsCOORDINATE2* coords;
    s32            dist;
    s32            value;
    s32            view;
    s32            flag;

    view   = Gp_GetViewIndex() & 0xFF;
    vp     = &vec;
    coords = ((TmdObject*)arg0->extra)->field_8;
    vp->vx = D_80073B8C->t[0] - coords->coord.t[0];
    vp->vy = D_80073B8C->t[1] - coords->coord.t[1];
    dist   = vec.vx * vec.vx;
    vp->vz = D_80073B8C->t[2] - coords->coord.t[2];
    dist  += vec.vy * vec.vy;
    dist   = SquareRoot0(dist + (vec.vz * vec.vz));
    switch (arg1) {
        case 0:
            if ((view != 5) && (view != 6)) {
                value = 5;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 5) {
                    value = 6;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 5;
                    }
                    return value;
                }
                if (flag == 6) {
                    value = 5;
                    flag  = dist < 0x2198;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 6;
            }
            return value;
        case 1:
            if ((view != 0xB) && (view != 0xC)) {
                value = 0xB;
                flag  = dist < 0x238C;
            } else {
                flag = view;
                if (flag == 0xB) {
                    value = 0xC;
                    flag  = dist < 0x238D;
                    if (flag) {
                        value = 0xB;
                    }
                    return value;
                }
                if (flag == 0xC) {
                    value = 0xB;
                    flag  = dist < 0x1A90;
                } else {
                    return 1;
                }
            }
            if (!flag) {
                value = 0xC;
            }
            return value;
        case 2:
            return 0x1C;
    }
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134900);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134A14);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80134D40);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_8013509C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801354A4);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135854);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135CB8);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80135F98);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_801364F4);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_8013669C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80136ACC);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80136D94);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_8013709C);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80137600);

INCLUDE_ASM("actors/nonmatchings/actor_403200/actor_403200_3", func_actor_403200_80137788);
