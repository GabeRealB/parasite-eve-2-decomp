#include "common.h"

#include "main/gameflow.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "gameplay/3CD8.h"
#include "actors/actor_206100.h"
#include "actors/actor_400500.h"
#include "psyq/inline_c.h"

#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")

extern s8 D_8007216C;

extern s32 D_80115738;

extern u32 Gp_LcgState;

extern s16 D_801818B8;

extern u16 D_80181A48;

extern s16 D_actor_206100_80158CD0;

/// Effect mode `func_actor_206100_8014E7D4` reads the state handler out of
/// `D_actor_206100_80149EC0` on: 1 and 2 are the frozen and retiring modes
/// `actor_503500` documents, and 0 is the running mode.
extern u8 D_801153F4;

void func_actor_206100_8014B0AC(Task* task, u8 arg1);
void func_actor_206100_8014DD3C(Task* task);
void func_actor_206100_8014DEAC(Task* task);
void func_actor_206100_8014FAE4(Task* task);
void func_8004BFF8(s16 angle, MATRIX* matrix);

GpEnemy* func_actor_206100_8014EE2C(s32 arg0);

void func_actor_206100_8014D574(Task* task);
void func_actor_206100_8014EB48(Task* task, s16 arg1);
void func_actor_206100_8014ED3C(Task* task, s16 arg1);
void func_actor_206100_8014F284(Task* task);
void func_actor_206100_8014F2F0(Task* task);
s16  func_actor_206100_8014F3C8(Task* task, s16 arg1);
void func_actor_206100_8014F738(Task* task);
void func_actor_206100_8014F770(Task* task);
void func_actor_206100_8014F7B4(Task* task);
void func_actor_206100_8014F878(Task* task);
void func_actor_206100_8014E964(Task* task);
void func_actor_206100_8014FBE4(Task* task);
void func_actor_206100_8014FCD4(Task* task);
void func_actor_206100_8014FDE8(Task* task);
void func_actor_206100_8014AB3C(GsCOORDINATE2* coord, u16 arg1, u16 arg2, s32 arg3);
void func_actor_206100_8014E228(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_80149ED0);

void func_actor_206100_8014A70C(GsCOORDINATE2* coord, u16 arg1, u16 arg2, u32 arg3)
{
    SVECTOR vec;
    s32     i;
    u16     variant;
    u16     param;

    if (Gp_State1C->field_4 != 0) {
        func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, 0x400, 0);
        if (Gp_State1C->field_4 >= 4) {
            return;
        }
    }

    variant = (arg3 >> 12) & 0xF;
    param   = arg3 & 0xFFF;

    switch (arg2) {
        case 0:
            Gp_SpawnEff(D_80115738, coord, 0x14001000 + param + variant, NULL);
            break;

        case 1:
            func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, param, 0);
            if (!(arg1 & 1)) {
                Gp_SpawnEff(D_80115738, coord, 0x01000000 + param + variant, NULL);
            }
            if (!(arg1 & 7)) {
                SVECTOR* dir;

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;

        case 2:
            func_actor_206100_8014AB3C(coord, ((u32)arg1 >> 1) % 6, param, 0);
            Gp_SpawnEff(D_80115738, coord, 0x10001000 + param + variant, NULL);
            for (i = 0; i < 4; i++) {
                SVECTOR* dir;

                Gp_SpawnEff(D_80115738, coord, 0x02001000 + param + variant, NULL);

                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vx      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vy      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                vec.vz      = 0x80 - ((Gp_LcgState >> 16) & 0xFF);

                dir = &vec;
                VectorNormalSS(dir, dir);
                gte_lddp(0x40);
                gte_ldsv(dir);
                gte_gpf12_real();
                gte_stsv(dir);
                Gp_SpawnEff(0x600E0, coord, param, dir);
            }
            break;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014AB3C);

void func_actor_206100_8014AF74(Task* task)
{
    Actor206100Work* work;
    TmdObject*       tmd;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    u16              hp;

    tmd                   = task->extra;
    work                  = (Actor206100Work*)task->work;
    enemy                 = (GpEnemy*)task->spawnArg2;
    tmd->otOffset         = 0xA;
    tmd->lightMtx         = &work->lightMtx;
    tmd->flags            = 0;
    tmd->colorMtx         = &work->colorMtx;
    coord                 = tmd->coords;
    work->eff_4C0.field_0 = &((TmdObject*)task->extra)->coords[1];
    work->eff_4C0.field_4 = 0x580;
    work->eff_4C0.field_6 = 3;
    enemy->field_4        = &coord->coord;
    enemy->field_48       = 0;
    enemy->field_1C.vx    = 0;
    enemy->field_1C.vy    = 0;
    enemy->field_1C.vz    = 0;
    enemy->field_18       = &((TmdObject*)task->extra)->coords[4];
    Gp_LinkNode(&enemy->node);
    enemy->node.field_4 = 1;
    enemy->field_54     = (s32)work->rec_384;
    enemy->field_50     = &D_actor_206100_80155198;
    hp                  = D_actor_206100_80155198.field_4;
    enemy->field_42     = hp;
    enemy->field_40     = hp;
    coord->sub          = &Gfx_ViewCoord;
    func_800B3F84(&work->anim, D_actor_206100_80158B24, (GpAnimObj*)tmd, work->animAux, work->slots);
    func_actor_206100_8014F18C(task);
    work->field_43E = ratan2(-coord->coord.m[2][0], coord->coord.m[2][2]);
    work->field_557 = 4;
}

