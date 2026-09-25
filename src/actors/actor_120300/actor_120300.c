#include "common.h"
#include "gameplay/gameplay.h"

#include "actors/actor.h"
#include "actors/actors_shared_80133c6c.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

/// Work block this overlay hangs off `Task::work`; each pair at
/// 0x4C0 and 0x4C8 is a request code plus its phase counter, reset together.
/// `field_4DE` is a 0/1 latch: `func_actor_120300_80133E94` calls
/// `Gp_SpawnWeaponEff` while it is set, clears it, then hands
/// `Gp_MsgPlayerWeapon` the zero that follows.
///
/// 0x4B8/0x4BC are `Gp_DispatchMsg` targets, not state:
/// `func_actor_120300_80133D04` sends message 0x7D5 to the actor, to 0x4B8 and
/// to 0x4BC in turn.
///
/// The block opens with a 0x14-byte animation context and its twenty 0x28-byte
/// animation slots. `Mem_Malloc` is
/// asked for 0x4E4 bytes -- the whole block -- by
/// `func_actor_120300_80132004` and `func_actor_120300_801321C8`, while
/// `func_actor_120300_80133330` walks slots 1..19 through `Gp_AnimResetSlot`
/// after parking 8 in `field_4D4`, then lifts the scale at `field_4E0` to
/// 0x1000 once the actor is up.
typedef struct Actor120300Work {
    /* 0x000 */ ActorAnimRig20 rig;
    /* 0x474 */ MATRIX         field_474; // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX         field_494; // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*          field_4B4; // Gp_DispatchMsg target for msgs 0x3E8/0x3E9
    /* 0x4B8 */ Task*          field_4B8;
    /* 0x4BC */ Task*          field_4BC;
    /* 0x4C0 */ s16            field_4C0;
    /* 0x4C2 */ s16            field_4C2;
    /* 0x4C4 */ u16            field_4C4; // phase countdown; request 9 advances once it reaches 0x10
    /* 0x4C6 */ byte           pad_4C6[0x2];
    /* 0x4C8 */ s16            field_4C8;
    /* 0x4CA */ s16            field_4CA;
    /* 0x4CC */ byte           pad_4CC[0x6];
    /* 0x4D2 */ u16            field_4D2; // animation index sent with message 0x3F4
    /* 0x4D4 */ u16            field_4D4; // animation id, indexed into the -1-terminated table below; written by func_actor_120300_80133330
    /* 0x4D6 */ u16            field_4D6;
    /* 0x4D8 */ u16            field_4D8;
    /* 0x4DA */ u16            field_4DA;
    /* 0x4DC */ s16            field_4DC; // facing, copied to and from the player's aim yaw
    /* 0x4DE */ s16            field_4DE; // player-eff flag: Gp_SpawnWeaponEff
    /* 0x4E0 */ s16            field_4E0; // uniform scale: broadcast to all three axes of a ScaleMatrix vector, so 0x1000 is 1.0
    /* 0x4E2 */ byte           pad_4E2[0x2];
} Actor120300Work;
STATIC_ASSERT_SIZEOF(Actor120300Work, 0x4E4);

extern Task* D_actor_120300_80141BA8;

/// The actor's five-entry task table, spawned from by index. Entries 2 and 3
/// are the two tasks kept in `field_4B8`/`field_4BC`; entry 4 is the fade to
/// black.
extern TaskDesc D_actor_120300_80141B6C[];

/// `func_800B4114` is deliberately not declared in `gameplay/1BC.h` (see its
/// note): this overlay hands it a `u16` animation id, so the `s32` `arg2` here
/// is what keeps the caller's zero-extension.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s32            D_8017DD6C;
extern s32            D_8017DEA4;
extern s32            D_8017E1F4;
extern GpAreaApplyRec D_80180204;
extern GpAnimSet*     D_actor_120300_801408CC[];
extern s32            D_actor_120300_80140910;
extern s16            D_actor_120300_8014095C[];

/// Animation id per `Actor120300Work::field_4D4`; -1 skips the restart.
extern s16 D_actor_120300_80140980[];

