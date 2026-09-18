#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"

extern s8  D_8007218A;
extern u8  D_80073BA9;
extern s32 D_actor_120300_80140910;
extern s32 D_actor_120300_80140A44;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B5C;
extern s32 D_actor_120300_801416D4;
extern s32 D_actor_120300_801417AC;
extern s32 D_actor_120300_80141884;
extern s32 D_actor_120300_80141A34;

/// Animation id per `Actor120300Work::field_4D4`; -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern TaskDesc ActorsShared80134898Desc;

/// Ticks slots 1..19 of a task's animation context and, if every one of them
/// then has `GpAnimSlot::field_10` bit 0x100 set, re-reads the work block and
/// restarts all twenty slots on the id `D_actor_120300_80140980` selects for
/// `field_4D4`, returning 1; a negative entry or an unset slot returns 0. The
/// gotos reproduce retail's block layout.
s32 func_actor_120300_80131EE0(Task* arg0)
{
    Actor120300Work* work;
    Actor120300Work* animWork;
    u16              anim;
    u16              i;
    u16              done;

    work = (Actor120300Work*)arg0->work;
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->slots[i].field_10 & 0x100)) {
            goto fail;
        }
    }
check:
    if (done) {
        if (D_actor_120300_80140980[work->field_4D4] >= 0) {
            anim                = D_actor_120300_80140980[work->field_4D4];
            animWork            = (Actor120300Work*)arg0->work;
            animWork->field_4D4 = anim;
            goto loop;
        fail:
            done = 0;
            goto check;
        loop:
            for (i = 1; i < 0x14; i++) {
                func_800B4114(&animWork->anim, i, anim, 0, 10);
            }
        }
        return 1;
    }
    return 0;
}

/// Spawn tick of a child actor that keeps the model facing the player: state 0
/// allocates the 0x4E4-byte `Actor120300Work` block, parks it in
/// `Task::work`, points the model's light and colour matrices at the block's
/// `field_474` / `field_494`, clears `TmdObject::field_C` and anchors the root
/// coordinate `sub` under part 4 of the spawning task's model
/// (`Task::spawnArg2->extra`); a failed allocation kills the task instead of
/// stepping to state 1. The texture page / CLUT row then come from the
/// placement record at the nested area table's `field_0` list whose id matches
/// neither 0xFF (end) nor 0x6A (the skip marker). Every tick after that reads
/// the parent work block's `field_4E0` and primes the colour matrix with the
/// root coordinate's own translation through `func_800D7A9C`, then replaces
/// that translation with the parent scale broadcast over all three axes and
/// folds it in with `ScaleMatrix`.
void func_actor_120300_80132004(Task* arg0)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GpAreaPlace*     place;
    s32              scale;
    s32              kill;
    s32              killCopy;
    u16              scaleRaw;
    TmdObject*       tmd2;
    Actor120300Work* work;
    u8               id;

    if (arg0->state == 0) {
        tmd        = arg0->extra;
        coord      = tmd->field_8;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                         = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8 + 4;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            Task_Kill(arg0);
            return;
        }
        place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->loc)->field_0;
        id    = place->field_0;
        while (id != 0xFF) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->field_0;
        }
        Gp_SetTmdBytes(arg0->extra, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->field_8->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->field_20, &vec);
}

/// Spawn tick of a child actor. State 0 allocates the 0x4E4-byte
/// `Actor120300Work` block, parks it in `Task::work`, points the model's
/// light and colour matrices at the block's `field_474` / `field_494`, clears
/// `TmdObject::field_C` and anchors the root coordinate `sub` under part 8 of
/// the spawning task's model (`Task::spawnArg2->extra`). A failed allocation
/// kills the task rather than stepping to state 1.
/// Every later tick reads the parent work block's `field_4E0` and primes the
/// colour matrix with the root coordinate's own translation through
/// `func_800D7A9C`, then replaces that translation with the parent scale
/// broadcast over all three axes and folds it in with `ScaleMatrix`.
void func_actor_120300_801321C8(Task* arg0)
{
    VECTOR           vec;
    GsCOORDINATE2*   coord;
    TaskIdMap*       map;
    TmdObject*       tmd;
    s32              scale;
    s32              kill;
    s32              killCopy;
    u16              scaleRaw;
    TmdObject*       tmd2;
    Actor120300Work* work;

    if (arg0->state == 0) {
        tmd        = arg0->extra;
        coord      = tmd->field_8;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                         = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->field_8 + 8;
            ((TmdObject*)arg0->extra)->field_C = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->field_1C  = &work->field_474;
            tmd->field_20  = &work->field_494;
            arg0->field_24 = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            Task_Kill(arg0);
            return;
        }
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->field_8->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->field_8->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->field_8->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->field_20, &vec);
}

INCLUDE_RODATA("actors/nonmatchings/actor_120300/actor_120300", D_actor_120300_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132338);

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_80132C60);

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

    task                               = (Task*)D_actor_120300_80141BA8;
    work                               = (Actor120300Work*)task->work;
    ((TmdObject*)task->extra)->field_C = 0;
    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_120300_80140B2C, 0);

    animWork            = (Actor120300Work*)task->work;
    animWork->field_4D4 = 8;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
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
/// the work block's light/colour matrices into `TmdObject::field_1C` / `field_20`,
/// bit 2 of `TmdObject::field_C` cleared and the animation context handed to
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
    coord      = tmd->field_8;
    map        = Mem_Malloc(0x4E4, 0);
    arg0->work = map;
    if (map == NULL) {
        Task_Kill(arg0);
        return;
    }
    work = (Actor120300Work*)map;
    Mem_Set(work, 0, 0x4E4);
    work->field_4B4         = Game_GetPtrSlot(3);
    D_actor_120300_80141BA8 = (Actor120300*)arg0;
    coord->sub              = &Gfx_ViewCoord;
    Tmd_AllocBuffers(tmd);
    tmd->field_1C = &work->field_474;
    tmd->field_20 = &work->field_494;
    tmd->field_C &= 0xFFFB;
    place         = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->loc)->field_0;
    id            = place->field_0;
    while (id != 0xFF) {
        if (id == 0x6A) {
            break;
        }
        place++;
        id = place->field_0;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, &D_actor_120300_80140910, (GpAnimObj*)tmd, work->pad_334, work->slots);
    animWork            = (Actor120300Work*)arg0->work;
    animWork->field_4D4 = 0xE;
    i                   = 1;
    do {
        animWork->slots[(u16)i].field_9 = 0x10;
        Gp_AnimResetSlot(&animWork->anim, (u16)i, 0xE);
        i++;
    } while ((u16)i < 0x14U);
    work->field_4B8 = Task_SpawnFromTable(&ActorsShared80134898Desc, 2, 0, (s32)arg0);
    work->field_4BC = Task_SpawnFromTable(&ActorsShared80134898Desc, 3, 0, (s32)arg0);
    arg0->field_24  = &D_actor_120300_80140A44;
    work->field_4E0 = 0x1000;
    Task_Reparent(arg0, work->field_4B8);
    Task_Reparent(arg0, work->field_4BC);
}

INCLUDE_ASM("actors/nonmatchings/actor_120300/actor_120300", func_actor_120300_801337C4);