/// Drives the retract effect the actor plays when `field_54D` clears: the three
/// parts `func_actor_206100_8014AF74` parents to the root coordinate are
/// rebuilt from scratch each frame, the second and third from an Euler angle
/// pair and the fourth from an angle plus a scale.
///
/// `field_54D` picks the direction of travel.  While it reads 1 the actor is
/// retiring: sub-state `field_53A` 0 latches the two angle pairs out of the
/// parts' matrices and falls straight into 1, which folds all six components
/// toward zero by a sixteenth and arms 2 once every one of them is within
/// 0x30; 2 then ramps `field_53C` down toward the 0x2AA the retired parts
/// shrink to.  Once `field_54D` reads 0 the else arm ramps `field_53C` back up
/// toward 0x1000, and while it is still under 0xF80 it rebuilds the parts the
/// same way; at 0xF80 the actor is whole again and the effect hands it to
/// `func_actor_206100_8014E228`.
///
/// The fourth part is the one that blows up rather than shrinks: `field_53C`
/// scales parts 2 and 3 down, so `mc` gets its reciprocal, `0x1000000 /
/// field_53C`.  Note also that the two arms clear `field_53A` rather than the
/// join after them: written once after the `if`, the else arm's tail becomes
/// instruction-for-instruction the case-2 tail and the jump optimiser merges
/// the two, costing the case-2 copy of the last fifty instructions.
void func_actor_206100_8014B0AC(Task* task, u8 arg1)
{
    VECTOR            scale;
    Actor206100Matrix rot;
    Actor206100Matrix ma;
    Actor206100Matrix mb;
    SVECTOR           euler;
    Actor206100Matrix mc;
    Actor206100Work*  work;
    GsCOORDINATE2*    base;
    GsCOORDINATE2*    c2;
    GsCOORDINATE2*    c3;
    GsCOORDINATE2*    c4;
    s32               invScale;

    base = ((TmdObject*)task->extra)->coords;
    work = (Actor206100Work*)task->work;
    c2   = &base[2];
    c3   = &base[3];
    c4   = &base[4];

    switch (work->field_54D) {
        case 1:
            switch (work->field_53A) {
                case 0:
                    base[0].flg = 0;
                    base[1].flg = 0;
                    base[2].flg = 0;
                    base[3].flg = 0;
                    base[4].flg = 0;
                    Gp_UpdateCoord(c4);
                    Gp_MtxToEuler(&c2->coord, &work->field_4D8);
                    Gp_MtxToEuler(&c3->coord, &work->field_4E0);
                    work->field_53A = 1;
                    work->field_53C = 0x1000;
                    /* fallthrough */
                case 1: {
                    Actor206100MatrixWords* ir;

                    work->field_4D8.vx = (u16)work->field_4D8.vx + ((s32) - (work->field_4D8.vx * 0x10) >> 7);
                    work->field_4D8.vy = (u16)work->field_4D8.vy + ((s32) - (work->field_4D8.vy * 0x10) >> 7);
                    work->field_4D8.vz = (u16)work->field_4D8.vz + ((s32) - (work->field_4D8.vz * 0x10) >> 7);
                    work->field_4E0.vx = (u16)work->field_4E0.vx + ((s32) - (work->field_4E0.vx * 0x10) >> 7);
                    work->field_4E0.vy = (u16)work->field_4E0.vy + ((s32) - (work->field_4E0.vy * 0x10) >> 7);
                    work->field_4E0.vz = (u16)work->field_4E0.vz + ((s32) - (work->field_4E0.vz * 0x10) >> 7);
                    ir                 = &rot.ident;
                    rot.ident.m00_m01  = 0x1000;
                    rot.ident.m02_m10  = 0;
                    ir->m11_m12        = 0x1000;
                    rot.ident.m20_m21  = 0;
                    ir->m22            = 0x1000;
                    RotMatrix(&work->field_4D8, &rot.mat);
                    ActorsShared80132c4c(&rot.mat, &c2->coord);
                    rot.ident.m00_m01 = 0x1000;
                    rot.ident.m02_m10 = 0;
                    ir->m11_m12       = 0x1000;
                    rot.ident.m20_m21 = 0;
                    ir->m22           = 0x1000;
                    RotMatrix(&work->field_4E0, &rot.mat);
                    ActorsShared80132c4c(&rot.mat, &c3->coord);
                    if ((abs(work->field_4D8.vx) < 0x30) && (abs(work->field_4D8.vy) < 0x30) && (abs(work->field_4D8.vz) < 0x30) &&
                        (abs(work->field_4E0.vx) < 0x30) && (abs(work->field_4E0.vy) < 0x30) && (abs(work->field_4E0.vz) < 0x30)) {
                        work->field_53A = 2;
                    }
                    c2->flg = 0;
                    c3->flg = 0;
                    c4->flg = 0;
                    Gp_UpdateCoord(c4);
                    break;
                }
                case 2: {
                    Actor206100MatrixWords* ia;
                    Actor206100MatrixWords* ib;
                    Actor206100MatrixWords* ic;
                    Actor206100MatrixWords* ir;

                    Gp_MtxToEuler(&c4->coord, &euler);
                    work->field_53C  = (u16)work->field_53C + ((0x2AA - work->field_53C) >> 3);
                    ia               = &ma.ident;
                    ma.ident.m00_m01 = 0x1000;
                    ma.ident.m02_m10 = 0;
                    ia->m11_m12      = 0x1000;
                    ma.ident.m20_m21 = 0;
                    ia->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    scale.vz         = work->field_53C;
                    ScaleMatrix(&ma.mat, &scale);
                    ActorsShared80132c4c(&ma.mat, &c2->coord);
                    ib               = &mb.ident;
                    mb.ident.m00_m01 = 0x1000;
                    mb.ident.m02_m10 = 0;
                    ib->m11_m12      = 0x1000;
                    mb.ident.m20_m21 = 0;
                    ib->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    scale.vz         = 0x1000;
                    ScaleMatrix(&mb.mat, &scale);
                    ActorsShared80132c4c(&mb.mat, &c3->coord);
                    ic               = &mc.ident;
                    mc.ident.m00_m01 = 0x1000;
                    mc.ident.m02_m10 = 0;
                    ic->m11_m12      = 0x1000;
                    mc.ident.m20_m21 = 0;
                    ic->m22          = 0x1000;
                    scale.vx         = 0x1000;
                    scale.vy         = 0x1000;
                    invScale         = 0x1000000 / work->field_53C;
                    scale.vz         = invScale;
                    ScaleMatrix(&mc.mat, &scale);
                    ir                = &rot.ident;
                    rot.ident.m00_m01 = 0x1000;
                    rot.ident.m02_m10 = 0;
                    ir->m11_m12       = 0x1000;
                    rot.ident.m20_m21 = 0;
                    ir->m22           = 0x1000;
                    RotMatrix(&euler, &rot.mat);
                    MulMatrix(&mc.mat, &rot.mat);
                    ActorsShared80132c4c(&mc.mat, &c4->coord);
                    base[2].flg = 0;
                    base[3].flg = 0;
                    base[4].flg = 0;
                    Gp_UpdateCoord(c4);
                    break;
                }
            }
            break;
        case 0:
            base[0].flg = 0;
            base[1].flg = 0;
            base[2].flg = 0;
            base[3].flg = 0;
            base[4].flg = 0;
            Gp_UpdateCoord(c4);
            if (work->field_53C < 0xF80) {
                Actor206100MatrixWords* ia;
                Actor206100MatrixWords* ib;
                Actor206100MatrixWords* ic;
                Actor206100MatrixWords* ir;

                Gp_MtxToEuler(&c4->coord, &euler);
                work->field_53C  = (u16)work->field_53C + ((0x1000 - work->field_53C) >> 2);
                ia               = &ma.ident;
                ma.ident.m00_m01 = 0x1000;
                ma.ident.m02_m10 = 0;
                ia->m11_m12      = 0x1000;
                ma.ident.m20_m21 = 0;
                ia->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = work->field_53C;
                ScaleMatrix(&ma.mat, &scale);
                ActorsShared80132c4c(&ma.mat, &c2->coord);
                ib               = &mb.ident;
                mb.ident.m00_m01 = 0x1000;
                mb.ident.m02_m10 = 0;
                ib->m11_m12      = 0x1000;
                mb.ident.m20_m21 = 0;
                ib->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                scale.vz         = 0x1000;
                ScaleMatrix(&mb.mat, &scale);
                ActorsShared80132c4c(&mb.mat, &c3->coord);
                ic               = &mc.ident;
                mc.ident.m00_m01 = 0x1000;
                mc.ident.m02_m10 = 0;
                ic->m11_m12      = 0x1000;
                mc.ident.m20_m21 = 0;
                ic->m22          = 0x1000;
                scale.vx         = 0x1000;
                scale.vy         = 0x1000;
                invScale         = 0x1000000 / work->field_53C;
                scale.vz         = invScale;
                ScaleMatrix(&mc.mat, &scale);
                ir                = &rot.ident;
                rot.ident.m00_m01 = 0x1000;
                rot.ident.m02_m10 = 0;
                ir->m11_m12       = 0x1000;
                rot.ident.m20_m21 = 0;
                ir->m22           = 0x1000;
                RotMatrix(&euler, &rot.mat);
                MulMatrix(&mc.mat, &rot.mat);
                ActorsShared80132c4c(&mc.mat, &c4->coord);
                base[2].flg = 0;
                base[3].flg = 0;
                base[4].flg = 0;
                Gp_UpdateCoord(c4);
                work->field_53A = 0;
            } else {
                func_actor_206100_8014E228(task);
                work->field_53A = 0;
            }
            break;
    }
}
/// Latches the actor's position and picks the nearer of the two `Gp_ActorSlots`
/// actors as its walk target: it stores the standing post in `field_434` /
/// `field_438`, then measures the XZ distance to each slot from the root
/// coordinate, keeping the closer one's position in `field_4D0` / `field_4D4`
/// and its distance in `field_528`.
///
/// Two details are load-bearing.  `Gp_ActorSlots[0]` is read *before* the three
/// post stores: written after them the scheduler moves the whole `lui` / `lw`
/// group below the stores, which costs twelve bytes of schedule and shifts
/// every later branch target.  And the player delta `d0` is normalised and fed
/// to `ratan2` even when slot 1 was the closer one, so `field_51C` follows the
/// player's bearing rather than the target's.
void func_actor_206100_8014B698(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   c0;
    GsCOORDINATE2*   c1;
    GpActorWork*     player;
    SVECTOR          d0;
    SVECTOR          d1;
    s32              dist0;
    s32              dist1;

    work            = (Actor206100Work*)task->work;
    coord           = ((TmdObject*)task->extra)->coords;
    player          = Gp_ActorSlots[0];
    work->field_434 = (u16)coord->coord.t[0];
    work->field_436 = (u16)coord->coord.t[1];
    work->field_438 = (u16)coord->coord.t[2];
    if (player != NULL) {
        c0    = player->extra->coords;
        d0.vx = (u16)c0->coord.t[0] - (u16)coord->coord.t[0];
        d0.vy = (u16)c0->coord.t[1] - (u16)coord->coord.t[1];
        d0.vz = (u16)c0->coord.t[2] - (u16)coord->coord.t[2];
        dist0 = SquareRoot0(d0.vx * d0.vx + d0.vz * d0.vz);
        if (Gp_ActorSlots[1] == NULL) {
            work->field_4D0 = c0->coord.t[0];
            work->field_4D2 = c0->coord.t[1];
            work->field_4D4 = c0->coord.t[2];
            work->field_528 = dist0;
        } else {
            c1    = Gp_ActorSlots[1]->extra->coords;
            d1.vx = (u16)c1->coord.t[0] - (u16)coord->coord.t[0];
            d1.vy = (u16)c1->coord.t[1] - (u16)coord->coord.t[1];
            d1.vz = (u16)c1->coord.t[2] - (u16)coord->coord.t[2];
            dist1 = SquareRoot0(d1.vx * d1.vx + d1.vz * d1.vz);
            if (dist1 < dist0) {
                work->field_4D0 = c1->coord.t[0];
                work->field_4D2 = c1->coord.t[1];
                work->field_4D4 = c1->coord.t[2];
                dist0           = dist1;
            } else {
                work->field_4D0 = c0->coord.t[0];
                work->field_4D2 = c0->coord.t[1];
                work->field_4D4 = c0->coord.t[2];
            }
            work->field_528 = dist0;
        }
        VectorNormalSS(&d0, &d0);
        work->field_51C = (ratan2(d0.vx, d0.vz) - (u16)work->field_43E) & 0xFFF;
    }
}
/// Tick handler of the beam child `func_actor_206100_8014EEC0` starts, the
/// same shape the marker `Actor00400_Fn02D48` has: while the effect mode
/// `D_801153F4` is 0 it advances the child's `field_5A` and folds `field_58` /
/// `field_5A` / `field_5C` into the root coordinate, raises `hit` when
/// either collision slot reports one of the three kinds 1/3/5 or when
/// `func_800E0C10`'s push-back says the beam is crowded, and retires the child
/// - clearing the object's draw flags, bumping the task state and switching
/// the effect kind to 2 - once `killCountdown` reaches 0x5B or the flag is up.
/// `field_64` is the scale the setup hands to `func_actor_206100_8014A70C`
/// biased by 0x10002000; it ramps 0x100 a frame to 0x600 and then holds.
void func_actor_206100_8014B8B4(Task* task)
{
    Actor206100ChildWork* child;
    GsCOORDINATE2*        coord;
    GpDeltaScratch        delta;
    s32                   mask;
    s32                   hit;
    s32                   mode;
    s32                   i;
    s32                   n;
    s32                   v;

    hit   = 0;
    child = (Actor206100ChildWork*)task->work;
    coord = ((TmdObject*)task->extra)->coords;
    mode  = 1;
    if (D_801153F4 == 0) {
        child->field_5A   += 2;
        *(u32*)&coord->flg = 0;
        coord->coord.t[0] += child->field_58;
        coord->coord.t[1] += child->field_5A;
        coord->coord.t[2] += child->field_5C;
        if (Gp_FindRec18(child->rec, 0) != 0) {
            for (i = 0; i < 2; i++) {
                switch (child->rec[i].key & 0xFFFF0000) {
                    case 0x10000:
                    case 0x30000:
                    case 0x50000:
                        hit = 1;
                        break;
                }
            }
        }
        n = func_800E0C10(child->rec, &delta, 2, &mask);
        if (n < 3) {
            if (n > 0) {
                if ((mask & 8) == 0) {
                    hit = 1;
                }
            }
        }
        Gp_ClearRec18Occupied(child->rec);
        if ((++task->killCountdown >= 0x5B) || (hit != 0)) {
            task->killCountdown = 0;
            child->obj.flags   &= 0x3FFF;
            mode                = 2;
            task->state        += 1;
        }
        v = child->field_64;
        if (v < 0x600) {
            child->field_64 = v + 0x100;
        } else {
            child->field_64 = 0x600;
        }
        func_actor_206100_8014A70C(coord, child->field_60, mode, child->field_64 + 0x10002000);
    }
}
/// Damage / knock-back tick: walks the six contact records of the actor's
/// `rec_384` table and turns the first occupied one into a hit.  `field_504` is
/// the cooldown that gates it -- `Gp_GetIdParam2` of the record arms it, and
/// `hit` / `heavy` are the two sub-state requests it leaves in `field_52C`, the
/// light flinch (1) and the heavy recovery (2) that the consumer `take_request`
/// maps to the 0x135 and 0x3A0 clips.  Both are initialised before the loop
/// rather than written as literals at each site: the arms that write
/// `field_52C` land in different basic blocks, and a literal in each of them is
/// reloaded per block, so only a value that is live from the entry block keeps
/// one register for all of them.  `hit` doubles as the value of the two hit
/// flags `field_52A` / `field_54B`, which the actor raises on the frame it
/// takes the hit and which stop the walk.
///
/// While the cooldown reads 0 the record's packed id is rolled through
/// `Gp_ComputeDamage` and `Gp_RollEnemyChance` -- a successful roll scales the
/// damage and selects the effect kind -- and the result is applied to the
/// enemy's `field_40` health counter through `func_800E2C78` and
/// `func_800DA6E8`.  The id's low parameter then picks one of the three flag
/// setters, one of the hit reaction sizes, or clears the hit flag again, and
/// the `0x7F`/`0x8000` pair on an id ending 0x1C forces the light reaction and
/// clears bit 0 of the object's draw flags.  The `else` arm is the same record
/// arriving with the cooldown still up: id parameter 0xD sounds
/// `func_800FDB18` on the root coordinate's second part alone.
///
/// The tail turns the object's own flags into requests the same way -- bit 0 of
/// `field_4C` clears and asks for the heavy reaction, bit 1 asks for the
/// consumer's sound-and-state pair, and the `Gp_TickObjFlag4` countdown applies
/// its knock-back and asks for the light one -- and every frame ends by
/// releasing the record table and counting the cooldown down, or clamping it to
/// 0 so it never goes negative.
void func_actor_206100_8014BAA8(Task* task)
{
    Actor206100Work* work;
    GpEnemy*         enemy;
    s32              kind;
    s16              amount;
    s32              dmg;
    s32              tmp;
    s32              tick;
    s32              i;
    s32              hit;
    s32              heavy;

    kind            = 0;
    hit             = 1;
    heavy           = 2;
    work            = (Actor206100Work*)task->work;
    enemy           = (GpEnemy*)task->spawnArg2;
    work->field_52A = 0;
    for (i = 0; i < 6; i++) {
        if ((work->rec_384[i].key & 0xFFFF0000) == 0x20000) {
            if (work->field_504 == 0) {
                work->field_52A = hit;
                work->field_54B = hit;
                dmg             = Gp_ComputeDamage(work->rec_384[i].key, work->field_528, 0, 0);
                amount          = dmg;
                work->field_504 = Gp_GetIdParam2(work->rec_384[i].key);
                if (Gp_RollEnemyChance(enemy, work->rec_384[i].key, 0) != 0) {
                    amount = ((u32)dmg << 16) >> 14;
                    kind   = 1;
                }
                func_800FDB18(Gp_GetIdParam1(work->rec_384[i].key) & 0xFFFF,
                              &((TmdObject*)task->extra)->coords[work->field_557], 0, &work->eff_4C0);
                if (amount >= 0xB4) {
                    work->field_52C = heavy;
                } else {
                    work->field_52C = hit;
                }
                switch (Gp_GetIdParam0(work->rec_384[i].key) & 0xFFFF) {
                    case 0:
                        break;
                    case 1:
                        Gp_SetObjFlag1((GpObj4C*)enemy);
                        break;
                    case 2:
                        Gp_SetObjFlag2((GpObj5D*)enemy, work->rec_384[i].key, 0);
                        break;
                    case 3:
                        Gp_SetObjFlag4((GpObj5C*)enemy, work->rec_384[i].key, 0);
                        break;
                    case 4:
                        work->field_52C = 4;
                        break;
                    case 5:
                    case 6:
                        work->field_52C = heavy;
                        break;
                    case 7:
                        kind            = 2;
                        work->field_52C = 2;
                        amount         += amount;
                        break;
                    case 8:
                        work->field_52C = 0;
                        work->field_52A = 0;
                        break;
                    case 9:
                        work->field_52C = hit;
                        break;
                }
                if ((work->rec_384[i].key & 0x7F) == 0x1C && (work->rec_384[i].key & 0x8000) == 0) {
                    enemy->field_4C &= 0xFE;
                    work->field_52C  = hit;
                }
                tmp = kind;
                switch (tmp) {
                    case 1:
                        Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[work->field_557], 0, 0);
                        break;
                    case 2:
                        Gp_SpawnEff(0x6009C, &((TmdObject*)task->extra)->coords[work->field_557], 2, 0);
                        break;
                }
                func_800E2C78((GpObj40*)enemy, work->rec_384[i].key, amount, 0);
                func_800DA6E8(&enemy->node, amount, 0);
                enemy->field_40 -= amount;
                if ((s16)enemy->field_40 < 0) {
                    enemy->field_40 = 0;
                }
            } else if ((Gp_GetIdParam1(work->rec_384[i].key) & 0xFFFF) == 0xD) {
                func_800FDB18(0xD, &((TmdObject*)task->extra)->coords[1], 0, &work->eff_4C0);
            }
        }
        if (work->field_52A != 0) {
            break;
        }
    }
    if (enemy->field_4C & 1) {
        enemy->field_4C &= 0xFE;
        work->field_52C  = 2;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        work->field_52C  = 3;
    }
    if (enemy->field_4C & 0xC) {
        tmp  = Gp_TickObjFlag4((GpObj5C*)enemy);
        tick = (s16)tmp;
        if (tick != 0) {
            enemy->field_40 -= tmp;
            func_800DA6E8(&enemy->node, tick, 0);
            if ((s16)enemy->field_40 < 0) {
                enemy->field_40 = 0;
            }
            work->field_52A = 1;
            work->field_52C = 1;
        }
        if (Gp_ObjFlag4Expired((GpObj5C*)enemy) != 0) {
            enemy->field_4C &= 0xF3;
        }
    }
    Gp_ClearRec18Occupied(work->rec_384);
    if (work->field_504 > 0) {
        work->field_504--;
    } else {
        work->field_504 = 0;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014BEC4);