extern s32 D_actor_120300_801409A8;
extern s32 D_actor_120300_801409C0;
extern s32 D_actor_120300_80140A20;
extern s32 D_actor_120300_80140A44;
extern s32 D_actor_120300_80140A6C;
extern s32 D_actor_120300_80140A84;
extern s32 D_actor_120300_80140A9C;
extern s32 D_actor_120300_80140ACC;
extern s32 D_actor_120300_80140AE4;
extern s32 D_actor_120300_80140AFC;
extern s32 D_actor_120300_80140B14;
extern s32 D_actor_120300_80140B2C;
extern s32 D_actor_120300_80140B44;
extern s32 D_actor_120300_80140B5C;
extern s32 D_actor_120300_80140B74;
extern s32 D_actor_120300_80140B94;
extern s32 D_actor_120300_80141524;
extern s32 D_actor_120300_801416D4;
extern s32 D_actor_120300_801417AC;
extern s32 D_actor_120300_80141884;
extern s32 D_actor_120300_8014195C;
extern s32 D_actor_120300_80141A34;

/// Fill part-1 translation and hand it to `func_800D7A9C`. `vec` is a
/// parameter rather than a local so its address stays out of the CSE class of
/// the `ScaleMatrix` argument that follows.
static inline void func_actor_120300_FillLight(Task* arg0, TmdObject* tmd, VECTOR* vec)
{
    vec->vx = tmd->coords[1].workm.t[0];
    vec->vy = arg0->extra.tmd->coords[1].workm.t[1];
    vec->vz = arg0->extra.tmd->coords[1].workm.t[2];
    func_800D7A9C(tmd, vec, 0, 3);
}

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
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    i    = 1;
    done = 1;
    for (; i < 0x14; i++) {
        if (!(work->rig.slots[i].flags & 0x100)) {
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
                func_800B4114(&animWork->rig.anim, i, anim, 0, 10);
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
    GpCoord*         coord;
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
        tmd        = arg0->extra.tmd;
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub             = ((Task*)arg0->spawnArg2)->extra.tmd->coords + 4;
            arg0->extra.tmd->flags = 0;
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
        Gp_SetTmdBytes(arg0->extra.tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
        arg0->state += 1;
    }
    tmd2     = arg0->extra.tmd;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = arg0->extra.tmd->coords->workm.t[1];
    vec.vz   = arg0->extra.tmd->coords->workm.t[2];
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
    GpCoord*         coord;
    TaskIdMap*       map;
    TmdObject*       tmd;
    s32              scale;
    s32              kill;
    s32              killCopy;
    u16              scaleRaw;
    TmdObject*       tmd2;
    Actor120300Work* work;

    if (arg0->state == 0) {
        tmd        = arg0->extra.tmd;
        coord      = tmd->coords;
        map        = Mem_Malloc(0x4E4, 0);
        arg0->work = map;
        if (map == NULL) {
            kill = 1;
        } else {
            work = (Actor120300Work*)map;
            Mem_Set(map, 0, 0x4E4);
            coord->sub             = ((Task*)arg0->spawnArg2)->extra.tmd->coords + 8;
            arg0->extra.tmd->flags = 0;
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
    tmd2     = arg0->extra.tmd;
    scaleRaw = ((Actor120300Work*)((Task*)arg0->spawnArg2)->work)->field_4E0;
    vec.vx   = tmd2->coords->workm.t[0];
    vec.vy   = arg0->extra.tmd->coords->workm.t[1];
    vec.vz   = arg0->extra.tmd->coords->workm.t[2];
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
/// record (`Player_Status.weapon`, with `Mc_SaveData.characterId` selecting the +1 or +0x22 block),
/// then clears the request. Cases 20 and 21 step the facing in `field_4DC`
/// and copy it onto the player's aim yaw.
///
/// Cases 13 and 18 keep the weapon-id locals block-scoped. One pseudo shared
/// by both cases schedules the `Player_Status.weapon` load ahead of the flag load.
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
    GpCoord*         actorCoord;
    GpCoord*         playerCoord;
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
                    msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                    w->field_4D2           = nextu;
                    msg.anim.field_4       = nextu;
                    msg.anim.field_8       = 1;
                    msg.anim.field_C       = 0xA;
                    msg.anim.field_10      = 1;
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
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                var_v0                 = 1;
                w->field_4D2           = 0;
                msg.anim.field_4       = 0;
                msg.anim.field_8       = 0;
                msg.anim.field_C       = 0;
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
            /* `D_actor_120300_80140A6C`, which sits 0x78 before `rec`: the ROM
               derives it from the address already in the register. */
            var_a2 = (s32)((u8*)rec - 0x78);
            goto dispatch;
        case 3:
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A6C, 0);
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 1;
                msg.anim.field_4       = 1;
                msg.anim.field_8       = 0;
                msg.anim.field_C       = 0;
                msg.anim.field_10      = 1;
                goto dispatch_work;
            }
            goto clear;
        case 4:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 2;
                msg.anim.field_4       = 2;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
                goto dispatch_work;
            }
            goto clear;
        case 5:
            Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A84, 0);
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 3;
                msg.anim.field_4       = 3;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
                goto dispatch_work;
            }
            goto clear;
        case 7:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 5;
                msg.anim.field_4       = 5;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
                goto dispatch_work;
            }
            goto clear;
        case 8:
            w      = (Actor120300Work*)arg0->work;
            var_a1 = 0x3F4;
            if (w->field_4B4 != NULL) {
                var_a2                 = (s32)&msg.anim;
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 6;
                msg.anim.field_4       = 6;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
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
                            var_a2                 = (s32)&msg.anim;
                            msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                            w->field_4D2           = 7;
                            msg.anim.field_4       = 7;
                            msg.anim.field_8       = 1;
                            msg.anim.field_C       = 0xA;
                            msg.anim.field_10      = 1;
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
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 8;
                msg.anim.field_4       = 8;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
                Gp_DispatchMsg(w->field_4B4, 0x3F4, (s32)&msg.anim, 0);
            }
            var_a0 = work->field_4B4;
            var_a1 = 0x3FD;
            var_a2 = 8;
            goto dispatch;
        case 11:
            Gp_DispatchMsg(((Actor120300Work*)arg0->work)->field_4B4, 0x3E9, (s32)&D_actor_120300_80140A9C, 0);
            w = (Actor120300Work*)arg0->work;
            if (w->field_4B4 != NULL) {
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 0xB;
                msg.anim.field_4       = 0xB;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
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
                msg.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2           = 9;
                msg.anim.field_4       = 9;
                msg.anim.field_8       = 1;
                msg.anim.field_C       = 0xA;
                msg.anim.field_10      = 1;
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
            weaponId                 = Player_Status.weapon;
            id                       = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            var_a1                   = 0x3E8;
            msg.anim.animBlock.index = id;
            msg.anim.field_4         = 1;
            msg.anim.field_8         = 0;
            msg.anim.field_C         = 0;
            msg.anim.field_10        = 0;
            var_a0                   = work->field_4B4;
            var_a2                   = (s32)&msg.anim;
            goto dispatch;
        }
        case 14:
            w = (Actor120300Work*)arg0->work;
            p = &msg2.anim;
            if (w->field_4B4 != NULL) {
                msg2.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2            = 0xF;
                p->field_4              = 0xF;
                p->field_8              = 1;
                p->field_C              = 0xA;
                p->field_10             = 1;
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
                msg2.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2            = 0xE;
                p->field_4              = 0xE;
                p->field_8              = 1;
                p->field_C              = 0xA;
                p->field_10             = 1;
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
                msg2.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2            = 0xD;
                p->field_4              = 0xD;
                p->field_8              = 1;
                p->field_C              = 0xA;
                p->field_10             = 1;
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
                msg2.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2            = 0xE;
                p->field_4              = 0xE;
                p->field_8              = 1;
                p->field_C              = 0xA;
                p->field_10             = 1;
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

            weaponId                  = Player_Status.weapon;
            id                        = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            var_a1                    = 0x3E8;
            msg2.anim.animBlock.index = id;
            msg2.anim.field_4         = 1;
            msg2.anim.field_8         = 1;
            msg2.anim.field_C         = 0xA;
            msg2.anim.field_10        = 0;
            var_a0                    = work->field_4B4;
            var_a2                    = (s32)&msg2.anim;
            goto dispatch;
        }
        case 19:
            w = (Actor120300Work*)arg0->work;
            p = &msg3.anim;
            if (w->field_4B4 != NULL) {
                var_a1                  = 0x3F4;
                msg3.anim.animBlock.ptr = D_actor_120300_801408CC;
                w->field_4D2            = 0x10;
                p->field_4              = 0x10;
                p->field_8              = 1;
                p->field_C              = 0xA;
                p->field_10             = 1;
                var_a2                  = (s32)p;
                goto dispatch_work;
            }
            goto clear;
        case 20:
            playerTask  = work->field_4B4;
            actorCoord  = arg0->extra.tmd->coords;
            playerCoord = playerTask->extra.tmd->coords;
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

