#include "common.h"

#include "actors/actor_120300.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/gfx.h"

extern s8         D_8007218A;
extern u8         D_80073BA9;
extern s32        D_actor_120300_80140910;
extern s32        D_actor_120300_80140A44;
extern s32        D_actor_120300_80140ACC;
extern GpAnimSet* D_actor_120300_801408CC[];
extern s16        D_actor_120300_8014095C[];
extern s32        D_actor_120300_80140A6C;
extern s32        D_actor_120300_80140A84;
extern s32        D_actor_120300_80140A9C;
extern s32        D_actor_120300_80140AE4;
extern s32        D_actor_120300_80140AFC;
extern s32        D_actor_120300_80140B14;
extern s32        D_actor_120300_80140B2C;
extern s32        D_actor_120300_80140B44;
extern s32        D_actor_120300_80140B5C;
extern s32        D_actor_120300_80140B74;
extern s32        D_actor_120300_801416D4;
extern s32        D_actor_120300_801417AC;
extern s32        D_actor_120300_80141884;
extern s32        D_actor_120300_80141A34;

/// Animation id per `Actor120300Work::field_4D4`; -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern TaskDesc ActorsShared80134898Desc;

/// Ticks slots 1..19 of a task's animation context and, if every one of them
/// then has `GpAnimSlot.flags` bit 0x100 set, re-reads the work block and
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
        if (!(work->slots[i].flags & 0x100)) {
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
/// `field_474` / `field_494`, clears `TmdObject::flags` and anchors the root
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
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                       = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords + 4;
            ((TmdObject*)arg0->extra)->flags = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            arg0->msgTable = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            taskKill(arg0);
            return;
        }
        place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
        id    = place->entryId;
        while (id != 0xFF) {
            if (id == 0x6A) {
                break;
            }
            place++;
            id = place->entryId;
        }
        Gp_SetTmdBytes(arg0->extra, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Spawn tick of a child actor. State 0 allocates the 0x4E4-byte
/// `Actor120300Work` block, parks it in `Task::work`, points the model's
/// light and colour matrices at the block's `field_474` / `field_494`, clears
/// `TmdObject::flags` and anchors the root coordinate `sub` under part 8 of
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
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub                       = ((TmdObject*)((Task*)arg0->spawnArg2)->extra)->coords + 8;
            ((TmdObject*)arg0->extra)->flags = 0;
            Tmd_AllocBuffers(tmd);
            kill           = 0;
            tmd->lightMtx  = &work->field_474;
            tmd->colorMtx  = &work->field_494;
            arg0->msgTable = &D_actor_120300_80140A44;
        }
        killCopy = kill;
        TOUCH_REG(killCopy);
        if (killCopy != 0) {
            taskKill(arg0);
            return;
        }
        arg0->state += 1;
    }
    tmd2     = arg0->extra;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = ((TmdObject*)arg0->extra)->coords->workm.t[1];
    vec.vz   = ((TmdObject*)arg0->extra)->coords->workm.t[2];
    func_800D7A9C(tmd2, &vec, 0, 3);
    scale  = scaleRaw & 0xFFFF;
    vec.vz = scale;
    vec.vy = scale;
    vec.vx = scale;
    ScaleMatrix(tmd2->colorMtx, &vec);
}