/// Spawn state of `D_actor_206100_80149E94`: builds the actor's work block --
/// `Mem_Calloc(0x558, 0)` parked straight in `Task::work`, the actor destroyed
/// if that fails -- empties both companion slots of `D_actor_206100_80158CBC`
/// through their index (the walked-pointer form gives the timer field an
/// induction variable of its own) and calls the setup `func_actor_206100_8014AF74`
/// with the block in place.
///
/// It then asks for the first clip: `field_50C` is written 2 -- the request kind
/// the animation player picks up -- with `field_510` as the clip and `field_51A`
/// the step scale, and the kind read back decides what to do with the clip phase
/// `field_512`.  Kind 1 either zeroes it, when the clip now playing is not the
/// requested one, or hands it to `func_actor_206100_8014F3C8` and re-requests
/// kind 3; kind 2 goes through `func_actor_206100_8014F284` and zeroes it; kind
/// 3 advances it by one.  Every path lands on slot 1 and steps animation slots
/// 1 through 0xE once.
///
/// The tail seeds the walk/HP scales (`field_508`, `field_50A`, `field_526` and
/// `field_53E`), zeroes the root coordinate's translation, takes the state-0
/// reference `Gp_IncStateF0Ref` and re-arms the actor in state 1 with the state
/// and sub-state indices cleared -- the two index pairs written through the two
/// fresh `Task::work` loads, the block-local store shape `func_actor_206100_8014CE60`
/// uses.
void func_actor_206100_8014C274(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* req;
    Actor206100Work* anim;
    Actor206100Work* state;
    Actor206100Work* tail;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    s16              kind;
    s32              slot;
    s32              i;

    enemy      = task->spawnArg2;
    coord      = ((TmdObject*)task->extra)->coords;
    task->work = Mem_Calloc(0x558, 0);
    work       = (Actor206100Work*)task->work;
    if (work == NULL) {
        Gp_DestroyEnemy(enemy, task);
        return;
    }
    D_801818B8      = 1;
    work->field_536 = D_80181A48;
    for (i = 0; i < 2; i++) {
        D_actor_206100_80158CBC[i].enemy = NULL;
        D_actor_206100_80158CBC[i].timer = 0;
    }
    func_actor_206100_8014AF74(task);
    req            = (Actor206100Work*)task->work;
    req->field_51A = 0x10;
    req->field_510 = 3;
    req->field_50C = 2;
    anim           = (Actor206100Work*)task->work;
    kind           = anim->field_50C;
    if (kind == 1) {
        if (anim->field_50E != anim->field_510) {
            anim->field_512 = 0;
        } else {
            anim->field_512 = func_actor_206100_8014F3C8(task, anim->field_512);
        }
        func_actor_206100_8014F2F0(task);
        anim->field_50C = 3;
        goto block_13;
    }
    if (kind == 2) {
        func_actor_206100_8014F284(task);
        anim->field_50C = 3;
        anim->field_512 = 0;
        goto block_13;
    }
    slot = 1;
    if (kind == 3) {
        TOUCH_REG(slot);
        anim->field_512 = (s16)((u16)anim->field_512 + 1);
    block_13:
        slot = 1;
    }
    do {
        Gp_AnimTickIndex((GpAnimCtx*)anim, slot);
        slot = slot + 1;
    } while (slot < 0xF);
    work->field_508   = 0x1000;
    work->field_50A   = 0x1000;
    work->field_53E   = 0x1EAA;
    work->field_54D   = 1;
    coord->coord.t[0] = 0;
    work->field_526   = 0x2710;
    coord->coord.t[1] = 0x2710;
    coord->coord.t[2] = 0;
    ((void (*)(s32))Gp_IncStateF0Ref)(0);
    state            = (Actor206100Work*)task->work;
    task->state      = 1;
    state->field_520 = 0;
    state->field_522 = 0;
    tail             = (Actor206100Work*)task->work;
    tail->field_520  = 0;
    tail->field_522  = 0;
}
INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E5C);

INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014C458);

/// Teleport: ramps the white-out `func_actor_206100_8014DEAC` fades with, and
/// once it is fully up, hands slot 3 the actor's new position and re-arms the
/// actor on the far side, spawning the screen tint `D_actor_206100_80158CCC`
/// describes as it goes.
void func_actor_206100_8014CB68(Task* task)
{
    Actor206100Work*  work;
    Actor206100Work*  work2;
    TmdObject*        tmd;
    GsCOORDINATE2*    coord;
    Actor206100Msg3E9 msg;

    work  = (Actor206100Work*)task->work;
    tmd   = task->extra;
    coord = tmd->coords;
    func_actor_206100_8014DEAC(task);
    work->field_51E = work->field_51E + 6;
    if ((s16)work->field_51E >= 0x100) {
        work->field_51E = 0xFF;
    }
    Fade_DrawOverlay((u8)work->field_51E, (u8)work->field_51E, (u8)work->field_51E, 2);
    if ((s16)work->field_51E == 0xFF) {
        work->obj_364.flags |= 0x8000;
        work->obj_414.flags |= 0x8000;
        msg.pos.vx           = 0x690;
        msg.pos.vy           = 0x1388;
        msg.pos.vz           = 0x898;
        msg.rot.vx           = 0;
        msg.rot.vy           = 0x200;
        msg.rot.vz           = 0;
        Gp_DispatchMsg((Task*)Game_GetPtrSlot(3), 0x3E9, (s32)&msg, 0);
        work2                         = (Actor206100Work*)task->work;
        work2->field_51A              = 0x10;
        work2->field_510              = 3;
        work2->field_50C              = 2;
        coord->coord.t[1]             = 0xDAC;
        work->field_526               = 0xDAC;
        coord->coord.t[0]             = 0x157C;
        coord->coord.t[2]             = 0x157C;
        work->field_43C               = 0;
        work->field_43E               = 0xA00;
        work->field_440               = 0;
        work->field_553               = Mc_SaveData.at4.loc.view;
        Mc_SaveData.at4.loc.view      = 7;
        work->field_51E               = 0;
        work->field_54D               = 0;
        work->field_522               = work->field_522 + 1;
        D_actor_206100_80158CCC.span  = 1;
        D_actor_206100_80158CCC.scale = 0x60;
        D_actor_206100_80158CCC.r     = 0x40;
        D_actor_206100_80158CCC.blend = 1;
        D_actor_206100_80158CCC.g     = 0x80;
        D_actor_206100_80158CCC.b     = 0x80;
        work->field_4F8               = Task_SpawnFromTable(&D_actor_206100_80158AF0, 0, 0, (s32)&D_actor_206100_80158CCC);
    }
}
void func_actor_206100_8014CD08(Task* task)
{
    Actor206100Work*  work;
    Actor206100Work*  work2;
    TmdObject*        tmd;
    GsCOORDINATE2*    coord;
    Actor206100Msg3E9 msg;

    work            = (Actor206100Work*)task->work;
    tmd             = task->extra;
    coord           = tmd->coords;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 3) {
        SndEvt_EnqueueType6(0x551E0004, 0, 0);
    }
    if ((s16)work->field_51E == 0x22) {
        coord->coord.t[0] = 0;
        coord->coord.t[2] = 0;
        work->field_526   = 0x1388;
        work->field_51E   = 0U;
        coord->coord.t[1] = 0x1B58;
        work->field_43E   = 0;
        D_8007216C        = 6;
        Gp_SetLightMode(*(void**)((u8*)task + OFFSET_OF(Task, spawnArg2)), 0);
        Gp_MsgPlayer3F3(0);
        msg.pos.vx = 0x690;
        msg.pos.vy = 0x1388;
        msg.pos.vz = 0x898;
        msg.rot.vx = 0;
        msg.rot.vy = 0xA00;
        msg.rot.vz = 0;
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E9, (s32)&msg, 0);
        work2            = (Actor206100Work*)task->work;
        work2->field_51A = 0x10;
        work2->field_510 = 1;
        work2->field_50C = 2;
        work->field_522  = work->field_522 + 1;
        return;
    }
    work->field_526 = (u16)(work->field_526 + ((s32)(0x1D4C - (s16)work->field_526) >> 2));
    func_actor_206100_8014EA8C(task, 0x30, work->field_43E);
}
/// State handler 4 of `D_actor_206100_80149E94`: clears the fixed-address
/// `D_801818B8` flag, ticks the per-state counter `field_51E` and seeds
/// `D_actor_206100_80158CD0` with 2 on its first frame.
///
/// Frame 3 retires the child task `func_actor_206100_8014CB68` spawned into
/// `field_4F8` and, if the kill left the counter where it was, fires the
/// overlay's sound event 0x551E0003.  Frame 0xC splats the 0x01202148 particle
/// ring -- the same one `func_actor_206100_8014D574` fires, at a radius of
/// 0x1000 and a constant y of -0x294 -- and frame 0x46 hands the actor to state
/// 1 (`func_actor_206100_8014CD08`): it clears the model coordinate's x and z,
/// plays the weapon and 0x3F3 messages under light mode 2, and zeroes the
/// sub-state index along with the new state.
///
/// The frame-0x46 block reads `task->work` again rather than reusing `work`,
/// the fresh load that keeps the pair of stores a block-local quantity -- the
/// same reload `set_state` below makes.
void func_actor_206100_8014CE60(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   ring;
    SVECTOR          vec;
    s32              i;
    s16              y;
    s16              frame;

    work            = (Actor206100Work*)task->work;
    D_801818B8      = 0;
    coord           = ((TmdObject*)task->extra)->coords;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 1) {
        D_actor_206100_80158CD0 = 2;
    }
    frame = (s16)work->field_51E;
    if (frame == 3) {
        if (work->field_4F8 != NULL) {
            Task_Kill(work->field_4F8);
        }
        if ((s16)work->field_51E == frame) {
            SndEvt_EnqueueType6(0x551E0003, 0, 0);
        }
    }
    if ((s16)work->field_51E == 0xC) {
        y    = -0x294;
        i    = 0;
        ring = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, ring, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E == 0x46) {
        Gp_MsgPlayerWeapon(1);
        Gp_MsgPlayer3F3(1);
        D_8007216C        = 2;
        coord->coord.t[0] = 0;
        coord->coord.t[2] = 0;
        next              = (Actor206100Work*)task->work;
        next->field_520   = 1;
        next->field_522   = 0;
    }
}
/// Clears `field_522` and hands `field_520` the new state, reloading the work
/// block through the task rather than taking the caller's pointer: the fresh
/// load is what makes `state` a block-local quantity, which is what lets
/// local-alloc hand it `$v0` (see `take_request` below).
static __inline__ void set_state(Task* task, s32 state)
{
    Actor206100Work* next = (Actor206100Work*)task->work;

    next->field_520 = state;
    next->field_522 = 0;
}

/// Consumes the pending sub-state request in `field_52C`, which is only
/// honoured while `field_52A` reads 1, and returns 1 when it did, so the caller
/// skips this frame's sub-state handler.  Requests 1 and 2 run an animation
/// through `func_actor_206100_8014EB48` (0x135, then the 0x3A0 recovery);
/// 3 and 4 sound `SndEvt_EnqueueType7` and move the actor to state 8 and 7 at
/// sub-state 0.  Every arm clears `field_52C`.
///
/// The arms that only run an animation `break` to the single `return 0` after
/// the switch: that leaves a `return 0` block between the last arm and the
/// join, which dbr later steals into the branch delay slots, while the arms
/// that set a state `return 1` straight past it.  The `s16` return is what
/// makes the flag a halfword value: promoting it in the caller is the
/// `addu $v0,$a0,$zero` at the join, where an `s32` return leaves the flag in
/// `$a0` and tests it there, one instruction short of the target.
static __inline__ s16 take_request(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    if (work->field_52A == 1) {
        switch (work->field_52C) {
            case 1:
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x135);
                break;
            case 2:
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x3A0);
                break;
            case 3:
                SndEvt_EnqueueType7(0x551E0002, 1);
                work->field_52C = 0;
                set_state(task, 8);
                return 1;
            case 4:
                SndEvt_EnqueueType7(0x551E0002, 1);
                work->field_52C = 0;
                func_actor_206100_8014EB48(task, 0x3A0);
                set_state(task, 7);
                return 1;
            default:
                work->field_52C = 0;
                break;
        }
    }
    return 0;
}