/// After `func_actor_120300_80131EE0`, runs the request at `field_4C8` (0..19):
/// most codes park an animation id in `field_4D4` and walk slots 1..19 through
/// `func_800B4114` or `Gp_AnimResetSlot`; a few also send message 0x7D4 or
/// change `field_4E0`. Case 1 phase 1 slides the model on X until
/// `coord.t[0] < 0xF3D`. Codes 0, 14, 19 and out-of-range clear `field_4C8`.
void func_actor_120300_80132C60(Task* arg0)
{
    TmdObject*       tmd;
    GpCoord*         coord;
    Actor120300Work* work;
    Actor120300Work* animWork;
    s32              i;
    s32              x;
    s32              msg;
    s32              n;
    s32              t;

    tmd   = arg0->extra.tmd;
    work  = (Actor120300Work*)arg0->work;
    coord = tmd->coords;
    func_actor_120300_80131EE0(arg0);
    switch ((u16)work->field_4C8) {
        case 1:
            switch ((u16)work->field_4CA) {
                case 0:
                    Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120300_80140AFC, 0);
                    i        = 1;
                    n        = 0x10;
                    animWork = (Actor120300Work*)arg0->work;
                    TOUCH_REG(animWork);
                    t = i;
                    TOUCH_REG(t);
                    animWork->field_4D4 = t;
                    do {
                        animWork->rig.slots[(u16)i].rate = n;
                        Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 1);
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
                            func_800B4114(&animWork->rig.anim, (u16)i, 0xE, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 4, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0x12, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 6, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 7, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0xD, 0, n);
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
                animWork->rig.slots[(u16)i].rate = 0x10;
                Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 8);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0xB, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 9, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0xA, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0xC, 0, n);
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
                animWork->rig.slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 0xE);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0xF, 0, n);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 0x10, 0, n);
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
                animWork->rig.slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 0x11);
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
                animWork->rig.slots[(u16)i].rate = n;
                Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 8);
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
                func_800B4114(&animWork->rig.anim, (u16)i, 5, 0, n);
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
/// (`GpAnimArg`, built from the equip-slot addend `Player_Status.weapon`), lifts
/// `field_4E0` to 0x1000 and drops the pending overlay replacement.  The
/// request codes at 0x4C0 and 0x4C8 are cleared either way, so any phase
/// counter armed alongside them restarts from the top.
void func_actor_120300_80133330(s32 arg0)
{
    Task*            task;
    Actor120300Work* work;
    Actor120300Work* animWork;
    SVECTOR          unused;
    GpAnimArg        rec;
    s32              i;
    s32              weaponId;
    s32              id;

    task                   = D_actor_120300_80141BA8;
    work                   = (Actor120300Work*)task->work;
    task->extra.tmd->flags = 0;
    Gp_DispatchMsg(task, 0x7D4, (s32)&D_actor_120300_80140B2C, 0);

    animWork            = (Actor120300Work*)task->work;
    animWork->field_4D4 = 8;
    i                   = 1;
    do {
        animWork->rig.slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 8);
        i++;
    } while ((u16)i < 0x14U);

    Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
    Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_120300_80140B5C, 0);
    if (arg0 == 0) {
        weaponId            = Player_Status.weapon;
        id                  = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
        rec.animBlock.index = id;
        rec.field_4         = 1;
        rec.field_8         = 0;
        rec.field_C         = 0;
        rec.field_10        = 1;
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
s32 func_actor_120300_801334A4(Task* arg0)
{
    Actor120300Work* work;

    work = arg0->work;
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
    GpCoord*         coord;
    GpAreaPlace*     place;
    u8               id;
    s32              i;

    tmd        = arg0->extra.tmd;
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
    D_actor_120300_80141BA8 = arg0;
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
    func_800B3F84(&work->rig.anim, &D_actor_120300_80140910, tmd, work->rig.poses, work->rig.slots);
    animWork            = (Actor120300Work*)arg0->work;
    animWork->field_4D4 = 0xE;
    i                   = 1;
    do {
        animWork->rig.slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&animWork->rig.anim, (u16)i, 0xE);
        i++;
    } while ((u16)i < 0x14U);
    work->field_4B8 = Task_SpawnFromTable(D_actor_120300_80141B6C, 2, 0, (s32)arg0);
    work->field_4BC = Task_SpawnFromTable(D_actor_120300_80141B6C, 3, 0, (s32)arg0);
    arg0->msgTable  = &D_actor_120300_80140A44;
    work->field_4E0 = 0x1000;
    Task_Reparent(arg0, work->field_4B8);
    Task_Reparent(arg0, work->field_4BC);
}

