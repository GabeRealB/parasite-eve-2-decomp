#include "common.h"
#include "actors/actor_101500.h"
#include "actors/actors_shared_80132fd0.h"
#include "main/wipsys.h"

extern MATRIX* D_80073B8C;

void ActorsShared80132fd0(Actor101500* actor)
{
    VECTOR3*         vec;
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    u32              seed;
    s32              off;
    u16              val;
    u16              val2;
    u16*             tbl;
    u8*              head;
    s32              diff;
    s32              dist;
    s32              y;
    s32              off2;
    s32              diff2;
    s32              dist2;

    head              = *(u8**)0x1F8003FC;
    *(u8**)0x1F8003FC = head - 0x10;
    vec               = (VECTOR3*)(head - 0x10);
    work              = actor->field_1C;
    coord             = actor->field_2C->coords;
    switch (work->field_35C) {
        case 0:
            off  = work->field_364 + 0x708;
            diff = D_80073B8C->t[1] - off - coord->coord.t[1];
            dist = abs(diff);
            if (dist < 30 || --work->field_362 <= 0) {
                work->field_35C = 1;
            } else {
                work->field_366 = diff > 0 ? 30 : -30;
            }
            vec->vx                = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy                = 0;
            vec->vz                = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_372        = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376        = 100;
            work->field_244.pos.vy = -300;
            work->field_244.pos.vz = 0;
            break;
        case 1:
            work->field_366 = 0;
            work->field_360 = 0;
            work->field_36C = 0;
            if (--work->field_362 < 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                val             = ActorsShared80132fd0Durations[(Gp_LcgState >> 16) & 0xF];
                work->field_352 = 6;
                work->field_35C = 2;
                work->field_35E = val;
            }
            vec->vx         = Player_Status.coordMtx->t[0] - coord->coord.t[0];
            vec->vy         = 0;
            vec->vz         = Player_Status.coordMtx->t[2] - coord->coord.t[2];
            work->field_372 = ratan2((s16)vec->vx, (s16)vec->vz) & 0xFFF;
            work->field_376 = 100;
            break;
        case 2:
            work->field_360  = 200;
            work->field_35E -= 200;
            if ((s16)work->field_35E < 0) {
                tbl             = ActorsShared80132ac4Durations;
                work->field_352 = 5;
                seed            = Gp_LcgState * 5 + 0x71357911;
                val2            = tbl[(seed >> 16) & 0xF];
                Gp_LcgState     = seed;
                work->field_34C = 0x400F0002;
                work->field_380 = 15;
                work->field_35C = 1;
                work->field_362 = val2;
            }
            if (work->field_36C == 0) {
                vec->vx = Player_Status.coordMtx->t[0] - coord->coord.t[0];
                vec->vy = 0;
                vec->vz = Player_Status.coordMtx->t[2] - coord->coord.t[2];
                if (SquareRoot0(vec->vx * vec->vx + vec->vz * vec->vz) < 1000) {
                    work->field_352        = 10;
                    work->field_35C        = 3;
                    work->field_34C        = 0;
                    work->field_36C        = 1;
                    work->field_2DC.flags |= 0x8000;
                }
            }
            break;
        case 3:
            diff2           = D_80073B8C->t[1] - 0x640;
            work->field_360 = 100;
            work->field_366 = 180;
            if (diff2 < coord->coord.t[1] || work->field_36A != 0) {
                work->field_35C        = 4;
                work->field_36A        = 0;
                work->field_352        = 6;
                Gp_LcgState            = Gp_LcgState * 5 + 0x71357911;
                work->field_362        = ((Gp_LcgState >> 16) & 0xF) + 15;
                work->field_2DC.flags &= 0x7FFF;
            }
            break;
        case 4:
            off2  = coord->coord.t[1] + 0x708;
            diff2 = D_80073B8C->t[1] - off2;
            dist2 = abs(diff2);
            if (dist2 < 0x60 || --work->field_362 <= 0) {
                work->field_35C = 2;
            } else {
                work->field_366 = diff2 > 0 ? 0x60 : -0x60;
            }
            break;
    }
    *(u8**)0x1F8003FC += 0x10;
}

const u32 ActorsShared80132fd0Padding = 0;