/// State handler: consumes a pending sub-state request and, when there was
/// none, runs the current sub-state handler.  The request is handled through
/// the inlined `take_request`, so a request that moved the actor to another
/// state skips this frame's handler entirely.
void func_actor_206100_8014CFF4(Task* task)
{
    Actor206100Work* sub                 = (Actor206100Work*)task->work;
    void             (*states[2])(Task*) = {
        func_actor_206100_8014F6F8,
        func_actor_206100_8014D14C,
    };

    if (take_request(task) == 0) {
        states[(s16)sub->field_522](task);
    }
}
/// Sub-state 1 of `func_actor_206100_8014CFF4`'s table: ticks the per-state
/// counter `field_51E` and arms `field_52E` with 0x18 on the first frame, eases
/// `field_35C` toward 0x4000 by a quarter of the remaining distance over frames
/// 0x29..0x4D, fires the overlay's sound events -- 0x551E0002 panned through
/// `Gp_GetObjPan` / `Gp_GetObjDepth` at 0x54 and plain at 0x77 -- flags the six
/// cue frames, hands state 2 to the actor at sub-state 0 when its `flags_514`
/// say so, and folds the heading onto the vector from the root coordinate to
/// the walk target `field_4D0` / `field_4D4`, exactly as
/// `func_actor_206100_8014D380` does but with a step of 0xC and a deadband of
/// 0x18, before handing the actor to `func_actor_206100_8014ED3C` with step
/// 0x10.  The heading fold is that function's, instruction for instruction
/// once the constants are substituted -- including the `yaw` load after the
/// `jal` and the deadband as a variable rather than a literal.
///
/// Three details are load-bearing:
///
/// - the six cue frames are an `||` chain, not a `switch`: GCC 2.8.1 expands a
///   six-case switch over the 0x24-wide range 0x54..0x77 into a jump table
///   (`sltiu` + `jr` off a `.rodata` table, the shape the m2c seed produced),
///   where the chain stays the five `beq` the target has.  The 0x54 and 0x77
///   constants of the two sound blocks above are `CSE`'d into registers the
///   chain's own comparisons then reuse, which is why they live in callee-saved
///   `$s3` / `$s0` across the calls in between.
/// - the three reads of `task->work` are three separate variables.  A single
///   variable assigned in all three places is one pseudo with three
///   definitions, and `global_alloc` homes the whole of it in one callee-saved
///   register -- `$s0` for the flags test and the state change as well as the
///   tail, which is the register only the tail's load crosses calls for.
/// - the state change goes through the inlined `set_state`, the same reloading
///   helper `func_actor_206100_8014D8E8` calls.
void func_actor_206100_8014D14C(Task* task)
{
    Actor206100Work* sub = (Actor206100Work*)task->work;
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    u16              ease;
    s32              pan;
    s32              cond;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    sub->field_51E = sub->field_51E + 1;
    if ((s16)sub->field_51E == 1) {
        sub->field_52E = 0x18;
    }
    if ((u32)(sub->field_51E - 0x29) < 0x25U) {
        ease           = sub->field_35C;
        sub->field_35C = ease + ((s32)((0x4000 - (ease * 0x10)) << 0x10) >> 0x16);
    }
    if ((s16)sub->field_51E == 0x54) {
        pan = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(0x551E0002, pan,
                            (s8)Gp_GetObjDepth(((TmdObject*)task->extra)->coords));
    }
    if ((s16)sub->field_51E == 0x77) {
        SndEvt_EnqueueType7(0x551E0002, 1);
    }
    if ((s16)sub->field_51E == 0x54 || (s16)sub->field_51E == 0x5B || (s16)sub->field_51E == 0x62 ||
        (s16)sub->field_51E == 0x69 || (s16)sub->field_51E == 0x70 || (s16)sub->field_51E == 0x77) {
        sub->field_555 = 1;
    }
    next = (Actor206100Work*)task->work;
    if ((next->flags_514.half & 1) || (next->flags_514.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond != 0) {
        SndEvt_EnqueueType7(0x551E0002, 1);
        set_state(task, 2);
    }
    work       = (Actor206100Work*)task->work;
    coord      = ((TmdObject*)task->extra)->coords;
    coord->flg = 0;
    vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
    vec.vy     = 0;
    vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
    VectorNormalSS(&vec, &vec);
    yaw   = ratan2(vec.vx, vec.vz);
    limit = 0x18;
    angle = (u16)work->field_43E;
    diff  = ((angle - yaw) << 20) >> 20;
    if (diff > limit) {
        work->field_43E = angle - 0xC;
    } else if (diff < -0x18) {
        work->field_43E = angle + 0xC;
    }
    func_actor_206100_8014ED3C(task, 0x10);
}
INCLUDE_RODATA("actors/nonmatchings/actor_206100/actor_206100", D_actor_206100_80149E94);

/// The state-0 dispatcher's sub-state table, a table in its own right rather
/// than the local array `func_actor_206100_8014CFF4` builds -- the dispatcher
/// copies it whole, which is why the copy is a three-word block move out of
/// `.rodata`.  `func_actor_206100_8014D6F4` has the same body over the sibling
/// table `D_actor_206100_80149EB4`.
const TaskFuncTable3 D_actor_206100_80149EA8 = {
    {
        func_actor_206100_8014F738,
        func_actor_206100_8014D574,
        func_actor_206100_8014F770,
    },
};

/// The sibling table, immediately after `D_actor_206100_80149EA8` in
/// `.rodata`: the three sub-state handlers `func_actor_206100_8014D6F4`
/// dispatches between, the first `func_actor_206100_8014F7B4` and the third
/// `func_actor_206100_8014F878` bracketing the ring of debris
/// `func_actor_206100_8014D8E8` throws.
const TaskFuncTable3 D_actor_206100_80149EB4 = {
    {
        func_actor_206100_8014F7B4,
        func_actor_206100_8014D8E8,
        func_actor_206100_8014F878,
    },
};

/// The last object in this unit's `.rodata`, one object after
/// `D_actor_206100_80149EB4` and flush against the unit's first code address:
/// the four handlers `func_actor_206100_8014E7D4` dispatches between.  The cast
/// is the second argument its call passes, which the four one-argument handlers
/// ignore -- see `Actor206100StateTable4`.
const Actor206100StateTable4 D_actor_206100_80149EC0 = {
    {
        (Actor206100StateFunc)func_actor_206100_8014FBE4,
        (Actor206100StateFunc)func_actor_206100_8014FCD4,
        (Actor206100StateFunc)func_actor_206100_8014E964,
        (Actor206100StateFunc)func_actor_206100_8014FDE8,
    },
};

/// State handler of `D_actor_206100_80149E94`: consumes a pending sub-state
/// request through the inlined `take_request`, and when there was none runs the
/// current sub-state handler from `D_actor_206100_80149EA8` and then steers the
/// actor along its heading.
///
/// The steering half is the same fold `ActorsShared80139c00` makes: clear the
/// root coordinate's `flg`, take the XZ vector from the coordinate to the walk
/// target `field_4D0` / `field_4D4`, normalise it, and turn `field_43E` toward
/// `ratan2` of it by 0x18 a frame while the heading is more than a deadband off.
/// Three details are load-bearing:
///
/// - `yaw` is read from the call before `angle` is loaded, so the angle load
///   sits after the `jal` and never conflicts with the call-clobbered `$a0`;
///   with the load written first it is pushed into a callee-saved register and
///   every later value moves up one.
/// - the deadband is a variable, not the literal 0x28 -- which is what makes
///   `diff`'s comparison a register-register `slt` against a `li`'d `$v1`, the
///   same `li` + `slt` shape `Actor00400_TurnToward`'s range parameter forces.
///   Written as a literal the test becomes `slti $a0,0x29` + `bnez` instead:
///   the literal is folded away into `!(diff < 0x29)`, where a register operand
///   reaches `gen_int_relational`'s `reverse_regs` arm and its constant is
///   `force_reg`'d.  Only the first test is affected; the second one, written
///   against the same literal, stays an `slti`.
/// - `field_43E` is read through the `u16` view, so the load is `lhu` and its
///   zero-extension folds into the load; the field's own `s16` view is the `lh`
///   `func_actor_206100_8014ED3C` makes.
///
/// It ends by handing the actor to `func_actor_206100_8014ED3C` with step 0x14,
/// the walk that retires the actor back to `field_434` / `field_438` once it has
/// travelled far enough.
void func_actor_206100_8014D380(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EA8;
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->field_522](task);
        work       = (Actor206100Work*)task->work;
        coord      = ((TmdObject*)task->extra)->coords;
        coord->flg = 0;
        vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy     = 0;
        vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x18;
        } else if (diff < -0x28) {
            work->field_43E = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
void func_actor_206100_8014D574(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              sound;
    s32              pan;
    s32              i;
    s16              y;

    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E < 0x1E) {
        work->field_35C = work->field_35C + ((s32) - (work->field_35C << 0x14) >> 0x15);
        work->field_35E = work->field_35E + ((s32) - (work->field_35E << 0x14) >> 0x15);
    }
    y = -0x64;
    if ((s16)work->field_51E == 0x1E) {
        i     = 0;
        coord = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
        sound = (((u16)((GpEnemy*)task->spawnArg2)->field_8 >> 12) << 8) | 0x551E0006;
        pan   = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
        SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(((TmdObject*)task->extra)->coords));
        work->field_51E = 0;
        work->field_526 = 0x1E78;
        work->field_522 = work->field_522 + 1;
    }
}
/// State handler of the second table, `D_actor_206100_80149EB4`: consumes a
/// pending sub-state request through the inlined `take_request`, and when there
/// was none runs the current sub-state handler from that table and then steers
/// the actor along its heading, exactly as `func_actor_206100_8014D380` does
/// over `D_actor_206100_80149EA8`.
///
/// The body is that function's instruction for instruction -- the two are the
/// same source shape over different tables, so the prologue's `lui` / `addiu`
/// pair is the only thing that differs between them, and an edit to one belongs
/// in the other.  See `func_actor_206100_8014D380` for what the steering fold
/// does and for why the `yaw` load sits after the `jal` and the deadband is a
/// variable.
void func_actor_206100_8014D6F4(Task* task)
{
    Actor206100Work* sub    = (Actor206100Work*)task->work;
    TaskFuncTable3   states = D_actor_206100_80149EB4;
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              angle;
    s32              yaw;
    s32              limit;
    s32              diff;

    if (take_request(task) == 0) {
        states.funcs[(s16)sub->field_522](task);
        work       = (Actor206100Work*)task->work;
        coord      = ((TmdObject*)task->extra)->coords;
        coord->flg = 0;
        vec.vx     = sub->field_4D0 - (u16)coord->coord.t[0];
        vec.vy     = 0;
        vec.vz     = sub->field_4D4 - (u16)coord->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x28;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x18;
        } else if (diff < -0x28) {
            work->field_43E = angle + 0x18;
        }
        func_actor_206100_8014ED3C(task, 0x14);
    }
}
/// Sub-state 1 of `func_actor_206100_8014D6F4`'s table: the ring of debris the
/// death throes throw off, and the draw that decides whether the actor
/// teleports out of them.
///
/// On the seventh frame of the sub-state it splats 0x20 effect particles
/// (`Gp_SpawnEff` id 0x01202148, the same pair `func_actor_206100_8014D574`
/// fires at frame 0x1E) around the actor's root coordinate -- `rsin` / `rcos`
/// of `i << 7` shifted down by 3, so a ring of radius 0x1000 in 0x20 steps,
/// held at a constant y of -0x3E8.  `y` is a local rather than a literal in
/// the store because the whole ring shares the height, the same local
/// `func_actor_206100_8014D574` hoists.
///
/// From frame 0x1F on it draws from `Gp_LcgState`: the one-in-four that lands
/// on `(state >> 16) & 3 == 0` hands state 2 to the teleport
/// `func_actor_206100_8014CB68` at sub-state 0 -- so the actor leaves the
/// scene it is exploding in -- and the rest restart the counter and advance
/// to the next sub-state.  The state change goes through the inlined
/// `set_state`, which reloads `task->work` instead of reusing `work`: that
/// fresh load is what keeps the pointer a block-local quantity, exactly as in
/// `take_request`.
void func_actor_206100_8014D8E8(Task* task)
{
    Actor206100Work* work;
    GsCOORDINATE2*   coord;
    SVECTOR          vec;
    s32              i;
    s16              y;

    work            = (Actor206100Work*)task->work;
    work->field_51E = work->field_51E + 1;
    if ((s16)work->field_51E == 7) {
        y     = -0x3E8;
        i     = 0;
        coord = ((TmdObject*)task->extra)->coords;
        do {
            vec.vx = (u32)rsin(i << 7) >> 3;
            vec.vy = y;
            vec.vz = (u32)rcos(i << 7) >> 3;
            Gp_SpawnEff(D_80115738, coord, 0x01202148, &vec);
            i++;
        } while (i < 0x20);
    }
    if ((s16)work->field_51E >= 0x1F) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if (((Gp_LcgState >> 0x10) & 3) == 0) {
            set_state(task, 2);
            return;
        }
        work->field_51E = 0;
        work->field_522 = work->field_522 + 1;
    }
}
/// Push the model's second coordinate's world position onto `G_SCRATCH_HEAD`
/// and hand it to `Gp_UpdateActorColor`.  The body is `ActorsShared8013a2c0`'s,
/// inlined the way `Actor405800_UpdateColor` and `Actor400600_UpdateColor`
/// inline it -- and it has to stay an inlined copy.  Only while expanding an
/// inline body does cc1 keep the scratch head's absolute address folded into
/// the memory operand (`lw $a1,0x1F8003FC` / `sw $a1,0x1F8003FC`, which the
/// assembler expands to the `lui` + `%lo` pair); written out at the call site
/// the same statements materialise the address in a register instead, and the
/// three instructions that costs are the whole difference.
static __inline__ void Actor206100_UpdateColor(Task* task)
{
    GsCOORDINATE2* coord;
    void**         scratch;
    u8*            head;
    VECTOR*        block;

    coord     = &((TmdObject*)task->extra)->coords[1];
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    block     = (VECTOR*)(head - 0x10);
    block->vx = coord->workm.t[0];
    block->vy = coord->workm.t[1];
    block->vz = coord->workm.t[2];
    *scratch  = block;
    Gp_UpdateActorColor(task->spawnArg2, block, 0, 0);
    *scratch = (u8*)*scratch + 0x10;
}

