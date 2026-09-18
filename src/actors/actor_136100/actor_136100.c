#include "common.h"

#include "actors/actor_136100.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/task.h"
#include "psyq/libgpu.h"

extern u8       D_80071075;
extern s8       D_8007218A;
extern u8       D_80073BAC;
extern u8       D_80073BA9;
extern s32      D_801833F4;
extern s32      D_801834AC;
extern s32      D_80183ACC;
extern GpObj4A  D_801884AC;
extern GpObj4A  D_801884F8;
extern s32      D_actor_136100_8013F180[];
extern s32      D_actor_136100_8013F1A0;
extern s32      D_actor_136100_8013F1D4;
extern s16      D_actor_136100_8013F218[];
extern s32      D_actor_136100_8013F224;
extern s32      D_actor_136100_8013F244;
extern s32      D_actor_136100_8013F2C4;
extern s32      D_actor_136100_8013F2F4;
extern s32      D_actor_136100_8013F304[];
extern s32      D_actor_136100_8013F31C;
extern s32      D_actor_136100_8013F334[];
extern s32      D_actor_136100_8013F364;
extern s32      D_actor_136100_8013F37C;
extern s32      D_actor_136100_8013F394;
extern s32      D_actor_136100_8013F3AC;
extern s32      D_actor_136100_8013F3F4;
extern s32      D_actor_136100_8013F40C;
extern s32      D_actor_136100_8013F424;
extern s32      D_actor_136100_8013F43C;
extern s32      D_actor_136100_8013F454;
extern s32      D_actor_136100_8013F46C;
extern s32      D_actor_136100_8013F784;
extern s32      D_actor_136100_8013F94C;
extern s32      D_actor_136100_8013FAE4;
extern s32      D_actor_136100_8013FC64;
extern s32      D_actor_136100_8013FD84;
extern s32      D_actor_136100_80140114;
extern s32      D_actor_136100_801402C4;
extern s32      D_actor_136100_801404EC;
extern s32      D_actor_136100_8014063C;
extern Task*    D_actor_136100_8014078C;
extern TaskDesc ActorsShared80134898Desc;
extern s8       D_80114C12;

void func_actor_136100_80131EC4(void);
void func_actor_136100_80132748(Task* arg0);
void func_actor_136100_80133238(Task* arg0);
void func_actor_136100_80134A18(Task* task);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131EC4);

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80131FBC);

