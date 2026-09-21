#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

extern s8  D_8007218A;
extern u8  D_80073BA9;
extern s32 D_actor_120300_80140910;
extern s32 D_actor_120300_80140A44;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140AFC;
extern s32 D_actor_120300_80140B14;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B44;
extern s32 D_actor_120300_80140B5C;
extern s32 D_actor_120300_80140B74;
extern s32 D_actor_120300_801416D4;
extern s32 D_actor_120300_801417AC;
extern s32 D_actor_120300_80141884;
extern s32 D_actor_120300_80141A34;

extern s16 D_actor_120300_80140980[];

extern TaskDesc ActorsShared80134898Desc;

s32 func_actor_120300_80131EE0(Task* arg0);

/// After `func_actor_120300_80131EE0`, runs the request at `field_4C8` (0..19):
/// most codes park an animation id in `field_4D4` and walk slots 1..19 through
/// `func_800B4114` or `Gp_AnimResetSlot`; a few also send message 0x7D4 or
/// change `field_4E0`. Case 1 phase 1 slides the model on X until
/// `coord.t[0] < 0xF3D`. Codes 0, 14, 19 and out-of-range clear `field_4C8`.
void func_actor_120300_80132C60(Task* arg0)
{
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    Actor120300Work* work;
    Actor120300Work* animWork;
    s32              i;
    s32              x;
    s32              msg;
    s32              n;
    s32              t;

    tmd   = (TmdObject*)arg0->extra;
    work  = (Actor120300Work*)arg0->work;
    coord = tmd->coords;
    func_actor_120300_80131EE0(arg0);
    switch ((u16)work->field_4C8) {
        case 1:
            switch ((u16)work->field_4CA) {
                case 0:
                    Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140AFC, 0);
                    i = 1;
                    n = 0x10;
                    SCHED_BARRIER();
                    animWork = (Actor120300Work*)arg0->work;
                    TOUCH_REG(animWork);
                    t = i;
                    TOUCH_REG(t);
                    animWork->field_4D4 = t;
                    do {
                        animWork->slots[(u16)i].rate = n;
                        Gp_AnimResetSlot(&animWork->anim, (u16)i, 1);
                        i++;
                    } while ((u16)i < 0x14U);
                    work->field_4CA = (u16)work->field_4CA + 1;
                    return;
                case 1:
                    x                 = coord->coord.t[0];
                    coord->flg        = 0;
                    x                -= 0x14;
                    coord->coord.t[0] = x;
                    if (x < 0xF3D) {
                        animWork            = (Actor120300Work*)arg0->work;
                        animWork->field_4D4 = 0xE;
                        SCHED_BARRIER();
                        i = 1;
                        n = 10;
                        do {
                            func_800B4114(&animWork->anim, (u16)i, 0xE, 0, n);
                            i++;
                        } while ((u16)i < 0x14U);
                        work->field_4C8 = 0;
                    }
                    return;
            }
            return;
        case 2:
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140B14, 0);
            work->field_4C8 = 0;
            return;
        case 3:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 4;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 4, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 4:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0x12;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0x12, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 5:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 6;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 6, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 6:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 7;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 7, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 7:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xD;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0xD, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 8:
            msg = (s32)&D_actor_120300_80140B2C;
            Gp_DispatchMsg(arg0, 0x7D4, msg, 0);
            Gp_DispatchMsg(work->field_4BC, 0x7D4, msg + 0x30, 0);
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 8;
            i                   = 1;
            do {
                animWork->slots[(u16)i].rate = 0x10;
                Gp_AnimResetSlot(&animWork->anim, (u16)i, 8);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 9:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xB;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0xB, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 10:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 9;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 9, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 11:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xA;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0xA, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 12:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xC;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0xC, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 13:
            work->field_4E0 = 0x400;
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140B74, 0);
            i                   = 1;
            n                   = 0x10;
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xE;
            do {
                animWork->slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->anim, (u16)i, 0xE);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 14:
            work->field_4E0 = 0x1000;
            break;
        case 15:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0xF;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0xF, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 16:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0x10;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 0x10, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 17:
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140B44, 0);
            i                   = 1;
            n                   = 0x10;
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 0x11;
            do {
                animWork->slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->anim, (u16)i, 0x11);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 18:
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140B2C, 0);
            i                   = 1;
            n                   = 0x10;
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 8;
            do {
                animWork->slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->anim, (u16)i, 8);
                i++;
            } while ((u16)i < 0x14U);
            work->field_4C8 = 0;
            return;
        case 19:
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = 5;
            SCHED_BARRIER();
            i = 1;
            n = 10;
            do {
                func_800B4114(&animWork->anim, (u16)i, 5, 0, n);
                i++;
            } while ((u16)i < 0x14U);
            break;
        case 0:
        default:
            break;
    }
    work->field_4C8 = 0;
}