/// State-2 tick: the `D_801153F4` effect mode 0 arm bumps the actor's two frame
/// counters and runs the handler `funcs[(s16)field_520]` picks out of a
/// two-entry local table, then drives the animation request and re-poses the
/// actor; mode 1 is that tail alone and mode 2 is the deferred-kill bit of the
/// model.  The table's entries are the ring stepper
/// `func_actor_206100_8014FAE4` and the companion tick
/// `func_actor_206100_8014DD3C`, which is the `field_520` index the spawn state
/// `func_actor_206100_8014C274` leaves at 0.
///
/// The table is two addresses materialised into `$v0`, not a block move out of
/// `.rodata` -- the head of the function's four `lui` / `addiu` / `sw` pairs
/// are the declaration initialiser.  They follow the two `Task` walks because
/// both of those are locals with initialisers as well, and a declaration's
/// initialiser is emitted where the declaration is.
///
/// From the request kind down the body is `func_actor_206100_8014E964`'s word
/// for word: the same `field_50C` re-arm / ramp / reset chain over a second
/// `task->work` load, and the same `for (i = 1; i < 0xF; i++)` slot tick whose
/// initialiser sits *after* the chain for the reason
/// `func_actor_206100_8014FCD4` documents.  The chain is the only reader of
/// `next`; everything else stays on `work`, which is why the two loads exist.
///
/// Both pose matrices write five words, and which of them land in a register is
/// load-bearing.  `matrix.ident.*` names the union's word view, so those three
/// stores stay frame-relative, while `*(s32*)&mtx->m[1][1]` and `mtx->m[2][2]`
/// reach the same words through the `mtx` pointer and so are `8($s0)` and
/// `0x10($s0)` off the local matrix's own address.  Written without `mtx` all
/// five are frame-relative and that address is never materialised -- the same
/// `rotation` / `mtx` split `func_actor_403100_801339EC` makes.
///
/// The nine stores onto the coordinate are the same split one level down:
/// through `dest` their address is a register plus a displacement, so the copy
/// is `4($s3)` followed by eight off `$v1`; naming `coord->coord.m[i][j]`
/// instead gives nine distinct sums and no register at all, and every store
/// comes out frame-relative.  `scale` is declared between the two matrices
/// because the frame slots are handed out in declaration order -- matrix /
/// scale / scaling is what puts them at 0x18, 0x38 and 0x48.
void func_actor_206100_8014DA28(Task* task)
{
    Actor206100Work* work               = (Actor206100Work*)task->work;
    TmdObject*       obj                = (TmdObject*)task->extra;
    void             (*funcs[2])(Task*) = {
        func_actor_206100_8014FAE4,
        func_actor_206100_8014DD3C,
    };
    Actor206100Work*  next;
    Actor206100Work*  sub;
    GsCOORDINATE2*    coord;
    GsCOORDINATE2*    scaled;
    MATRIX*           mtx;
    MATRIX*           mtx2;
    MATRIX*           dest;
    Actor206100Matrix matrix;
    VECTOR            scale;
    Actor206100Matrix scaling;
    s32               i;
    s16               state;

    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            work->flags_514.parts.field_516 = work->flags_514.parts.field_516 + 1;
            work->field_518                 = work->field_518 + 1;
            funcs[(s16)work->field_520](task);
            next  = (Actor206100Work*)task->work;
            state = next->field_50C;
            if (state == 1) {
                if (next->field_50E != next->field_510) {
                    next->field_512 = 0;
                } else {
                    next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
                }
                func_actor_206100_8014F2F0(task);
                next->field_50C = 3;
            } else if (state == 2) {
                func_actor_206100_8014F284(task);
                next->field_50C = 3;
                next->field_512 = 0;
            } else if (state == 3) {
                next->field_512 = next->field_512 + 1;
            }
            for (i = 1; i < 0xF; i++) {
                Gp_AnimTickIndex(&next->anim, i);
            }
            work->flags_514.parts.half = work->slots[1].field_10;
            func_actor_206100_8014B0AC(task, work->field_54D);
            coord                = ((TmdObject*)task->extra)->coords;
            sub                  = (Actor206100Work*)task->work;
            mtx                  = &matrix.mat;
            matrix.ident.m00_m01 = 0x1000;
            matrix.ident.m02_m10 = 0;
            *(s32*)&mtx->m[1][1] = 0x1000;
            matrix.ident.m20_m21 = 0;
            mtx->m[2][2]         = 0x1000;
            RotMatrixZ(sub->field_440, &matrix.mat);
            func_8004BFF8(sub->field_43E, &matrix.mat);
            dest                  = &coord->coord;
            dest->m[0][0]         = matrix.mat.m[0][0];
            dest->m[0][1]         = matrix.mat.m[0][1];
            dest->m[0][2]         = matrix.mat.m[0][2];
            dest->m[1][0]         = matrix.mat.m[1][0];
            dest->m[1][1]         = matrix.mat.m[1][1];
            dest->m[1][2]         = matrix.mat.m[1][2];
            dest->m[2][0]         = matrix.mat.m[2][0];
            dest->m[2][1]         = matrix.mat.m[2][1];
            dest->m[2][2]         = matrix.mat.m[2][2];
            coord->flg            = 0;
            scaled                = ((TmdObject*)task->extra)->coords;
            scale.vx              = work->field_53E;
            scale.vy              = scale.vx;
            scale.vz              = scale.vx;
            mtx2                  = &scaling.mat;
            scaling.ident.m00_m01 = 0x1000;
            scaling.ident.m02_m10 = 0;
            *(s32*)&mtx2->m[1][1] = 0x1000;
            scaling.ident.m20_m21 = 0;
            mtx2->m[2][2]         = 0x1000;
            ScaleMatrix(&scaling.mat, &scale);
            MulMatrix(&scaled->coord, &scaling.mat);
            /* fallthrough */
        case 1:
            Actor206100_UpdateColor(task);
            obj->flags &= 0xFF7F;
            return;
    }
}
/// Companion tick: holds the per-state counter at the explosion frame and then
/// fills and retires the actor's two companion slots (see the header for the
/// full walk -- each slot is handled on its own, and the fifth release moves
/// the actor to state 2).
///
/// Both slots are reached by *index* rather than through a walking pointer, and
/// that is what the target's register file depends on: a walked pointer makes
/// `timer`'s read and write two identical `DEST_ADDR` givs on the same biv,
/// which `combine_givs` merges into one that survives `strength_reduce`'s
/// "worth while" test -- so the second field is given an induction variable of
/// its own, `$s1` goes to it instead of to `work`, and the frame grows by a
/// slot.  Indexed, the two `timer` accesses are displacements off the address
/// register strength reduction builds for `enemy`, and neither is reduced.
/// See `DECOMPILATION_LEARNINGS.md`, "A walked pointer's second field becomes a
/// second induction variable".
///
/// The two loops carry their own counters for the same reason: one variable
/// used by both is a single pseudo whose live range spans both loops, so
/// local-alloc has to home it in a callee-saved register for the whole
/// function, where the target's second loop counts in `$a0`.
void func_actor_206100_8014DD3C(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GpEnemy*         enemy;
    s32              i;
    s32              j;
    u8               count;

    work = (Actor206100Work*)task->work;
    if ((s16)work->field_51E == 0x1E) {
        Gp_ArmStateF0(1);
    } else {
        work->field_51E = work->field_51E + 1;
    }
    func_actor_206100_8014DEAC(task);
    i = 0;
    do {
        if (work->field_551 < 5 && D_actor_206100_80158CBC[i].enemy == NULL) {
            if (D_actor_206100_80158CBC[i].timer == 0) {
                enemy = func_actor_206100_8014EE2C(work->field_551);
                if (enemy != NULL) {
                    D_actor_206100_80158CBC[i].enemy = enemy;
                    enemy->field_40                  = 1;
                    work->field_551                  = work->field_551 + 1;
                }
            } else {
                D_actor_206100_80158CBC[i].timer = D_actor_206100_80158CBC[i].timer - 1;
            }
        }
        i++;
    } while (i < 2);
    j = 0;
    do {
        if (D_actor_206100_80158CBC[j].enemy != NULL &&
            D_actor_206100_80158CBC[j].enemy->field_40 <= 0) {
            D_actor_206100_80158CBC[j].enemy = NULL;
            D_actor_206100_80158CBC[j].timer = 0xB4;
            count                            = work->field_552 + 1;
            work->field_552                  = count;
            if (count >= 5) {
                next            = (Actor206100Work*)task->work;
                task->state     = 2;
                next->field_520 = 0;
                next->field_522 = 0;
            }
        }
        j++;
    } while (j < 2);
}
/// Companion tick every state runs: while the flag `field_550` is up it ramps
/// the roll `field_440` by 0x20 a frame and clears the flag once the ramp lands
/// on a 0x1000 boundary, and it walks the actor along the eight-vertex ring
/// `field_4F4` the index `field_548` points into.  Reaching the vertex (the
/// planar distance below 0x3E8) advances `field_548` modulo 8 and the ring-step
/// counter `field_54F`, which resets after its sixth step and re-arms the roll;
/// otherwise it steers the yaw `field_43E` toward the vertex by 0x2C a frame
/// and hands the actor to `func_actor_206100_8014EA8C` for a 0x40 step.
///
/// The three diffs are written into the `delta` `SVECTOR` although only `vx`
/// and `vz` are read back -- the distance is planar, so `vy` is dead.  That
/// dead member is load-bearing: as scalar locals the three stay in registers
/// and their stores disappear, and it is the store boundaries of the *stack*
/// form that the target's four recomputed ring addresses hang off.
///
/// The else arm reads the ring through the `ring` / `index` pair instead of a
/// pointer to the entry.  With both operands in registers the address is a
/// register `plus` that survives the two `vec` stores, so the ring index and
/// base are loaded once; a pointer to the entry leaves `lbu field_548` /
/// `lw field_4F4` inside the address expression, which any store invalidates,
/// and it also puts the base in the `addu`'s first operand.  See
/// `DECOMPILATION_LEARNINGS.md`, "Array index vs intermediate pointer for
/// `addu` operand order".
void func_actor_206100_8014DEAC(Task* task)
{
    Actor206100Work*    sub = (Actor206100Work*)task->work;
    Actor206100Work*    work;
    GsCOORDINATE2*      coord;
    GsCOORDINATE2*      coord2;
    Actor206100RingPos* ring;
    u32                 index;
    SVECTOR             delta;
    SVECTOR             vec;
    u16                 roll;
    u8                  count;
    s32                 angle;
    s32                 yaw;
    s32                 diff;
    s32                 limit;

    coord = ((TmdObject*)task->extra)->coords;
    if (sub->field_54F == 0) {
        sub->field_550 = 1;
    }
    if (sub->field_550 == 1) {
        roll           = sub->field_440 + 0x20;
        sub->field_440 = roll;
        if ((roll & 0xFFF) == 0) {
            sub->field_550 = 0;
        }
    }
    delta.vx       = (u16)sub->field_4F4[sub->field_548].field_0 - (u16)coord->coord.t[0];
    delta.vy       = (u16)sub->field_4F4[sub->field_548].field_2 - (u16)coord->coord.t[1];
    delta.vz       = (u16)sub->field_4F4[sub->field_548].field_4 - (u16)coord->coord.t[2];
    sub->field_526 = (u16)sub->field_4F4[sub->field_548].field_2;
    if ((s16)SquareRoot0(delta.vx * delta.vx + delta.vz * delta.vz) < 0x3E8) {
        sub->field_548 = (sub->field_548 + 1) & 7;
        count          = sub->field_54F + 1;
        sub->field_54F = count;
        if (count >= 6) {
            sub->field_54F = 0;
        }
    } else {
        ring        = sub->field_4F4;
        index       = sub->field_548;
        work        = (Actor206100Work*)task->work;
        coord2      = ((TmdObject*)task->extra)->coords;
        coord2->flg = 0;
        vec.vx      = (u16)ring[index].field_0 - (u16)coord2->coord.t[0];
        vec.vy      = 0;
        vec.vz      = (u16)ring[index].field_4 - (u16)coord2->coord.t[2];
        VectorNormalSS(&vec, &vec);
        yaw   = ratan2(vec.vx, vec.vz);
        limit = 0x100;
        angle = (u16)work->field_43E;
        diff  = ((angle - yaw) << 20) >> 20;
        if (diff > limit) {
            work->field_43E = angle - 0x2C;
        } else if (diff < -0x100) {
            work->field_43E = angle + 0x2C;
        }
        func_actor_206100_8014EA8C(task, 0x40, sub->field_43E);
    }
}
/// Retarget tick: `func_actor_206100_8014EB48` arms the state to 1 with the
/// clip it wants in `field_544`, and this walks the three phases it takes to
/// get there -- fire the switch sound, ramp the phase `field_542` half the
/// remaining distance each frame and advance once it is within 0x20 of the
/// clip, then step the phase down by 0x10 a frame until it is back at zero,
/// where the phase snaps to 0 and the state to the idle below.
///
/// State 0 is that idle, and what a freshly spawned (zeroed) block sits in: the
/// leftover phase decays by an eighth of itself each frame.
///
/// The decay reads `field_542` twice and lands as an `lh` and an `lhu` pair,
/// with nothing in the C saying so: the multiplicand's sign is needed (it feeds
/// the shift) while the addend's high bits are dead, the sum going straight back
/// through `sh` into the same halfword.  The ramp in case 2, whose operands do
/// need full width, is where the `(u16)` casts are load-bearing.
void func_actor_206100_8014E0C0(Task* task)
{
    Actor206100Work* work;
    s32              sound;
    s32              pan;
    s16              phase;

    work = (Actor206100Work*)task->work;
    switch (work->field_554) {
        case 0:
            work->field_542 = work->field_542 + ((s32) - (work->field_542 * 0x10) >> 7);
            break;
        case 1:
            sound = (((u16)((GpEnemy*)task->spawnArg2)->field_8 >> 12) << 8) | 0x40040006;
            pan   = (s8)Gp_GetObjPan(((TmdObject*)task->extra)->coords);
            SndEvt_EnqueueType6(sound, pan, (s8)Gp_GetObjDepth(((TmdObject*)task->extra)->coords));
            work->field_554 = work->field_554 + 1;
            break;
        case 2:
            phase           = (u16)work->field_542 + ((s32)(((u16)work->field_544 - (u16)work->field_542) << 0x14) >> 0x15);
            work->field_542 = phase;
            if (phase < work->field_544 - 0x20) {
                break;
            }
            work->field_554 = work->field_554 + 1;
            break;
        case 3:
            phase           = (u16)work->field_542 - 0x10;
            work->field_542 = phase;
            if ((phase << 0x10) <= 0) {
                work->field_554 = 0;
                work->field_542 = 0;
            }
            break;
    }
}
INCLUDE_ASM("actors/nonmatchings/actor_206100/actor_206100", func_actor_206100_8014E228);