/// Spawn tick of the cutscene actor's second phase: allocates the 0x4F0-byte
/// `Actor136100Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- `Tmd_AllocBuffers`, `TmdObject::field_C` cleared, the
/// work block's light/colour matrices into `TmdObject::field_1C` / `field_20`
/// and the animation-context task reparented under `D_actor_136100_8014078C`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker), and this all runs even on the `Mem_Malloc` failure path,
/// which still advances the state after killing the task.
///
/// The dead `VECTOR` is read back through `arg0->extra` rather than the local
/// `obj`, which is what makes the original reload `Task::extra` for each of the
/// three coordinate reads (see `func_actor_136100_80132284`).
void func_actor_136100_801320E0(Task* arg0)
{
    Actor136100Work* work;
    VECTOR           vec;
    GpAreaPlace*     place;
    u8               id;

    if (arg0->state == 0) {
        TmdObject*     tmd   = arg0->extra;
        GsCOORDINATE2* coord = tmd->field_8;

        work       = Mem_Malloc(0x4F0, 0);
        arg0->work = (TaskIdMap*)work;
        if (work == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(work, 0, 0x4F0);
            coord->sub                         = (GsCOORDINATE2*)arg0->spawnArg2;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_136100_8013F2F4;
            Task_Reparent(D_actor_136100_8014078C, arg0);
        }
        place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
        id    = place->field_0;
        while (id != 0xFF) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->field_0;
        }
        Gp_SetTmdBytes((TmdObject*)arg0->extra, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    {
        TmdObject* obj = arg0->extra;

        Gp_UpdateCoord(obj->field_8);
        vec.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
        vec.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        vec.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

void func_actor_136100_80132284(Task* arg0)
{
    Actor136100Work* work;
    VECTOR           vec;

    if (arg0->state == 0) {
        TmdObject*     tmd   = arg0->extra;
        GsCOORDINATE2* coord = tmd->field_8;

        work       = Mem_Malloc(0x4F0, 0);
        arg0->work = (TaskIdMap*)work;
        if (work == NULL) {
            Task_Kill(arg0);
        } else {
            Mem_Set(work, 0, 0x4F0);
            coord->sub                         = (GsCOORDINATE2*)arg0->spawnArg2;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_136100_8013F2F4;
            Task_Reparent(D_actor_136100_8014078C, arg0);
        }
        arg0->state += 1;
        if (arg0->spawnArg1 != 0) {
            GsCOORDINATE2* reset = ((TmdObject*)arg0->extra)->field_8;

            Gfx_RotMatrixX(&reset->coord, 0x400, 1);
            reset->coord.t[1] = 0xC8;
            reset->flg        = 0;
        }
    }
    {
        TmdObject* obj = arg0->extra;

        Gp_UpdateCoord(obj->field_8);
        vec.vx = ((TmdObject*)arg0->extra)->field_8->workm.t[0];
        vec.vy = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
        vec.vz = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
        func_800D7A9C(obj, &vec, 0, 3);
    }
}

/// Build the 0x3E8 weapon record (`GpRec14`) for `anim` from the equip-slot
/// addend (`D_80073BA9`), arm `field_4DE` with it and send it to slot 3.
///
/// A macro rather than an inline: the record must be one frame slot shared by
/// every expansion, while the work pointer and the id stay per-expansion
/// pseudos -- shared, they globalise into one register across the switch.
#define func_actor_136100_SendWeaponRec(task, anim, a, b)                        \
    {                                                                            \
        Actor136100Work* msgWork;                                                \
        s32              weaponId;                                               \
        s32              id;                                                     \
                                                                                 \
        msgWork            = (Actor136100Work*)(task)->work;                     \
        weaponId           = D_80073BA9;                                         \
        id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22; \
        rec.field_0        = id;                                                 \
        msgWork->field_4DE = anim;                                               \
        rec.field_4        = anim;                                               \
        rec.field_8        = a;                                                  \
        rec.field_C        = b;                                                  \
        rec.field_10       = 0;                                                  \
        Gp_DispatchMsg(msgWork->field_4B4, 0x3E8, (s32) & rec, 0);               \
    }

/// Step the cutscene actor's `field_4C4` request.  Request 1 runs a three-step
/// sequence on `field_4C6` (send the 0x3E9 / 0x3F2 placement, wait for 0x3F0,
/// then wait six ticks on `field_4C8`) before sending the weapon record; 2..6
/// send it straight away with their own animation.  A finished request is
/// cleared.
void func_actor_136100_801323F8(Task* arg0)
{
    Actor136100Work* work = (Actor136100Work*)arg0->work;
    GpRec14          rec;

    if (Game_Session->field_1 != 0) {
        func_actor_136100_80131EC4();
    }
    switch ((u16)work->field_4C4) {
        case 0:
            break;
        case 1:
            switch ((u16)work->field_4C6) {
                case 0:
                    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F304[0], 0);
                    Gp_DispatchMsg(work->field_4B4, 0x3F2, (s32)&D_actor_136100_8013F304[6], 0);
                    work->field_4C8 = 0;
                    work->field_4C6++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_4B4, 0x3F0, 0, 0) == 0) {
                        work->field_4C6++;
                    }
                    return;
                case 2:
                    if (++work->field_4C8 < 6) {
                        return;
                    }
                    func_actor_136100_SendWeaponRec(arg0, 0x2F, 1, 5);
                    break;
                default:
                    return;
            }
            break;
        case 2:
            func_actor_136100_SendWeaponRec(arg0, 0x32, 1, 0xA);
            break;
        case 3:
            func_actor_136100_SendWeaponRec(arg0, 0x34, 1, 0xA);
            break;
        case 4:
            Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F37C, 0);
            func_actor_136100_SendWeaponRec(arg0, 0x2F, 0, 0);
            break;
        case 5:
            func_actor_136100_SendWeaponRec(arg0, 1, 1, 0xA);
            break;
        case 6:
            func_actor_136100_SendWeaponRec(arg0, 1, 0, 0);
            break;
    }
    work->field_4C4 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80132748);

