#include "common.h"

#include "actors/actor_105400.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/sound.h"
#include "main/wipsys.h"

extern u8 D_801153F4;
extern s8 D_80115416;

void func_800B4114(Actor05400Work* arg0, s32 arg1, s16 arg2, s32 arg3, s32 arg4);

/// Death handler of the main task (its state 2). Death state `field_32E` 3 is
/// the wait the hit handler enters when the enemy dies: effects are spawned
/// every fourth frame until message bit 1 moves it to 0. State 0 drops the
/// enemy's lock-on node and both collision objects and starts the sequence;
/// state 1 runs it for 0x78 frames, shrinking the model towards an eighth of
/// its scale while flickering through `D_actor_105400_8013CE90`, spawning the
/// same two randomly offset effects every fourth frame, setting bit 1 of the
/// model's `field_C` at frame 0x14, spawning effect 0x600A5 at 0x1E and
/// switching the light mode at 0x6E, then ends in state 2. Independently,
/// `field_330` 0 calls `Gp_ReleaseStateF0Add` once with this sub-state's entry
/// of `D_actor_105400_80133A2C` (message bit 2 clears the hold value 2). The
/// pose and colour are ticked every frame, and the enemy is destroyed once the
/// sequence has ended and the release has run.
void func_actor_105400_8013246C(GpEnemy* arg0, Actor05400* arg1)
{
    SVECTOR          ofs;
    VECTOR           pos;
    Actor05400Obj2C* obj;
    Actor05400Work*  work;
    Actor05400Work*  anim;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   tmp;
    Actor05400Clip*  clip;
    u16              scale;
    s32              r;
    s8               flag;
    s32              x;
    s32              z;
    s32              x2;
    s32              z2;
    s32              i;
    s32              value;

    obj   = arg1->field_2C;
    work  = arg1->field_1C;
    coord = obj->field_8;
    scale = 0x1000;
    switch (D_801153F4) {
        case 1:
            pos.vx = coord->workm.t[0];
            pos.vy = coord->workm.t[1];
            pos.vz = coord->workm.t[2];
            Gp_UpdateActorColor(arg1->field_20, &pos, 0, 0);
            return;
        case 2:
            obj->field_C = 0x80;
            return;
        case 0:
            break;
    }
    if ((work->field_33A & 1) && (s16)work->field_32E == 3) {
        work->field_32E = 0;
    }
    if ((work->field_33A & 2) && (s16)work->field_330 == 2) {
        work->field_330 = 0;
    }
    switch ((s16)work->field_32E) {
        case 0:
            work->field_326 = 0x1000;
            work->field_2FC = coord->coord;
            arg0->recs      = 0;
            Gp_UnlinkNode(&arg0->node);
            Gp_UnlinkObj(&work->node0);
            Gp_UnlinkObj(&work->node1);
            Gp_SetLightMode((GpObj4C*)arg0, 1);
            if (work->field_334 == 0) {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = r;
            } else {
                work->field_328 = 0;
                work->field_32C = 0;
                work->field_32E = 1;
                r               = Gp_LcgState * 5 + 0x71357911;
                Gp_LcgState     = r;
            }
            flag            = 1;
            work->field_32A = (((u32)r >> 16) & 0xF) + 0xA;
            D_80115416      = flag;
            break;
        case 1:
            if ((s16)work->field_326 > 0x200) {
                work->field_326 -= 0x20;
            }
            switch ((s16)work->field_32C) {
                case 0:
                    work->field_32A--;
                    if ((s16)work->field_32A <= 0) {
                        work->field_32C = 1;
                    }
                    scale = work->field_326;
                    break;
                case 1:
                    clip  = &D_actor_105400_8013CE90[(s16)work->field_32A];
                    scale = ((s16)work->field_326 * (s16)clip->field_2) >> 12;
                    if (clip->field_0 != 0) {
                        work->field_32C = 0;
                        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
                        work->field_32A = ((Gp_LcgState >> 16) & 0xF) + 0xA;
                    } else {
                        work->field_32A++;
                    }
                    break;
            }
            func_actor_105400_801336D4(arg1, &work->field_2FC, scale, 0);
            if (!(work->field_328 & 3)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx      = x;
                ofs.vy      = -0x9C4;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(0x600E0, coord, 0x400, &ofs);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx      = x2;
                ofs.vy      = -0x960;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(0x60070, coord, 0x30011600, &ofs);
            }
            work->field_328++;
            if ((s16)work->field_328 == 0x14) {
                obj->field_C |= 2;
            }
            if ((s16)work->field_328 == 0x1E) {
                Gp_SpawnEff(0x600A5, coord, 5, NULL);
            }
            if ((s16)work->field_328 == 0x6E) {
                Gp_SetLightMode((GpObj4C*)arg0, 2);
            }
            if ((s16)work->field_328 >= 0x78) {
                work->field_32E = 2;
            }
            tmp    = arg1->field_2C->field_8;
            pos.vx = tmp->workm.t[0];
            pos.vy = tmp->workm.t[1];
            pos.vz = tmp->workm.t[2];
            Gp_UpdateActorColor(arg1->field_20, &pos, 0, 0);
            break;
        case 2:
            break;
        case 3:
            arg0->node.flags = 1;
            work->field_328++;
            if (!(work->field_328 & 3)) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x = -x;
                }
                ofs.vx      = x;
                ofs.vy      = -0x9C4;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z           = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z = -z;
                }
                ofs.vz = z;
                Gp_SpawnEff(0x600E0, coord, 0x400, &ofs);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                x2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    x2 = -x2;
                }
                ofs.vx      = x2;
                ofs.vy      = -0x960;
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                z2          = (Gp_LcgState >> 16) & 0x3FF;
                if (!((Gp_LcgState >> 16) & 0x400)) {
                    z2 = -z2;
                }
                ofs.vz = z2;
                Gp_SpawnEff(0x60070, coord, 0x30011600, &ofs);
            }
            break;
    }
    if ((s16)work->field_330 == 0) {
        Gp_ReleaseStateF0Add((GpObj20E*)arg1, D_actor_105400_80133A2C[work->field_334]);
        work->field_330 = 1;
        Gp_ClearAreaFlag4((GpAreaKey*)&gGameSession->at4);
    }
    anim = arg1->field_1C;
    i    = 1;
    if ((s16)anim->field_320 != anim->field_322) {
        anim->field_322 = anim->field_320;
        anim->field_324 = 0;
        value           = D_actor_105400_80133A18[(s16)anim->field_320];
        for (; i < 10; i++) {
            func_800B4114(anim, i, (s16)anim->field_320, 0, value);
        }
    } else {
        TOUCH_REG(i);
        anim->field_324 += i;
        do {
            Gp_AnimTickIndex(&anim->anim, i);
            i++;
        } while (i < 10);
    }
    tmp    = arg1->field_2C->field_8;
    pos.vx = tmp->workm.t[0];
    pos.vy = tmp->workm.t[1];
    pos.vz = tmp->workm.t[2];
    Gp_UpdateActorColor(arg1->field_20, &pos, 0, 0);
    if ((s16)work->field_32E == 2 && (s16)work->field_330 == 1) {
        Gp_DestroyEnemy(arg0, (Task*)arg1);
    }
}