/// Effect-mode tick of the `field_520` state table `D_actor_206100_80149EC0`,
/// keyed on `D_801153F4`.  Mode 2 only sets the model's deferred-kill bit and
/// leaves; mode 0 runs the handler `field_520` selects, latches the animation
/// slot's flags into `flags_514` and eases the root coordinate -- x and z to a
/// sixteenth of their distance to zero, y the same fraction of the way to the
/// height `field_526` -- before falling into the shared tail; mode 1 is that
/// tail alone.
///
/// The table is copied onto the stack first, the same local jump table
/// `func_actor_206100_8014F524` builds, which is what the prologue's four-word
/// block move out of `.rodata` is.  The tail is `Actor206100_UpdateColor`; see
/// there for why it stays inline.
void func_actor_206100_8014E7D4(Task* task)
{
    Actor206100Work*       work;
    TmdObject*             obj;
    GsCOORDINATE2*         coord;
    Actor206100StateTable4 states;

    work   = (Actor206100Work*)task->work;
    obj    = (TmdObject*)task->extra;
    coord  = obj->coords;
    states = D_actor_206100_80149EC0;
    switch (D_801153F4) {
        case 2:
            obj->flags |= 0x80;
            return;
        case 0:
            states.funcs[(s16)work->field_520](task, &states);
            work->flags_514.half = work->slots[1].field_10;
            coord->coord.t[0]    = coord->coord.t[0] + (-coord->coord.t[0] >> 4);
            coord->coord.t[2]    = coord->coord.t[2] + (-coord->coord.t[2] >> 4);
            coord->coord.t[1] =
                coord->coord.t[1] + (((s16)work->field_526 - coord->coord.t[1]) >> 4);
            /* fallthrough */
        case 1:
            Actor206100_UpdateColor(task);
            return;
    }
}
/// Teleport-state tick: the same animation request chain
/// `func_actor_206100_8014FCD4` runs -- re-arm, ramp or reset the clip phase
/// `field_512` and tick every slot -- but on its own frame counter, and with the
/// tail this actor needs instead of that one's: `field_51E` reaching 0x32
/// rewinds it and steps the state index `field_520`, the frame the sub-state
/// table walks to pick the next handler.
///
/// `coord` is the actor's root coordinate, cleared so `GsGetLw` rebuilds it --
/// the same dereference-store local `func_actor_206100_8014FDE8` binds.
///
/// The loop initialiser sits after the sub-state chain for the reason
/// `func_actor_206100_8014FCD4` documents: ahead of it the store that
/// materialises `i` shares a block with the case-3 increment, post-reload CSE
/// folds that increment's `+ 1` into `+ $s0`, and the phase is written with
/// `addu`.  Here the branch targets the initialiser instead.
void func_actor_206100_8014E964(Task* task)
{
    Actor206100Work* work;
    Actor206100Work* next;
    GsCOORDINATE2*   coord;
    s32              i;
    s16              state;

    work            = (Actor206100Work*)task->work;
    coord           = ((TmdObject*)task->extra)->coords;
    work->field_51E = work->field_51E + 1;
    next            = (Actor206100Work*)task->work;
    state           = next->field_50C;
    if (state == 1) {
        if (next->field_50E != next->field_510) {
            next->field_512 = 0;
        } else {
            next->field_512 = func_actor_206100_8014F3C8(task, next->field_512);
        }
        func_actor_206100_8014F2F0(task);
        next->field_50C = 3;
    } else if (state == 2) {
        func_actor_206100_8014F284(task);
        next->field_50C = 3;
        next->field_512 = 0;
    } else if (state == 3) {
        next->field_512 = next->field_512 + 1;
    }
    for (i = 1; i < 0xF; i++) {
        Gp_AnimTickIndex(&next->anim, i);
    }
    coord->flg = 0;
    if ((s16)work->field_51E >= 0x32) {
        work->field_51E = 0;
        work->field_520 = work->field_520 + 1;
    }
}
void func_actor_206100_8014EA8C(Task* task, s16 arg1, s16 arg2)
{
    ((TmdObject*)task->extra)->coords->coord.t[0] += ((rsin(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->coords->coord.t[2] += ((rcos(arg2) << 4) * arg1) >> 16;
    ((TmdObject*)task->extra)->coords->flg         = 0;
}

void func_actor_206100_8014EB48(Task* task, s16 arg1)
{
    Actor206100Work* work = (Actor206100Work*)task->work;

    work->field_556 = 1;
    work->field_554 = 1;
    work->field_544 = arg1;
}

void func_actor_206100_8014EB60(Task* task)
{
    Actor206100Work*  work;
    GsCOORDINATE2*    coords;
    SVECTOR           rot;
    Actor206100Matrix matrix;
    MATRIX*           dest;
    MATRIX*           mtx;

    work   = (Actor206100Work*)task->work;
    coords = ((TmdObject*)task->extra)->coords;
    dest   = &coords[5].coord;
    mtx    = &matrix.mat;

    matrix.ident.m00_m01 = 0x1000;
    matrix.ident.m02_m10 = 0;
    *(s32*)&mtx->m[1][1] = 0x1000;
    matrix.ident.m20_m21 = 0;
    mtx->m[2][2]         = 0x1000;

    Gp_MtxToEuler(dest, &rot);
    rot.vx += work->field_540;
    RotMatrix(&rot, &matrix.mat);
    dest->m[0][0] = matrix.mat.m[0][0];
    dest->m[0][1] = matrix.mat.m[0][1];
    dest->m[0][2] = matrix.mat.m[0][2];
    dest->m[1][0] = matrix.mat.m[1][0];
    dest->m[1][1] = matrix.mat.m[1][1];
    dest->m[1][2] = matrix.mat.m[1][2];
    dest->m[2][0] = matrix.mat.m[2][0];
    dest->m[2][1] = matrix.mat.m[2][1];
    dest->m[2][2] = matrix.mat.m[2][2];
}

void func_actor_206100_8014EC54(Task* task)
{
    Actor206100Work* work = (Actor206100Work*)task->work;
    s16              value;
    s16              angle;

    switch (work->field_556) {
        case 0:
            work->field_540 = (u16)work->field_540 + ((-(work->field_540 * 0x10)) >> 7);
            return;
        case 1:
            work->field_556 = 2;
            return;
        case 2:
            value           = (u16)work->field_540 + ((-0x3000 - work->field_540 * 0x10) >> 6);
            work->field_540 = value;
            if (value < -0x2DF) {
                work->field_556++;
                return;
            }
            return;
        case 3:
            angle           = (u16)work->field_540 + 0x1C;
            work->field_540 = angle;
            if (angle >= 0) {
                work->field_556 = 0;
            }
            break;
    }
}
void func_actor_206100_8014ED3C(Task* task, s16 arg1)
{
    Actor206100Work*        work;
    GsCOORDINATE2*          coord;
    Actor206100DistScratch* head;
    Actor206100DistScratch* scratch;

    head                                      = *(Actor206100DistScratch**)G_SCRATCH_HEAD;
    scratch                                   = head - 1;
    *(Actor206100DistScratch**)G_SCRATCH_HEAD = scratch;
    work                                      = (Actor206100Work*)task->work;
    coord                                     = ((TmdObject*)task->extra)->coords;
    func_actor_206100_8014EA8C(task, arg1, work->field_43E);
    scratch->delta.vx = -(u16)coord->coord.t[0];
    scratch->delta.vz = -(u16)coord->coord.t[2];
    scratch->dist     = SquareRoot0(scratch->delta.vx * scratch->delta.vx +
                                    scratch->delta.vz * scratch->delta.vz);
    if (scratch->dist >= 0x191) {
        coord->coord.t[0] = work->field_434;
        coord->coord.t[2] = work->field_438;
    }
    *(Actor206100DistScratch**)G_SCRATCH_HEAD += 1;
}

extern TaskDesc D_80147E48;

GpEnemy* func_actor_206100_8014EE2C(s32 arg0)
{
    GpEnemy*   enemy;
    TmdObject* obj;

    enemy = Gp_SpawnEnemyFromTable(&D_80147E48, 0, 3, NULL);
    if (enemy != NULL) {
        enemy->field_8  = arg0 << 12;
        enemy->field_3C = &D_actor_206100_80155134[(s16)arg0];
        obj             = (TmdObject*)enemy->task->extra;
        obj->tpage      = 0;
        obj->clut       = 2;
        Tmd_ProcessStream(obj);
        Tmd_ProcessStream(obj);
        return enemy;
    }
    return NULL;
}

void func_actor_206100_8014EEC0(Task* task)
{
    Actor206100ChildWork* child;
    GpRec18*              rec;
    GsCOORDINATE2*        coord;

    child               = (Actor206100ChildWork*)task->work;
    coord               = ((TmdObject*)task->extra)->coords;
    task->killCountdown = 0;
    child->field_64     = 0x100;
    child->field_60     = 0;
    coord->sub          = &Gfx_ViewCoord;
    coord->flg          = 0;
    child->obj.field_18 = Gp_PackPair(&D_actor_206100_80155194, 0);
    child->obj.field_8  = ((TmdObject*)task->extra)->coords;
    rec                 = child->rec;
    child->obj.field_C  = rec;
    child->obj.field_10 = 0;
    child->obj.field_12 = 0;
    child->obj.field_14 = 0;
    child->obj.field_1C = 0x140;
    child->obj.flags    = 1;
    Gp_LinkObj(3, &child->obj);
    Gp_InitRec18Table(rec, 2, 0);
    child->obj.flags |= 0xC000;
    Gp_UpdateCoord(coord);
    func_actor_206100_8014A70C(coord, (u16)child->field_60, 0, child->field_64 + 0x10002000);
    task->state++;
}