/// Play `anim` on the `field_4C0` task (message 0x3F4) and record it as the
/// current `field_4E2` chain entry; does nothing while that task is unset.
static inline void func_actor_136100_PlayAnim(Task* task, u16 anim, s32 blend, s32 speed)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    GpAnimArg        msg;

    if (work->field_4C0 != NULL) {
        msg.field_0     = &D_actor_136100_8013F1D4;
        work->field_4E2 = anim;
        msg.field_4     = anim;
        msg.field_8     = blend;
        msg.field_C     = speed;
        msg.field_10    = 0;
        Gp_DispatchMsg(work->field_4C0, 0x3F4, (s32)&msg, 0);
    }
}

/// Advance the animation chain like `func_actor_136100_80133558`, then run the
/// `field_4D4` request: 1 and 3 play a fixed animation, 2 steps the
/// `field_4D6` sequence -- three sound-and-animation shots every 15 ticks
/// (`field_4DA` countdown, `field_4D8` shot count) before a final animation.
/// Every request that finishes clears `field_4D4`.
void func_actor_136100_80132BC0(Task* arg0)
{
    Actor136100Work* work;
    s16              anim;

    work = (Actor136100Work*)arg0->work;
    if (work->field_4C0 != NULL && Gp_DispatchMsg(work->field_4C0, 0x3ED, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->field_4E2];
        if (anim >= 0) {
            func_actor_136100_PlayAnim(arg0, anim, 1, 0xA);
        }
    }
    switch ((u16)work->field_4D4) {
        case 0:
            break;
        case 1:
            func_actor_136100_PlayAnim(arg0, 1, 0, 0);
            break;
        case 2:
            switch ((u16)work->field_4D6) {
                case 0:
                    work->field_4D8 = 0;
                    work->field_4DA = 0;
                    work->field_4D6++;
                    return;
                case 1:
                    if (--work->field_4DA > 0) {
                        return;
                    }
                    if (work->field_4D8 >= 3) {
                        func_actor_136100_PlayAnim(arg0, 1, 1, 0xA);
                        break;
                    }
                    SndEvt_EnqueueType6(0x40720009, 0, 0);
                    func_actor_136100_PlayAnim(arg0, 2, 1, 0xA);
                    work->field_4DA = 0xF;
                    work->field_4D8++;
                    return;
                default:
                    return;
            }
            break;
        case 3:
            Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F3F4, 0);
            func_actor_136100_PlayAnim(arg0, 0, 0, 0);
            break;
    }
    work->field_4D4 = 0;
}