/// Main tick of the cutscene actor. State 0 waits until no other cutscene is
/// up (`Gp_StateC08.field_A` / `gDisplayState.pendingMode`), builds the work block, then either arms
/// play (`func_actor_120300_80133330`) once flag nibble 0x2D is set or sends
/// the slot-3 weapon record and starts the script. States 1-4 step the area
/// records, the pending `Gp_TakePendingObj4C` cue, and the overlay-load
/// phases. Every path but the cutscene-busy early-out then ticks the two
/// animation helpers, draws the floor quad, and scales the model.
void func_actor_120300_801337C4(Task* arg0)
{
    union {
        struct {
            SVECTOR rot;
            VECTOR  vec;
        } draw;
        GpAnimArg rec;
    } scratch;
    Actor120300Work* work;
    Actor120300Work* temp;
    TmdObject*       tmd;
    s32              state;
    s32              weaponId;
    s32              scale;
    s16              ready;
    s32              take;
    u16              scaleRaw;
    u16              evtId;
    u8               evtKind;
    u8               evtSub;

    state = arg0->state;
    work  = (Actor120300Work*)arg0->work;
    switch (state) {
        case 0:
            if ((Gp_StateC08.field_A != 1) && (gDisplayState.pendingMode == 0)) {
                func_actor_120300_801335D8(arg0);
                work = (Actor120300Work*)arg0->work;
                if (GameFlag_GetNibble(0x2D) != 0) {
                    func_actor_120300_80133330(1);
                    if (GameFlag_GetNibble(0x2E) != 0) {
                        work->field_4D6 = 1;
                    }
                    arg0->state = 4;
                } else {
                    weaponId = Player_Status.weapon;
                    if (Mc_SaveData.characterId == 1) {
                        weaponId = weaponId + 1;
                    } else {
                        weaponId = weaponId + 0x22;
                    }
                    scratch.rec.animBlock.index = weaponId;
                    scratch.rec.field_4         = 1;
                    scratch.rec.field_8         = 0;
                    scratch.rec.field_C         = 0;
                    scratch.rec.field_10        = 0;
                    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&scratch.rec, 0);
                    GameFlag_SetNibble(0x2C, 1);
                    GameFlag_SetNibble(0x2D, 1);
                    func_800E3FAC(0xA2, 0xB);
                    func_800E8634((s32)&D_actor_120300_80140B94, 0, (s32)&D_actor_120300_80141524);
                    Mc_SaveData.sceneEvent = 2;
                    arg0->state           += 1;
                }
                Mem_CopyUnaligned(&D_actor_120300_801409A8, &D_8017DD6C, 0x18);
                Mem_CopyUnaligned(&D_actor_120300_80140A20, &D_8017E1F4, 0x24);
                Mem_CopyUnaligned(&D_actor_120300_801409C0, &D_8017DEA4, 0x60);
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Gp_ApplyAreaRecs(&D_80180204);
                arg0->state += 1;
            }
            break;
        case 2:
            ready = 0;
            temp  = (Actor120300Work*)arg0->work;
            if ((s16)Gp_TakePendingObj4C(&evtId, &evtKind, &evtSub) != 0) {
                if (!((s16)evtId & 0x8000)) {
                    if ((evtId & 0x7FFF) == 5) {
                        ready = Player_Status.field_24 != 0;
                    }
                }
            }
            if (ready != 0) {
                if ((s8)evtKind == 1) {
                    temp->field_4DA = 0;
                }
                if ((s8)evtKind == 2) {
                    temp->field_4DA = 1;
                }
                temp->field_4D8 = 0;
                take            = 1;
            } else {
                take = 0;
            }
            if (take != 0) {
                arg0->state += 1;
            }
            break;
        case 3:
            if ((s16)func_actor_120300_801334A4(arg0) != 0) {
                arg0->state -= 1;
            }
            break;
        case 4:
            Gp_DispatchMsg(work->field_4BC, 0x7D4, (s32)&D_actor_120300_80140B5C, 0);
            arg0->state = 2;
            break;
    }

    func_actor_120300_80132338(arg0);
    func_actor_120300_80132C60(arg0);
    scratch.draw.rot.vx = 0;
    scratch.draw.rot.vy = 0x380;
    scratch.draw.rot.vz = 0;
    Gp_DrawFloorQuad(&arg0->extra.tmd->coords[1], 0x300, &scratch.draw.rot);
    tmd      = arg0->extra.tmd;
    scaleRaw = work->field_4E0;
    func_actor_120300_FillLight(arg0, tmd, &scratch.draw.vec);
    scale               = scaleRaw & 0xFFFF;
    scratch.draw.vec.vz = scale;
    scratch.draw.vec.vy = scale;
    scratch.draw.vec.vx = scale;
    ScaleMatrix(tmd->colorMtx, &scratch.draw.vec);
}