/// Sets the actor up for play: clears the model's `field_C`, sends message
/// 0x7D5 to the actor and to the two slots at 0x4B8/0x4BC, and resets animation
/// slots 1..19 to the 8 it first parks in `field_4D4`.
/// `func_actor_120300_801337C4` calls it with 1 once flag nibble 0x2D is set; a
/// zero argument additionally hands the task at 0x4B4 the player-weapon record
/// (`GpRec14`, built from the equip-slot addend `D_80073BA9`), lifts
/// `field_4E0` to 0x1000 and drops the pending overlay replacement.  The
/// request codes at 0x4C0 and 0x4C8 are cleared either way, so any phase
/// counter armed alongside them restarts from the top.
void func_actor_120300_80133330(s32 arg0)
{
    Task*            task;
    Actor120300Work* work;
    Actor120300Work* animWork;
    SVECTOR          unused;
    GpRec14          rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task                             = (Task*)D_actor_120300_80141BA8;
    work                             = (Actor120300Work*)task->work;
    ((TmdObject*)task->extra)->flags = 0;
    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_120300_80140B2C, 0);

    animWork            = (Actor120300Work*)task->work;
    animWork->field_4D4 = 8;
    i                   = 1;
    do {
        animWork->slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 8);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_120300_80140B5C, 0);
    if (arg0 == 0) {
        weaponId     = D_80073BA9;
        id           = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
        rec.field_0  = id;
        rec.field_4  = 1;
        rec.field_8  = 0;
        rec.field_C  = 0;
        rec.field_10 = 1;
        Gp_DispatchMsg(work->field_4B4, 0x3E8, (s32)&rec, 0);
        Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_120300_80140ACC, 0);
        work->field_4E0 = 0x1000;
        CdCmd_CancelReplaceAndActivate();
    }
    work->field_4C0 = 0;
    work->field_4C8 = 0;
}

/// Tick for the two overlay-load phases the work block arms at 0x4DA: phase 0
/// walks the three-entry request list through 0x4D6 (0x416D4, then 0x417AC,
/// then 0x41884) before leaving through 0x4D8, while phase 1 issues the last
/// record 0x41A34 once and then only counts 0x4D8.  Each of the two phases
/// returns 1 while the session at `gGameSession->eventState` is still 0, so the
/// task that calls this keeps the actor alive until play starts.
s32 func_actor_120300_801334A4(Actor120300* arg0)
{
    Actor120300Work* work;

    work = arg0->field_1C;
    switch (work->field_4DA) {
        case 0:
            switch (work->field_4D8) {
                case 0:
                    switch (work->field_4D6) {
                        case 0:
                            func_800E8614((s32)&D_actor_120300_801416D4, 0);
                            work->field_4D6++;
                            break;
                        case 1:
                            func_800E8614((s32)&D_actor_120300_801417AC, 0);
                            work->field_4D6++;
                            break;
                        default:
                            func_800E8614((s32)&D_actor_120300_80141884, 0);
                            break;
                    }
                    work->field_4D8++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
        case 1:
            switch (work->field_4D8) {
                case 0:
                    func_800E8614((s32)&D_actor_120300_80141A34, 0);
                    work->field_4D8++;
                    break;
                case 1:
                    if (gGameSession->eventState == 0) {
                        return 1;
                    }
                    break;
            }
            break;
    }
    return 0;
}

/// Spawn tick: allocates the 0x4E4-byte `Actor120300Work` block, zeroes it and
/// parks it in `Task::work`, then wires the model object up -- `Tmd_AllocBuffers`,
/// the work block's light/colour matrices into `TmdObject::lightMtx` / `field_20`,
/// bit 2 of `TmdObject::flags` cleared and the animation context handed to
/// `func_800B3F84` along with the work block's slot array.
/// The texture page / CLUT row come from the placement record at the nested
/// area table's `field_0` list whose id matches neither 0xFF (end) nor 0x6A
/// (the skip marker).  Animation slots 1..19 are then re-armed with slot count
/// 0xE, and the two helper tasks the shared table's entries 2 and 3 spawn are
/// reparented under this one, which also lifts `field_4E0` to 0x1000.
void func_actor_120300_801335D8(Task* arg0)
{
    Actor120300Work* work;
    TaskIdMap*       map;
    Actor120300Work* animWork;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               id;
    s32              i;

    tmd        = arg0->extra;
    coord      = tmd->coords;
    map        = Mem_Malloc(0x4E4, 0);
    arg0->work = map;
    if (map == NULL) {
        taskKill(arg0);
        return;
    }
    work = (Actor120300Work*)map;
    Mem_Set(work, 0, 0x4E4);
    work->field_4B4         = gameGetPtrSlot(3);
    D_actor_120300_80141BA8 = (Actor120300*)arg0;
    coord->sub              = &gGfxViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->lightMtx = &work->field_474;
    tmd->colorMtx = &work->field_494;
    tmd->flags   &= 0xFFFB;
    place         = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    id            = place->entryId;
    while (id != 0xFF) {
        if (id == 0x6A) {
            break;
        }
        place++;
        id = place->entryId;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, &D_actor_120300_80140910, tmd, work->pad_334, work->slots);
    animWork            = (Actor120300Work*)arg0->work;
    animWork->field_4D4 = 0xE;
    i                   = 1;
    do {
        animWork->slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 0xE);
        i++;
    } while ((u16)i < 0x14U);
    work->field_4B8 = Task_SpawnFromTable(&ActorsShared80134898Desc, 2, 0, (s32)arg0);
    work->field_4BC = Task_SpawnFromTable(&ActorsShared80134898Desc, 3, 0, (s32)arg0);
    arg0->msgTable  = &D_actor_120300_80140A44;
    work->field_4E0 = 0x1000;
    Task_Reparent(arg0, work->field_4B8);
    Task_Reparent(arg0, work->field_4BC);
}