/// Step the cutscene actor's second-phase `field_4C4` request, the sibling of
/// `func_actor_136100_801323F8`: request 2 runs the three-step `field_4C6`
/// sequence (0x3F3 / 0x3E9 / 0x3F2 placement, wait for 0x3F0, six ticks on
/// `field_4C8`), the others send the weapon record straight away.  A finished
/// request is cleared.
void func_actor_136100_80132E78(Task* arg0)
{
    Actor136100Work* work = (Actor136100Work*)arg0->work;
    GpRec14          rec;

    if (Game_Session->field_1 != 0) {
        func_actor_136100_80131EC4();
    }
    switch ((u16)work->field_4C4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_4B4, 0x3F3, 0, 0);
            func_actor_136100_SendWeaponRec(arg0, 1, 0, 0);
            break;
        case 2:
            switch ((u16)work->field_4C6) {
                case 0:
                    Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
                    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F334[0], 0);
                    Gp_DispatchMsg(work->field_4B4, 0x3F2, (s32)&D_actor_136100_8013F334[6], 0);
                    work->field_4C8 = 0;
                    work->field_4C6++;
                    return;
                case 1:
                    if (Gp_DispatchMsg(work->field_4B4, 0x3F0, 0, 0) == 0) {
                        work->field_4C6++;
                    }
                    return;
                case 2:
                    if (++work->field_4C8 < 6) {
                        return;
                    }
                    func_actor_136100_SendWeaponRec(arg0, 0x2F, 1, 5);
                    break;
                default:
                    return;
            }
            break;
        case 3:
            func_actor_136100_SendWeaponRec(arg0, 0x31, 1, 0xA);
            break;
        case 4:
            func_actor_136100_SendWeaponRec(arg0, 0x35, 1, 0xA);
            break;
        case 5:
            Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F364, 0);
            func_actor_136100_SendWeaponRec(arg0, 1, 0, 0);
            break;
        case 6:
            func_actor_136100_SendWeaponRec(arg0, 0x2F, 0, 0);
            break;
        case 7:
            func_actor_136100_SendWeaponRec(arg0, 1, 1, 0xA);
            break;
    }
    work->field_4C4 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_136100/actor_136100", func_actor_136100_80133238);

/// Advance the cutscene actor's animation chain and send its pending placement.
///
/// Once the `field_4C0` task reports its current animation done (message
/// 0x3ED), steps `field_4E2` to the next entry of the `D_actor_136100_8013F218`
/// chain (negative ends it) and plays it with 0x3F4.  Then sends the 0x3E9
/// placement selected by `field_4D4` (1..3) and clears the request.
void func_actor_136100_80133558(Task* arg0)
{
    Actor136100Work* work;
    Actor136100Work* msgWork;
    GpAnimArg        msg;
    u16              anim;

    work = (Actor136100Work*)arg0->work;
    if (work->field_4C0 != NULL && Gp_DispatchMsg(work->field_4C0, 0x3ED, 0, 0) == 0) {
        anim = D_actor_136100_8013F218[work->field_4E2];
        if (D_actor_136100_8013F218[work->field_4E2] >= 0) {
            msgWork = (Actor136100Work*)arg0->work;
            if (msgWork->field_4C0 != NULL) {
                msg.field_0        = &D_actor_136100_8013F1D4;
                msgWork->field_4E2 = anim;
                msg.field_4        = anim;
                msg.field_8        = 1;
                msg.field_C        = 0xA;
                msg.field_10       = 0;
                Gp_DispatchMsg(msgWork->field_4C0, 0x3F4, (s32)&msg, 0);
            }
        }
    }
    switch ((u16)work->field_4D4) {
        case 0:
            break;
        case 1:
            Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F424, 0);
            break;
        case 2:
            Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F43C, 0);
            break;
        case 3:
            Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F40C, 0);
            break;
    }
    work->field_4D4 = 0;
}