/// Fade task, entry 4 of the actor's task table: darkens the screen to black.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the task kills itself.
void func_actor_120300_80133B5C(Task* arg0)
{
    OverlayFadeWork* work;
    OverlayFadeWork* alloc;

    work = (OverlayFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (OverlayFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            work         = alloc;
            work->b      = 0;
            work->g      = 0;
            work->r      = 0;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)work->r, (u8)work->g, (u8)work->r, 2);
            work->r += (u16)arg0->spawnArg1;
            work->g += (u16)arg0->spawnArg1;
            work->b += (u16)arg0->spawnArg1;
            if (work->r >= 0x100) {
                taskKill(arg0);
            }
            break;
    }
}

/// Message 0x7D5 handler: a nonzero `arg2` shows the task's model (clears
/// `TmdObject` flag 0x80), zero hides it. `arg1` is the message id.
void func_actor_120300_80133C38(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = task->extra.tmd;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}

/// Message 0x7D4 handler: places the task's model in the world. The model's
/// coordinate is parented to the view coordinate, takes `placement`'s
/// position as its translation and its rotation applied Y, then X, then Z.
/// `arg1` is the message id.
void func_actor_120300_80133C6C(Task* task, s32 arg1, GpXformArg* placement)
{
    GpCoord* coord;
    MATRIX*  mtx;

    coord             = task->extra.tmd->coords;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    mtx               = &coord->coord;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(mtx, placement->rot.vy, 1);
    Gfx_RotMatrixX(mtx, placement->rot.vx, 0);
    Gfx_RotMatrixZ(mtx, placement->rot.vz, 0);
    coord->flg = 0;
}

