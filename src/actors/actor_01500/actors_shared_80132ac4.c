#include "common.h"
#include "actors/actor_101500.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/wipsys.h"

void ActorsShared80132ac4(Actor101500* actor)
{
    Actor101500Work* work;
    GsCOORDINATE2*   coord;
    VECTOR*          frame;
    s32              flag;
    s16              pose;
    s16              pose2;
    s16              val;

    *(VECTOR**)0x1F8003FC -= 1;
    frame                  = *(VECTOR**)0x1F8003FC;
    work                   = actor->field_1C;
    coord                  = actor->field_2C->coords;
    flag                   = 0;
    if (work->field_37A != 0) {
        work->field_35A = 2;
        work->field_352 = 7;
        work->field_356 = 0;
        pose2           = ActorsShared80132ac4Durations[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
        work->field_358 = 1;
        work->field_364 = 0;
        work->field_34C = 0x400F0002;
        work->field_380 = 0xF;
        work->field_362 = pose2;
    }
    work->field_376 = 0;
    frame->vx       = Player_Status.coordMtx->t[0] - coord->coord.t[0];
    frame->vy       = 0;
    frame->vz       = Player_Status.coordMtx->t[2] - coord->coord.t[2];
    if (SquareRoot0(frame->vx * frame->vx + frame->vz * frame->vz) < 2500) {
        work->field_35A = 1;
        pose            = 3;
        if (work->field_36E != 0) {
            pose = 4;
        }
        work->field_352 = pose;
        work->field_34C = 0x400F0001;
        work->field_362 = 0;
        work->field_364 = 0;
        Gp_ArmStateF0(1);
    } else {
        if (D_801153F2[0] & 1) {
            if (work->field_362 == 0) {
                Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                work->field_362 = ((Gp_LcgState >> 16) & 0x1F) + 1;
            }
        }
        if (work->field_362 != 0) {
            work->field_362--;
            if (work->field_362 <= 0) {
                flag = 1;
            }
        }
        work->field_364--;
        if (work->field_364 == 0) {
            if (D_801153F2[1] != 0) {
                flag = 1;
            }
            Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
            work->field_364 = ((Gp_LcgState >> 16) & 0x1F) + 1;
        }
        if (actor->field_20->hp != ActorsShared80132ac4MaxHp) {
            flag = 1;
        }
        if (flag != 0) {
            work->field_35A = 2;
            pose2           = 7;
            if (work->field_36E != 0) {
                pose2 = 8;
            }
            __asm__("" : "+r"(pose2), "=r"(val));
            work->field_352 = pose2;
            val             = ActorsShared80132ac4Durations[((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16) & 0xF];
            work->field_358 = 1;
            work->field_37A = 1;
            work->field_364 = 0;
            work->field_34C = 0x400F0002;
            work->field_380 = 0xF;
            work->field_362 = val;
            Gp_ArmStateF0(1);
        }
    }
    *(VECTOR**)0x1F8003FC += 1;
}