/// Reset the cutscene actor's animation state and re-send the weapon record.
///
/// Clears the first two value/countdown pairs, re-arms all nineteen animation
/// slots through `Gp_AnimResetSlot` with the work block's slot count at 1, then
/// sends slot 3 the 0x3E9 placement and the 0x3E8 weapon record
/// (`GpRec14`) built from the equip-slot addend (`D_80073BA9`), the pair
/// `func_actor_136100_8013467C` sends on its own.  `field_4DE` is armed on the
/// way past.
///
/// Three separate `task->work` loads are what the original reaches the block
/// with -- the stores to `field_4C4` / `field_4CC` invalidate the first in cse,
/// and the first is still live for the 0x3E9 send after the loop.  The dead
/// `SVECTOR` is not read; it reserves the 8-byte local the frame has between
/// the outgoing-arg area and `rec` (see `func_actor_136100_801347B8`).
void func_actor_136100_80133690(void)
{
    Task*            task;
    Actor136100Work* work;
    Actor136100Work* animWork;
    Actor136100Work* msgWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->work;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    animWork            = (Actor136100Work*)task->work;
    animWork->field_4E0 = 1;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_136100_8013F31C, 0);

    msgWork            = (Actor136100Work*)task->work;
    weaponId           = D_80073BA9;
    id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0        = id;
    msgWork->field_4DE = 1;
    rec.field_4        = 1;
    rec.field_8        = 0;
    rec.field_C        = 0;
    rec.field_10       = 0;
    Gp_DispatchMsg(msgWork->field_4B4, 0x3E8, (s32)&rec, 0);
}

/// Second half of the cutscene actor's re-arm: clears the first two
/// value/countdown pairs, sends the 0x7D4 cue to the host task, re-arms all
/// nineteen animation slots with the work block's slot count at 3, then re-sends
/// the two placement cues and the 0x3E8 weapon record (`GpRec14`) built from the
/// equip-slot addend (`D_80073BA9`).  `arg0 == 1` additionally resets the
/// fourth bone's rotation to zero.
///
/// Two `task->work` loads reach the block: the stores to `field_4C4` /
/// `field_4CC` invalidate the first in cse, and it is still live for the 0x3E9
/// and 0x3E9/`field_4C0` sends after the loop.  The dead `SVECTOR` is not read;
/// it reserves the 8-byte local the frame has between the outgoing-arg area and
/// `rec` (see `func_actor_136100_80133690`).
void func_actor_136100_8013379C(s32 arg0)
{
    Task*            task;
    Actor136100Work* work;
    Actor136100Work* animWork;
    Actor136100Work* msgWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task            = D_actor_136100_8014078C;
    work            = (Actor136100Work*)task->work;
    work->field_4C4 = 0;
    work->field_4CC = 0;

    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_136100_8013F3AC, 0);

    animWork            = (Actor136100Work*)task->work;
    animWork->field_4E0 = 3;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 3);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)D_actor_136100_8013F334, 0);

    msgWork            = (Actor136100Work*)task->work;
    weaponId           = D_80073BA9;
    id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    rec.field_0        = id;
    msgWork->field_4DE = 1;
    rec.field_4        = 1;
    rec.field_8        = 0;
    rec.field_C        = 0;
    rec.field_10       = 0;
    Gp_DispatchMsg(msgWork->field_4B4, 0x3E8, (s32)&rec, 0);

    Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F40C, 0);

    if (arg0 == 1) {
        Gfx_RotMatrixY(&((TmdObject*)task->extra)->field_8[4].coord, 0, 1);
    }
}

/// Cue handler: when the pending `Gp_TakePendingObj4C` event is a positive
/// id 5 (and `D_80073BAC` is set), kind 0x12 in phase 0 or kind 0x13 in phase 1
/// notifies via `func_actor_136100_80134A18` and plays the phase's first cue on
/// the first hit (`func_800E8634`, advancing `field_4DC`) or its repeat cue after.
/// `ready` must be `s16`: as `s32` the `!= 0` store fuses into the callee-saved
/// home and the join copy into `$v0` disappears.
s32 func_actor_136100_80133904(Task* task)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;
    s16              ready;

    ready = 0;
    if (Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
        if (!((s16)evtId & 0x8000)) {
            if ((evtId & 0x7FFF) == 5) {
                ready = D_80073BAC != 0;
            }
        }
    }
    if (ready == 0 || D_80114C12 == 1) {
        return 0;
    }
    if (D_80071075 != 0) {
        return 0;
    }
    if ((s8)evtKind == 0x12 && work->field_4E4 == 0) {
        func_actor_136100_80134A18(task);
        if (work->field_4DC == 0) {
            func_800E8634((s32)&D_actor_136100_8013F94C, 0, (s32)&D_actor_136100_8013FAE4);
            work->field_4DC++;
        } else {
            func_800E8614((s32)&D_actor_136100_8013FC64, 0);
        }
        return 1;
    }
    if ((s8)evtKind == 0x13 && work->field_4E4 == 1) {
        func_actor_136100_80134A18(task);
        if (work->field_4DC == 0) {
            func_800E8634((s32)&D_actor_136100_801402C4, 0, (s32)&D_actor_136100_801404EC);
            work->field_4DC++;
        } else {
            func_800E8614((s32)&D_actor_136100_8014063C, 0);
        }
        return 1;
    }
    return 0;
}