/// Broadcasts message 0x7D5 -- the visibility control the actor's display task
/// handles -- to the actor itself and to the two task slots on its work block.
/// Sending it is the whole body: `arg0` is the message's payload and only 0/1
/// are accepted.
void func_actor_120300_80133D04(s32 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (arg0 == 0) {
        Gp_DispatchMsg(D_actor_120300_80141BA8, 0x7D5, 0, 0);
        Gp_DispatchMsg(work->field_4B8, 0x7D5, 0, 0);
        Gp_DispatchMsg(work->field_4BC, 0x7D5, 0, 0);
    } else if (arg0 == 1) {
        Gp_DispatchMsg(D_actor_120300_80141BA8, 0x7D5, 1, 0);
        Gp_DispatchMsg(work->field_4B8, 0x7D5, 1, 0);
        Gp_DispatchMsg(work->field_4BC, 0x7D5, 1, 0);
    }
}

void func_actor_120300_80133DA4(void)
{
    CdCmd_EnqueueReplaceOverlay82();
    gGameSession->viewDirty = 1;
}

void func_actor_120300_80133DD4(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_actor_120300_80133DF4(void)
{
    Gp_RestoreStreamRng();
}

void func_actor_120300_80133E14(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120300_80133E34(s16 arg0)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

/// Requests the player-weapon effect be killed: latches `field_4DE` so the
/// call happens once, and `func_actor_120300_80133E94` consumes the latch.
void func_actor_120300_80133E54(void)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->field_4DE == 0) {
        work->field_4DE = 1;
        Gp_KillPlayerEffs();
    }
}

/// Runs the pending player-weapon effect and reports it: the latch at
/// `field_4DE` keeps it one-shot, and the `Gp_MsgPlayerWeapon` argument beside
/// the clear is the same zero.
void func_actor_120300_80133E94(void)
{
    Actor120300Work* work = D_actor_120300_80141BA8->work;

    if (work->field_4DE != 0) {
        Gp_SpawnWeaponEff();
        work->field_4DE = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

/// Spawns the fade task (entry 4 of the actor's task table) at rate 9.
void func_actor_120300_80133EE4(void)
{
    Task_SpawnFromTable(D_actor_120300_80141B6C, 4, 9, 0);
}

void func_actor_120300_80133F14(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            func_800E8614((s32)&D_actor_120300_8014195C, 0);
            arg0->state += 1;
            break;
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(arg0);
            }
            break;
    }
}