/// Request handler for the code latched in `field_4C0`. While the session
/// event state is set, a finished 0x3ED query advances `field_4D2` from
/// `D_actor_120300_8014095C` and restarts that animation. The switch sends
/// 0x3F4 animation payloads, 0x3E9 placement records and the 0x3E8 weapon
/// record (`D_80073BA9`, with `D_8007218A` selecting the +1 or +0x22 block),
/// then clears the request. Cases 20 and 21 step the facing in `field_4DC`
/// and copy it onto the player's aim yaw.
///
/// Cases 13 and 18 keep the weapon-id locals block-scoped. One pseudo shared
/// by both cases schedules the `D_80073BA9` load ahead of the flag load.
void func_actor_120300_80132338(Task* arg0)
{
    union {
        GpAnimArg anim;
        s32       words[6];
    } msg;
    union {
        GpAnimArg anim;
        s32       words[6];
    } msg2;
    union {
        GpAnimArg anim;
        s32       words[6];
    } msg3;
    Actor120300Work* work;
    Actor120300Work* w;
    GpAnimArg*       p;
    Task*            var_a0;
    s32              var_a1;
    s32              var_a2;
    s32              var_v0;
    Task*            playerTask;
    Task*            playerTask2;
    GameActor*       player;
    GameActor*       player2;
    GsCOORDINATE2*   actorCoord;
    GsCOORDINATE2*   playerCoord;
    s16*             nextp;
    s16              next;
    u16              nextu;
    s32              yaw;
    s32              absYaw;
    s16              target;
    s16              cur;
    u16              curu;
    s32              dx;
    s32              dz;
    s32              diff;
    u16              temp;
    s32*             rec;

    work = (Actor120300Work*)arg0->work;
    if (gGameSession->eventState != 0) {
        if ((work->field_4B4 != NULL) && (Gp_DispatchMsg(work->field_4B4, 0x3ED, 0, 0) == 0)) {
            if (D_actor_120300_8014095C[work->field_4D2] >= 0) {
                nextu = ((u16*)D_actor_120300_8014095C)[work->field_4D2];
                w     = (Actor120300Work*)arg0->work;
                if (w->field_4B4 != NULL) {
                    msg.anim.field_0  = D_actor_120300_801408CC;
                    w->field_4D2      = nextu;
                    msg.anim.field_4  = nextu;
                    msg.anim.field_8  = 1;
                    msg.anim.field_C  = 0xA;
                    msg.anim.field_10 = 1;
                    Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)&msg.anim, 0);
                }
            }
        }
    }
    switch ((u16)work->field_4C0) {
        case 0:
            goto clear;
        case 1:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2           = (s32)&msg.anim;
                msg.anim.field_0 = D_actor_120300_801408CC;
                var_v0           = 1;
                w->field_4D2     = 0;
                msg.anim.field_4 = 0;
                msg.anim.field_8 = 0;
                msg.anim.field_C = 0;
                SCHED_BARRIER();
                msg.anim.field_10 = var_v0;
                goto dispatch_work;
            }
            goto clear;
        case 2:
            rec = &D_actor_120300_80140AE4;
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)rec, 0);
            var_a1 = 0x3F2;
            var_a0 = ((Actor120300Work*)arg0->work)->field_4B4;
            var_a2 = (s32)((u8*)rec - 0x78);
            goto dispatch;
        case 3:
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A6C, 0);
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2            = (s32)&msg.anim;
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 1;
                msg.anim.field_4  = 1;
                msg.anim.field_8  = 0;
                msg.anim.field_C  = 0;
                msg.anim.field_10 = 1;
                goto dispatch_work;
            }
            goto clear;
        case 4:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2            = (s32)&msg.anim;
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 2;
                msg.anim.field_4  = 2;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                goto dispatch_work;
            }
            goto clear;
        case 5:
            Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A84, 0);
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2            = (s32)&msg.anim;
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 3;
                msg.anim.field_4  = 3;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                goto dispatch_work;
            }
            goto clear;
        case 7:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2            = (s32)&msg.anim;
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 5;
                msg.anim.field_4  = 5;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                goto dispatch_work;
            }
            goto clear;
        case 8:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2            = (s32)&msg.anim;
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 6;
                msg.anim.field_4  = 6;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                goto dispatch_work;
            }
            goto clear;
        case 9:
            switch ((u16)work->field_4C2) {
                case 0:
                    work->field_4C4 = 0;
                    work->field_4C2 = (u16)work->field_4C2 + 1;
                    return;
                case 1:
                    temp            = work->field_4C4 + 1;
                    work->field_4C4 = temp;
                    if ((u32)(temp & 0xFFFF) >= 0x10U) {
                        w      = (Actor120300Work*)arg0->work;
                        var_a1 = 0x3F4;
                        if (w->field_4B4 != NULL) {
                            var_a2            = (s32)&msg.anim;
                            msg.anim.field_0  = D_actor_120300_801408CC;
                            w->field_4D2      = 7;
                            msg.anim.field_4  = 7;
                            msg.anim.field_8  = 1;
                            msg.anim.field_C  = 0xA;
                            msg.anim.field_10 = 1;
                            goto dispatch_work;
                        }
                        goto clear;
                    }
                    return;
            }
            return;
        dispatch_work:
            var_a0 = w->field_4B4;
        dispatch:
            Gp_DispatchMsg(var_a0, var_a1, var_a2, 0);
            work->field_4C0 = 0;
            return;
        case 10:
            w = (Actor120300Work*)arg0->work;
            if (w->field_4B4 != NULL) {
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 8;
                msg.anim.field_4  = 8;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)&msg.anim, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 11:
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A9C, 0);
            w = (Actor120300Work*)arg0->work;
            if (w->field_4B4 != NULL) {
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 0xB;
                msg.anim.field_4  = 0xB;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)&msg.anim, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 12:
            w = (Actor120300Work*)arg0->work;
            if (w->field_4B4 != NULL) {
                msg.anim.field_0  = D_actor_120300_801408CC;
                w->field_4D2      = 9;
                msg.anim.field_4  = 9;
                msg.anim.field_8  = 1;
                msg.anim.field_C  = 0xA;
                msg.anim.field_10 = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)&msg.anim, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            var_a2 = 0x18;
            goto dispatch;
        case 13: {
            s32 weaponId;
            s32 id;

            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140ACC, 0);
            weaponId          = D_80073BA9;
            id                = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            var_a1            = 0x3E8;
            msg.anim.field_0  = (void*)id;
            msg.anim.field_4  = 1;
            msg.anim.field_8  = 0;
            msg.anim.field_C  = 0;
            msg.anim.field_10 = 0;
            var_a0            = work->field_4B4;
            var_a2            = (s32)&msg.anim;
            goto dispatch;
        }
        case 14:
            w = (Actor120300Work*)arg0->work;
            p = &msg2.anim;
            if (w->field_4B4 != NULL) {
                msg2.anim.field_0 = D_actor_120300_801408CC;
                w->field_4D2      = 0xF;
                p->field_4        = 0xF;
                p->field_8        = 1;
                p->field_C        = 0xA;
                p->field_10       = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)p, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 15:
            w = (Actor120300Work*)arg0->work;
            p = &msg2.anim;
            if (w->field_4B4 != NULL) {
                msg2.anim.field_0 = D_actor_120300_801408CC;
                w->field_4D2      = 0xE;
                p->field_4        = 0xE;
                p->field_8        = 1;
                p->field_C        = 0xA;
                p->field_10       = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)p, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 16:
            w = (Actor120300Work*)arg0->work;
            p = &msg2.anim;
            if (w->field_4B4 != NULL) {
                msg2.anim.field_0 = D_actor_120300_801408CC;
                w->field_4D2      = 0xD;
                p->field_4        = 0xD;
                p->field_8        = 1;
                p->field_C        = 0xA;
                p->field_10       = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)p, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 17:
            w = (Actor120300Work*)arg0->work;
            p = &msg2.anim;
            if (w->field_4B4 != NULL) {
                msg2.anim.field_0 = D_actor_120300_801408CC;
                w->field_4D2      = 0xE;
                p->field_4        = 0xE;
                p->field_8        = 1;
                p->field_C        = 0xA;
                p->field_10       = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)p, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            SCHED_BARRIER();
            var_a2 = 8;
            goto dispatch;
        case 18: {
            s32 weaponId;
            s32 id;

            weaponId           = D_80073BA9;
            id                 = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
            var_a1             = 0x3E8;
            msg2.anim.field_0  = (void*)id;
            msg2.anim.field_4  = 1;
            msg2.anim.field_8  = 1;
            msg2.anim.field_C  = 0xA;
            msg2.anim.field_10 = 0;
            var_a0             = work->field_4B4;
            var_a2             = (s32)&msg2.anim;
            goto dispatch;
        }
        case 19:
            w = (Actor120300Work*)arg0->work;
            p = &msg3.anim;
            if (w->field_4B4 != NULL) {
                var_a1            = 0x3F4;
                msg3.anim.field_0 = D_actor_120300_801408CC;
                w->field_4D2      = 0x10;
                p->field_4        = 0x10;
                p->field_8        = 1;
                p->field_C        = 0xA;
                p->field_10       = 1;
                var_a2            = (s32)p;
                goto dispatch_work;
            }
            goto clear;
        case 20:
            playerTask  = work->field_4B4;
            actorCoord  = ((TmdObject*)arg0->extra)->coords;
            playerCoord = ((TmdObject*)playerTask->extra)->coords;
            player      = (GameActor*)playerTask->work;
            switch ((u16)work->field_4C2) {
                case 0:
                    work->field_4DC = player->field_6A;
                    work->field_4C2 = (u16)work->field_4C2 + 1;
                    /* fallthrough */
                case 1:
                    if (actorCoord->coord.t[0] > playerCoord->coord.t[0]) {
                        dx = (u16)actorCoord->coord.t[0] - (u16)playerCoord->coord.t[0];
                        dz = (u16)playerCoord->coord.t[2] - (u16)actorCoord->coord.t[2];
                    } else {
                        dx = (u16)playerCoord->coord.t[0] - (u16)actorCoord->coord.t[0];
                        dz = (u16)actorCoord->coord.t[2] - (u16)playerCoord->coord.t[2];
                    }
                    target = ratan2((s16)dz, (s16)dx);
                    cur    = work->field_4DC;
                    diff   = target - cur;
                    if (diff < 0) {
                        diff = -diff;
                    }
                    if (diff < 0x31) {
                        work->field_4C2 = (u16)work->field_4C2 + 1;
                    } else if (cur < target) {
                        work->field_4DC = cur + 0x30;
                    } else {
                        work->field_4DC = cur - 0x30;
                    }
                    /* fallthrough */
                case 2:
                    player->field_6A = work->field_4DC;
                    return;
            }
            return;
        case 21:
            yaw         = work->field_4DC;
            playerTask2 = work->field_4B4;
            SOFT_TOUCH_REG(playerTask2);
            absYaw = yaw;
            if (yaw < 0) {
                SOFT_TOUCH_REG(absYaw);
                absYaw = -absYaw;
            }
            __asm__("" : "+m"(work->field_4DC));
            player2 = (GameActor*)playerTask2->work;
            curu    = (u16)work->field_4DC;
            if (absYaw < 0x31) {
                work->field_4DC = 0;
                work->field_4C0 = 0;
            } else if (yaw < 0) {
                work->field_4DC = curu + 0x30;
            } else {
                work->field_4DC = curu - 0x30;
            }
            player2->field_6A = work->field_4DC;
            return;
        default:
            goto clear;
    }
clear:
    work->field_4C0 = 0;
}