/// First tick of the cutscene actor: allocates the 0x4F0-byte
/// `Actor136100Work` block, zeroes it and parks it in `Task::work`, then wires
/// the model object up -- `Tmd_AllocBuffers`, the work block's light/colour
/// matrices into `TmdObject::field_1C` / `field_20`, bit 2 of `TmdObject::field_C`
/// cleared and the animation context handed to `func_800B3F84`.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker).
void func_actor_136100_80133A88(Task* arg0)
{
    Actor136100Work* work;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               id;

    tmd        = arg0->extra;
    coord      = tmd->field_8;
    map        = Mem_Malloc(0x4F0, 0);
    arg0->work = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor136100Work*)map;
    Mem_Set(work, 0, 0x4F0);
    work->field_4B4         = Game_GetPtrSlot(3);
    D_actor_136100_8014078C = arg0;
    coord->sub              = &Gfx_ViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->field_1C = &work->field_474;
    tmd->field_20 = &work->field_494;
    tmd->field_C &= 0xFFFB;
    place         = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&Game_Session->field_4)->field_0;
    id            = place->field_0;
    while (id != 0xFF) {
        if (id == 0x6A) {
            break;
        }
        place++;
        id = place->field_0;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, &D_actor_136100_8013F1A0, (GpAnimObj*)tmd, work->pad_334, work->slots);
    arg0->field_24 = &D_actor_136100_8013F2F4;
}

/// Classify the pending `Gp_TakePendingObj4C` event for the cutscene's start
/// cue: id 5 with kind 0x10 is 1 (phase 0), kind 0x11 is 2 (phase 1), anything
/// else 0.  The `s16` return is what keeps the result in its own pseudo, copied
/// into the caller's compare register after the join.
static inline s16 func_actor_136100_TakeStartCue(u16* evtId, u8* evtKind, u8* evtSub)
{
    if (Gp_TakePendingObj4C(evtId, evtKind, evtSub) != 0) {
        if ((*evtId & 0x7FFF) == 5) {
            if ((s8)*evtKind == 0x10) {
                return 1;
            }
            if ((s8)*evtKind == 0x11) {
                return 2;
            }
        }
    }
    return 0;
}

/// Re-arm animation slots 1..19 with slot count `count`
/// (`func_actor_136100_801347B8`'s loop, reaching the work block through `task`).
static inline void func_actor_136100_ResetSlots(Task* task, s32 count)
{
    Actor136100Work* work = (Actor136100Work*)task->work;
    s32              i;

    work->field_4E0 = count;
    i               = 1;
    do {
        work->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&work->anim, (u16)i, count);
        i++;
    } while ((u16)i < 0x14U);
}

/// Send the 0x3F4 animation record for `anim` to the `field_4C0` task, if any.
/// A macro: `anim` must stay a literal per expansion, or both constant loads
/// share one register and the two branches cross-jump earlier.
#define func_actor_136100_SendAnimRec(task, anim)                       \
    {                                                                   \
        Actor136100Work* animWork = (Actor136100Work*)(task)->work;     \
                                                                        \
        if (animWork->field_4C0 != NULL) {                              \
            rec.field_0         = (s32) & D_actor_136100_8013F1D4;      \
            animWork->field_4E2 = anim;                                 \
            rec.field_4         = anim;                                 \
            rec.field_8         = 0;                                    \
            rec.field_C         = 0;                                    \
            rec.field_10        = 0;                                    \
            Gp_DispatchMsg(animWork->field_4C0, 0x3F4, (s32) & rec, 0); \
        }                                                               \
    }

/// `func_actor_136100_80134A18`'s body over the shared `rec` slot: count the
/// live entries of `D_actor_136100_8013F180` and send them with message 0x3F7.
#define func_actor_136100_SendTable(task)                          \
    {                                                              \
        Actor136100Work* msgWork = (Actor136100Work*)(task)->work; \
        s32              n;                                        \
                                                                   \
        n = 0;                                                     \
        while (D_actor_136100_8013F180[n & 0xFFFF] != 0) {         \
            n += 1;                                                \
        }                                                          \
        rec.field_0 = (s32) & D_actor_136100_8013F180[0];          \
        rec.field_4 = n & 0xFFFF;                                  \
        Gp_DispatchMsg(msgWork->field_4B4, 0x3F7, (s32) & rec, 0); \
    }

/// Refresh the shadow coordinate and hand its translation to `func_800D7A9C`.
/// `vec` is a parameter rather than a local so the caller's buffer address
/// stays out of the CSE class of the `Gp_DrawFloorQuad` argument that follows.
static inline void func_actor_136100_UpdateShadow(Task* arg0, VECTOR* vec)
{
    TmdObject* obj = arg0->extra;

    Gp_UpdateCoord(&obj->field_8[1]);
    vec->vx = ((TmdObject*)arg0->extra)->field_8[1].workm.t[0];
    vec->vy = ((TmdObject*)arg0->extra)->field_8[1].workm.t[1];
    vec->vz = ((TmdObject*)arg0->extra)->field_8[1].workm.t[2];
    func_800D7A9C(obj, vec, 0, 3);
}

/// Main tick of the cutscene actor.  State 0 allocates the work block, picks
/// the phase (`field_4E4`, from game flag 0x73) and spawns the two helper tasks;
/// state 1 sends the phase's opening cues; state 2 waits for the matching start
/// cue, sends the weapon record and table and sets game flag 0x7C; states 3..5
/// wait on `Game_Session->field_1` and `func_actor_136100_80133904`.  Every
/// state then runs the phase's three per-frame handlers and redraws the shadow.
///
/// Every message record, the shadow `VECTOR` and the floor-quad `SVECTOR` share
/// the one `rec` frame slot; the dead `SVECTOR` reserves the 8-byte local below
/// it (see `func_actor_136100_80133690`).
void func_actor_136100_80133BC8(Task* arg0)
{
    Actor136100Work* work = (Actor136100Work*)arg0->work;
    SVECTOR          unused;
    GpRec14          rec;
    s32              cue;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;
    u16              evtId2;
    u8               evtKind2;
    u8               evtSub2;

    switch (arg0->state) {
        case 0:
            if (GameFlag_GetNibble(0x7C) != 0) {
                Task_Kill(arg0);
                return;
            }
            func_actor_136100_80133A88(arg0);
            work            = (Actor136100Work*)arg0->work;
            work->field_4E4 = GameFlag_GetNibble(0x73) == 0;
            work->field_4B8 = Task_SpawnFromTable(&ActorsShared80134898Desc, 2, 0,
                                                  (s32)((TmdObject*)arg0->extra)->field_8 + 0x140);
            if (work->field_4E4 == 0) {
                func_actor_136100_ResetSlots(arg0, 1);
                work->field_4BC = Task_SpawnFromTable(&ActorsShared80134898Desc, 3, 0, (s32)&Gfx_ViewCoord);
            } else {
                func_actor_136100_ResetSlots(arg0, 3);
                work->field_4BC = Task_SpawnFromTable(&ActorsShared80134898Desc, 3, 1,
                                                      (s32)((TmdObject*)arg0->extra)->field_8 + 0x280);
                Mem_CopyUnaligned(&D_actor_136100_8013F224, &D_801833F4, 0x20);
                Mem_CopyUnaligned(&D_actor_136100_8013F2C4, &D_80183ACC, 0x30);
                Mem_CopyUnaligned(&D_actor_136100_8013F244, &D_801834AC, 0x80);
            }
            work->field_4C0 = Game_GetPtrSlot(0xA);
            arg0->state++;
            break;
        case 1:
            if (work->field_4E4 == 0) {
                func_800E3FAC(0xA2, 0x19);
                Gp_DispatchMsg(arg0, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_136100_8013F454, 0);
                if (work->field_4C0 != NULL) {
                    Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F3F4, 0);
                }
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_136100_8013F394, 0);
                func_actor_136100_ResetSlots(arg0, 1);
                func_actor_136100_SendAnimRec(arg0, 1);
            } else {
                func_800E3FAC(0xA2, 0x1A);
                Gp_DispatchMsg(arg0, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
                Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
                if (work->field_4C0 != NULL) {
                    Gp_DispatchMsg(work->field_4C0, 0x3E9, (s32)&D_actor_136100_8013F40C, 0);
                }
                Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_136100_8013F3AC, 0);
                func_actor_136100_ResetSlots(arg0, 3);
                func_actor_136100_SendAnimRec(arg0, 5);
            }
            arg0->state++;
            break;
        case 2:
            cue = func_actor_136100_TakeStartCue(&evtId, &evtKind, &evtSub);
            if (cue == 1 && work->field_4E4 == 0) {
                if (D_80114C12 == 1) {
                    return;
                }
                if (D_80071075 != 0) {
                    return;
                }
                func_actor_136100_SendWeaponRec(arg0, 1, 1, 0xA);
                func_800E8634((s32)&D_actor_136100_8013F46C, 0, (s32)&D_actor_136100_8013F784);
                Gp_UnlinkObj4A(0, &D_801884AC);
                func_actor_136100_SendTable(arg0);
                GameFlag_SetNibble(0x7C, 1);
                arg0->state++;
                break;
            }
            if (func_actor_136100_TakeStartCue(&evtId2, &evtKind2, &evtSub2) == 2 && work->field_4E4 == 1) {
                if (D_80114C12 == 1) {
                    return;
                }
                if (D_80071075 != 0) {
                    return;
                }
                func_actor_136100_SendWeaponRec(arg0, 1, 1, 0xA);
                func_800E8634((s32)&D_actor_136100_8013FD84, 0, (s32)&D_actor_136100_80140114);
                Gp_UnlinkObj4A(0, &D_801884F8);
                func_actor_136100_SendTable(arg0);
                GameFlag_SetNibble(0x7C, 1);
                arg0->state++;
            }
            break;
        case 3:
            if (Game_Session->field_1 == 0) {
                arg0->state++;
            }
            break;
        case 4:
            if ((s16)func_actor_136100_80133904(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 5:
            if (Game_Session->field_1 == 0) {
                arg0->state--;
            }
            break;
    }
    if (work->field_4E4 == 0) {
        func_actor_136100_801323F8(arg0);
        func_actor_136100_80132748(arg0);
        func_actor_136100_80132BC0(arg0);
    } else {
        func_actor_136100_80132E78(arg0);
        func_actor_136100_80133238(arg0);
        func_actor_136100_80133558(arg0);
    }
    func_actor_136100_UpdateShadow(arg0, (VECTOR*)&rec);
    ((SVECTOR*)&rec)->vx = 0;
    ((SVECTOR*)&rec)->vy = 0x380;
    ((SVECTOR*)&rec)->vz = 0;
    Gp_DrawFloorQuad(&((TmdObject*)arg0->extra)->field_8[1], 0x300, (SVECTOR*)&rec);
}
