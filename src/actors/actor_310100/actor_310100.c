#include "common.h"
#include "gameplay/1BC.h"

#include "actors/actor_310100.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_310100_801798E4;
extern TaskDesc D_actor_310100_801798F0;
extern TaskDesc D_actor_310100_801798FC;
extern TaskDesc D_actor_310100_80179920;
extern u32      D_actor_310100_80179754;
extern u32      D_actor_310100_80179794;
extern u32      D_actor_310100_801798B4;
extern u32      D_actor_310100_801797FC;
extern s16*     D_actor_310100_8017989C[];
extern s8       D_8007106B;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

s32 func_actor_310100_80161E24(Task* task)
{
    Actor310100Work* work;
    GpAnimRec*       rec;
    GpObj38*         obj;
    s32              i;
    u16              step;

    work = (Actor310100Work*)task->work;
    obj  = (GpObj38*)((TmdObject*)task->extra)->coords;
    rec  = Gp_AnimGetRec(&work->anim, &work->slots[1]);
    if (rec != work->field_4EC) {
        if (rec != NULL) {
            if (work->field_508 == 0x6C) {
                if (rec->flags & 0x20) {
                    SndEvt_EnqueueType6(D_actor_310100_801798A8[work->field_50A], Gp_GetObjPan(obj), 0);
                    step = work->field_50A;
                    if (step < 2U) {
                        work->field_50A = (u16)(step + 1);
                    }
                }
            } else {
                if (rec->flags & 0x20) {
                    SndEvt_EnqueueType6(0x51050006, Gp_GetObjPan(obj), 0);
                }
                if (rec->flags & 0x10) {
                    SndEvt_EnqueueType6(0x51050007, Gp_GetObjPan(obj), 0);
                }
            }
        }
        work->field_4EC = rec;
    }
    i = 1;
    do {
        Gp_AnimTickIndex(&work->anim, i & 0xFFFF);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    return work->slots[1].field_10 & 1;
}

void func_actor_310100_80161F80(Task* task)
{
    GpAnimArg        arg;
    Actor310100Work* work;
    Actor310100Work* anim;
    Actor310100Work* msg;
    Task*            player;
    u16              seed;
    u16              ok;
    s32              i;

    work = (Actor310100Work*)task->work;
    if (func_actor_310100_80161E24(task) & 0xFFFF) {
        seed = D_actor_310100_8017989C[work->field_4F8][work->field_4FA];
        if (D_actor_310100_8017989C[work->field_4F8][work->field_4FA] >= 0) {
            anim = (Actor310100Work*)task->work;
            i    = 1;
            do {
                func_800B4114(&anim->anim, i & 0xFFFF, seed & 0xFFFF, 0, 8);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
            work->field_4FA = seed;
        }
    }
    player = ((Actor310100Work*)task->work)->field_4E8;
    if (player == NULL || Gp_DispatchMsg(player, 0x3ED, 0, 0) == 0) {
        ok = 1;
    } else {
        ok = 0;
    }
    if (ok) {
        seed = D_actor_310100_8017989C[0][work->field_4F6];
        if (D_actor_310100_8017989C[0][work->field_4F6] >= 0) {
            msg = (Actor310100Work*)task->work;
            if (msg->field_4E8 != NULL) {
                arg.field_0  = &D_actor_310100_801797FC;
                arg.field_4  = seed;
                arg.field_8  = 1;
                arg.field_C  = 0xA;
                arg.field_10 = 1;
                Gp_DispatchMsg(msg->field_4E8, 0x3F4, (s32)&arg, 0);
            }
            work->field_4F6 = seed;
        }
    }
}

/// Spawn tick of the actor task, registered as its task state handler: states 1
/// and 2 — and state 0, which first parks `D_8007106B` at 2 — only step the
/// state, and state 3 spawns the display model. `spawnArg1` picks which one:
/// `D_actor_310100_80179920` with display id 0x6D, or `D_actor_310100_801798FC`
/// with 0x6C. It then walks the nested area place list for the record carrying
/// that id, drops the record's translation into the spawned model's root
/// coordinate frame, yaws that frame to the record's `field_A`, parks the
/// display work block's `field_4F0` at 0 and tears this task down.
void func_actor_310100_801620FC(Task* task)
{
    Actor310100Work* work;
    Actor310100Work* display;
    Task*            modelTask;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               mode;

    work = (Actor310100Work*)((Task*)task->spawnArg2)->work;
    switch (task->state) {
        case 0:
            D_8007106B = 2;
            /* fallthrough */
        case 1:
        case 2:
            task->state++;
            return;
        case 3:
            if (task->spawnArg1 == 0) {
                do {
                    mode = 0x6D;
                } while (0);
                work->field_4E4 = Task_SpawnOnDefaultList(&D_actor_310100_80179920, 1, work->field_506, 0);
            } else if (task->spawnArg1 == 1) {
                do {
                    do {
                        mode = 0x6C;
                    } while (0);
                } while (0);
                work->field_4E4 = Task_SpawnOnDefaultList(&D_actor_310100_801798FC, 1, work->field_506, 0);
            } else {
                goto skip;
            }
        skip:
            modelTask = work->field_4E4;
            place     = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            while (place->field_0 != 0xFF && place->field_0 != mode) {
                place++;
            }
            obj               = (TmdObject*)modelTask->extra;
            coord             = obj->coords;
            coord->coord.t[0] = place->field_4;
            coord->coord.t[1] = place->field_6;
            coord->coord.t[2] = place->field_8;
            Gfx_RotMatrixY(&coord->coord, place->field_A, 0);
            display            = (Actor310100Work*)work->field_4E4->work;
            display->field_4F0 = 0;
            taskKill(task);
            Display_ResetHeapWrapper();
            break;
    }
}

/// Second spawn tick of the actor task: states 1 and 2 — and state 0, which
/// first parks `D_8007106B` at 2 — only step the state, and state 3 spawns the
/// display model on the default list: `D_actor_310100_80179920` with display id
/// 0x6D for `spawnArg1` 0, `D_actor_310100_801798FC` with 0x6C for 1, both at
/// table index 2 with `arg2` 5 and 7. It then walks the nested area place list
/// for the record carrying that id, drops the record's translation into the
/// spawned model's root coordinate frame, yaws that frame to the record's
/// `field_A`, parks the display work block's `field_4F0` at 0 and tears this
/// task down.
void func_actor_310100_80162284(Task* task)
{
    Actor310100Work* work;
    Actor310100Work* display;
    Task*            modelTask;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u8               mode;

    work = (Actor310100Work*)((Task*)task->spawnArg2)->work;
    switch (task->state) {
        case 0:
            D_8007106B = 2;
            /* fallthrough */
        case 1:
        case 2:
            task->state++;
            return;
        case 3:
            if (task->spawnArg1 == 0) {
                do {
                    mode = 0x6D;
                } while (0);
                work->field_4E4 = Task_SpawnOnDefaultList(&D_actor_310100_80179920, 2, 5, 0);
            } else if (task->spawnArg1 == 1) {
                do {
                    do {
                        mode = 0x6C;
                    } while (0);
                } while (0);
                work->field_4E4 = Task_SpawnOnDefaultList(&D_actor_310100_801798FC, 2, 7, 0);
            } else {
                goto skip;
            }
        skip:
            modelTask = work->field_4E4;
            place     = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
            while (place->field_0 != 0xFF && place->field_0 != mode) {
                place++;
            }
            obj               = (TmdObject*)modelTask->extra;
            coord             = obj->coords;
            coord->coord.t[0] = place->field_4;
            coord->coord.t[1] = place->field_6;
            coord->coord.t[2] = place->field_8;
            Gfx_RotMatrixY(&coord->coord, place->field_A, 0);
            display            = (Actor310100Work*)work->field_4E4->work;
            display->field_4F0 = 0;
            taskKill(task);
            Display_ResetHeapWrapper();
            break;
    }
}

void func_actor_310100_80161F80(Task* task);

/// Spawns the display model for `D_actor_310100_801798FC`: allocates the 0x50C
/// work block into `task->work`, hands it the view coordinate and the two TMD
/// buffers, binds the animation set selected by the display id (0x6C or 0x6D),
/// seeds its 18 slots, points `task->msgTable` at `D_actor_310100_801798B4` and
/// applies the nested area record matching that id through `Gp_SetTmdBytes`.
void func_actor_310100_80162414(Task* task, s32 arg1)
{
    Actor310100Work* work;
    Actor310100Work* work2;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u16              mode;
    u16              active;
    u8               id;
    s32              i;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = (TmdObject*)task->extra;
    work       = (Actor310100Work*)Mem_Malloc(0x50C, false);
    mode       = arg1;
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->field_508 = arg1;
    Mem_Set(task->work, 0U, 0x50CU);
    work->field_4E8 = Game_GetPtrSlot(3);
    coord->sub      = &gGfxViewCoord;
    Tmd_AllocBuffers(obj);
    obj->lightMtx = &work->field_43C;
    obj->colorMtx = &work->field_45C;
    obj->flags    = 0;
    if (mode == 0x6C) {
        func_800B3F84(&work->anim, &D_actor_310100_80179754, (GpAnimObj*)obj, work->pad_30C,
                      &work->slots[0]);
    } else {
        func_800B3F84(&work->anim, &D_actor_310100_80179794, (GpAnimObj*)obj, work->pad_30C,
                      &work->slots[0]);
    }
    i      = 1;
    active = task->spawnArg1;
    work2  = (Actor310100Work*)task->work;
    do {
        work2->slots[i & 0xFFFF].field_9 = 0x10;
        Gp_AnimResetSlot(&work2->anim, i & 0xFFFF, active);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    func_actor_310100_80161F80(task);
    task->msgTable = &D_actor_310100_801798B4;
    id             = mode;
    place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    while (place->field_0 != 0xFF && place->field_0 != id) {
        place++;
    }
    Gp_SetTmdBytes(obj, (s8)place->field_D, (s8)place->field_E);
}

/// Common spawn of the two floor-quad display handlers: `func_actor_310100_801631B0`
/// passes display id 0x6C and `func_actor_310100_801632B0` 0x6D. Does the same
/// 0x50C work block setup as `func_actor_310100_80162414`, except it also parks
/// the task's `spawnArg1` in `field_504` — the argument `func_actor_310100_80162C64`
/// hands the display task it spawns — and reads it back as the payload the
/// eighteen animation slots are reset with.
void func_actor_310100_801625E4(Task* task, s32 arg1)
{
    Actor310100Work* work;
    Actor310100Work* work2;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    u16              mode;
    u16              active;
    u8               id;
    s32              i;

    coord      = ((TmdObject*)task->extra)->coords;
    obj        = (TmdObject*)task->extra;
    work       = (Actor310100Work*)Mem_Malloc(0x50C, false);
    mode       = arg1;
    task->work = (TaskIdMap*)work;
    if (work == NULL) {
        taskKill(task);
        return;
    }
    work->field_508 = arg1;
    Mem_Set(task->work, 0U, 0x50CU);
    work->field_4E8 = Game_GetPtrSlot(3);
    coord->sub      = &gGfxViewCoord;
    Tmd_AllocBuffers(obj);
    obj->lightMtx = &work->field_43C;
    obj->colorMtx = &work->field_45C;
    obj->flags    = 0;
    if (mode == 0x6C) {
        func_800B3F84(&work->anim, &D_actor_310100_80179754, (GpAnimObj*)obj, work->pad_30C,
                      &work->slots[0]);
    } else {
        func_800B3F84(&work->anim, &D_actor_310100_80179794, (GpAnimObj*)obj, work->pad_30C,
                      &work->slots[0]);
    }
    i               = 1;
    work->field_504 = task->spawnArg1;
    active          = work->field_504;
    work2           = (Actor310100Work*)task->work;
    do {
        work2->slots[i & 0xFFFF].field_9 = 0x10;
        Gp_AnimResetSlot(&work2->anim, i & 0xFFFF, active);
        i += 1;
    } while ((u32)(i & 0xFFFF) < 0x13U);
    func_actor_310100_80161F80(task);
    task->msgTable = &D_actor_310100_801798B4;
    id             = mode;
    place          = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    while (place->field_0 != 0xFF && place->field_0 != id) {
        place++;
    }
    Gp_SetTmdBytes(obj, (s8)place->field_D, (s8)place->field_E);
}

/// Controller for the display model spawned from `D_actor_310100_801798FC`.
/// Plaza ambience state 3 (`CdCmd_Queue.field_1F8`) forces the task into state
/// 3, where it idles. Otherwise state 0 allocates the 0x50C work block, and
/// states 1 and 2 toggle the model on the plaza stream slot: it is shown while
/// the ambience state is 0/1 with slot `field_1EE` at 0xE6 or above, or state 2
/// with the slot outside 7..0x6B. State 1 spawns it at the area place with
/// id 0x6C; state 2 kills it again once that condition drops.
void func_actor_310100_801627BC(Task* task)
{
    Actor310100Work* work;
    Actor310100Work* work2;
    GpAreaPlace*     place;
    GsCOORDINATE2*   coord;
    Task*            child;
    u16              st;
    u16              on;

    st = CdCmd_Queue.field_1F8;
    if (st == 3) {
        task->state = st;
    }
    switch (task->state) {
        case 0:
            task->work = (TaskIdMap*)Mem_Malloc(0x50C, false);
            if (task->work == NULL) {
                Gp_DestroyEnemy(task->spawnArg2, task);
                return;
            }
            task->msgTable = &D_actor_310100_801798B4;
            task->state++;
            break;
        case 1:
            on = 1;
            if (CdCmd_Queue.field_1F8 < 2U) {
                on = CdCmd_Queue.field_1EE >= 0xE6U;
            }
            if (CdCmd_Queue.field_1F8 == 2 && (u32)(CdCmd_Queue.field_1EE - 7) < 0x65U) {
                on = 0;
            }
            if (on) {
                work  = (Actor310100Work*)task->work;
                place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
                while (place->field_0 != 0xFF && place->field_0 != 0x6C) {
                    place++;
                }
                child             = Task_SpawnFromTable(&D_actor_310100_801798FC, 2, 1, 0);
                work->field_4E4   = child;
                coord             = ((TmdObject*)child->extra)->coords;
                coord->coord.t[0] = place->field_4;
                coord->coord.t[1] = place->field_6;
                coord->coord.t[2] = place->field_8;
                Gfx_RotMatrixY(&coord->coord, place->field_A, 0);
                task->state++;
            }
            break;
        case 2:
            on = 1;
            if (CdCmd_Queue.field_1F8 < 2U) {
                on = CdCmd_Queue.field_1EE >= 0xE6U;
            }
            if (CdCmd_Queue.field_1F8 == 2 && (u32)(CdCmd_Queue.field_1EE - 7) < 0x65U) {
                on = 0;
            }
            if (!on) {
                work2 = (Actor310100Work*)task->work;
                task->state--;
                taskKill(work2->field_4E4);
                work2->field_4E4 = NULL;
            }
            break;
    }
}

/// Controller for the display model spawned from `D_actor_310100_80179920`
/// (id 0x6D), the counterpart of `func_actor_310100_801627BC`. State 0 allocates
/// the 0x50C work block and seeds the display task's `spawnArg1` (`field_504`)
/// with 0x18. The model is shown while the plaza ambience state is 0/1 with slot
/// `field_1EE` in 0x4B..0xC3; states 2, 4 and 5 hide it. On hiding, state 2 keeps
/// the display task's `field_504` before killing it.
void func_actor_310100_801629FC(Task* task)
{
    Actor310100Work* work;
    GpAreaPlace*     place;
    GsCOORDINATE2*   coord;
    Task*            child;
    u16              st;
    u16              on;

    work = (Actor310100Work*)task->work;
    st   = CdCmd_Queue.field_1F8;
    if (st == 3) {
        task->state = st;
    }
    switch (task->state) {
        case 0:
            task->work = (TaskIdMap*)(work = Mem_Malloc(0x50C, false));
            if (work == NULL) {
                Gp_DestroyEnemy(task->spawnArg2, task);
                return;
            }
            task->msgTable  = &D_actor_310100_801798B4;
            work->field_504 = 0x18;
            task->state++;
            break;
        case 1:
            on = 1;
            if (CdCmd_Queue.field_1F8 < 2U) {
                on = (u32)(CdCmd_Queue.field_1EE - 0x4B) < 0x79U;
            }
            if (CdCmd_Queue.field_1F8 == 2) {
                on = 0;
            }
            if (CdCmd_Queue.field_1F8 == 4) {
                on = 0;
            }
            if (CdCmd_Queue.field_1F8 == 5) {
                on = 0;
            }
            if (on) {
                place = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
                while (place->field_0 != 0xFF && place->field_0 != 0x6D) {
                    place++;
                }
                child             = Task_SpawnFromTable(&D_actor_310100_80179920, 2, work->field_504, 0);
                work->field_4E4   = child;
                coord             = ((TmdObject*)child->extra)->coords;
                coord->coord.t[0] = place->field_4;
                coord->coord.t[1] = place->field_6;
                coord->coord.t[2] = place->field_8;
                Gfx_RotMatrixY(&coord->coord, place->field_A, 0);
                task->state++;
            }
            break;
        case 2:
            on = 1;
            if (CdCmd_Queue.field_1F8 < 2U) {
                on = (u32)(CdCmd_Queue.field_1EE - 0x4B) < 0x79U;
            }
            if (CdCmd_Queue.field_1F8 == 2) {
                on = 0;
            }
            if (CdCmd_Queue.field_1F8 == 4) {
                on = 0;
            }
            if (CdCmd_Queue.field_1F8 == 5) {
                on = 0;
            }
            if (!on) {
                work->field_504 = ((Actor310100Work*)work->field_4E4->work)->field_504;
                task->state--;
                taskKill(work->field_4E4);
                work->field_4E4 = NULL;
            }
            break;
    }
}

void func_actor_310100_80162C64(Task* task, s32 msgId, s32 arg2, Actor310100Placement* placement)
{
    Actor310100Work* work;

    work = (Actor310100Work*)task->work;
    if (work->field_4E4 != NULL) {
        taskKill(work->field_4E4);
    }
    work->field_506 = placement->pos.vy;
    Display_SpawnWithOt(&D_actor_310100_801798E4, 0, arg2, (s32)task);
}

/// Message 0x7D7 handler: parks the display task's work block at state 2 and
/// returns when handed mode 3, otherwise tears the display task down and spawns
/// a fresh one from `D_actor_310100_801798F0`.
void func_actor_310100_80162CDC(Task* task, s32 msgId, s32 arg2)
{
    Actor310100Work* work;
    Actor310100Work* display;

    work    = (Actor310100Work*)task->work;
    display = (Actor310100Work*)work->field_4E4->work;
    if (arg2 == 3) {
        display->field_4F0 = 2;
        return;
    }
    if (work->field_4E4 != NULL) {
        taskKill(work->field_4E4);
    }
    Display_SpawnWithOt(&D_actor_310100_801798F0, 0, arg2, (s32)task);
}

/// Message 0x7DD handler, and the display task's placement command: marks the
/// display work block dirty, then either forwards the payload to the animation
/// task (`pos.vx` zero — the seed carries the yaw into `field_4F6` and message
/// 0x3F4 gets `pos.vy` / `pos.vz` as a `GpAnimArg`) or reseeds the nineteen
/// animation slots (`pos.vz` zero resets them through `Gp_AnimResetSlot`,
/// otherwise `func_800B4114` blends them) and records the new base in
/// `field_4F8` / `field_4FA`.
void func_actor_310100_80162D50(Task* task, s32 msgId, Actor310100Placement* placement)
{
    GpAnimArg        arg;
    Actor310100Work* work;
    Actor310100Work* disp;
    Actor310100Work* msgDisp;
    Actor310100Work* resetDisp;
    Task*            display;
    u16              vy;
    u16              vz;
    u16              blend;
    u16              active;
    s32              i;

    work            = (Actor310100Work*)task->work;
    display         = work->field_4E4;
    disp            = (Actor310100Work*)display->work;
    disp->field_4F0 = 1;
    if (placement->pos.vx == 0) {
        disp->field_4F6 = placement->pos.vy;
        msgDisp         = (Actor310100Work*)display->work;
        vy              = placement->pos.vy;
        vz              = placement->pos.vz;
        if (msgDisp->field_4E8 != NULL) {
            arg.field_0  = &D_actor_310100_801797FC;
            arg.field_4  = vy;
            arg.field_8  = vz;
            arg.field_C  = 0xA;
            arg.field_10 = 1;
            Gp_DispatchMsg(msgDisp->field_4E8, 0x3F4, (s32)&arg, 0);
        }
    } else {
        active    = placement->pos.vy;
        blend     = placement->pos.vz;
        resetDisp = (Actor310100Work*)display->work;
        i         = 1;
        if (blend == 0) {
            do {
                resetDisp->slots[i & 0xFFFF].field_9 = 0x10;
                Gp_AnimResetSlot(&resetDisp->anim, i & 0xFFFF, active);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
        } else {
            do {
                func_800B4114(&resetDisp->anim, i & 0xFFFF, active, 0, 8);
                i += 1;
            } while ((u32)(i & 0xFFFF) < 0x13U);
        }
        disp->field_4F8 = placement->pos.vx;
        disp->field_4FA = placement->pos.vy;
    }
}

/// Message 0x7D4 handler: drops the payload's translation into the display
/// task's root coordinate frame, yaws that frame to the payload's `rot.vy` and
/// marks it dirty.
void func_actor_310100_80162EC8(Task* task, s32 msgId, Actor310100Placement* placement)
{
    Actor310100Work* work;
    GsCOORDINATE2*   coord;

    work              = (Actor310100Work*)task->work;
    coord             = ((TmdObject*)work->field_4E4->extra)->coords;
    coord->coord.t[0] = placement->pos.vx;
    coord->coord.t[1] = placement->pos.vy;
    coord->coord.t[2] = placement->pos.vz;
    Gfx_RotMatrixY(&coord->coord, placement->rot.vy, 0);
    coord->flg = 0;
}

/// Teardown handler: kills the display task hanging off the work block and
/// parks this task in state 3.
void func_actor_310100_80162F34(Task* task)
{
    Actor310100Work* work;

    work = (Actor310100Work*)task->work;
    if (work->field_4E4 != NULL) {
        taskKill(work->field_4E4);
        work->field_4E4 = NULL;
    }
    task->state = 3;
}

void Gp_UpdateCoord(GsCOORDINATE2* arg0);
void func_800D7A9C(TmdObject* arg0, VECTOR* arg1, s32 arg2, s32 arg3);
void func_actor_310100_801625E4(Task* task, s32 arg1);

/// Second state handler of the display model spawned from
/// `D_actor_310100_801798FC` (descriptor arg 0x80168C00): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6C and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_80162F88(Task* task)
{
    Actor310100Work* work;
    SVECTOR          rot;
    VECTOR           vec;
    TmdObject*       extra;

    work = (Actor310100Work*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_80162414(task, 0x6C);
            task->state++;
            /* fallthrough */
        case 1:
            rot.vx = 0;
            rot.vy = 0x380;
            rot.vz = 0;
            Gp_DrawFloorQuad(&((TmdObject*)task->extra)->coords[1], 0x300, &rot);
            switch (work->field_4F0) {
                case 0:
                    break;
                case 1:
                    func_actor_310100_80161F80(task);
                    break;
                case 2:
                default:
                    return;
            }
            extra  = (TmdObject*)task->extra;
            vec.vx = extra->coords[1].workm.t[0];
            vec.vy = ((TmdObject*)task->extra)->coords[1].workm.t[1];
            vec.vz = ((TmdObject*)task->extra)->coords[1].workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Second state handler of the display model spawned from
/// `D_actor_310100_80179920` (descriptor arg 0x801730B0): the spawn tick hands
/// the model to `func_actor_310100_80162414` with display id 0x6D and steps to
/// state 1, and every later tick draws the floor quad at the model's part-1
/// frame, runs `func_actor_310100_80161F80` while the display state is 1 and
/// hands that frame's translation to `func_800D7A9C`. Display state 2, the
/// freeze parked by `func_actor_310100_80162CDC`, returns before either.
void func_actor_310100_8016309C(Task* task)
{
    Actor310100Work* work;
    SVECTOR          rot;
    VECTOR           vec;
    TmdObject*       extra;

    work = (Actor310100Work*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_80162414(task, 0x6D);
            task->state++;
            /* fallthrough */
        case 1:
            rot.vx = 0;
            rot.vy = 0x380;
            rot.vz = 0;
            Gp_DrawFloorQuad(&((TmdObject*)task->extra)->coords[1], 0x300, &rot);
            switch (work->field_4F0) {
                case 0:
                    break;
                case 1:
                    func_actor_310100_80161F80(task);
                    break;
                case 2:
                default:
                    return;
            }
            extra  = (TmdObject*)task->extra;
            vec.vx = extra->coords[1].workm.t[0];
            vec.vy = ((TmdObject*)task->extra)->coords[1].workm.t[1];
            vec.vz = ((TmdObject*)task->extra)->coords[1].workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// State handler for the display model spawned by `func_actor_310100_80162C64`:
/// the spawn tick seeds the tracker from the model's part-1 coordinate frame and
/// steps to state 1, and every later tick draws the floor quad until the display
/// state goes non-zero.
void func_actor_310100_801631B0(Task* task)
{
    Actor310100Work* work;
    Actor310100Vec   pos;
    TmdObject*       extra;

    work = (Actor310100Work*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_801625E4(task, 0x6C);
            Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
            extra      = (TmdObject*)task->extra;
            pos.vec.vx = extra->coords[1].workm.t[0];
            pos.vec.vy = ((TmdObject*)task->extra)->coords[1].workm.t[1];
            pos.vec.vz = ((TmdObject*)task->extra)->coords[1].workm.t[2];
            func_800D7A9C(extra, &pos.vec, 0, 3);
            task->state++;
            break;
        case 1:
            if (work->field_4F0 == 0) {
                pos.rot.vx = 0;
                pos.rot.vy = 0x380;
                pos.rot.vz = 0;
                Gp_DrawFloorQuad(&((TmdObject*)task->extra)->coords[1], 0x300, &pos.rot);
            }
            break;
    }
}

/// The other display-model state handler (message 0x6D): the spawn tick seeds
/// the tracker from the model's part-1 coordinate frame and steps to state 1,
/// and every later tick draws the floor quad while the display state is still
/// below 2.
void func_actor_310100_801632B0(Task* task)
{
    Actor310100Work* work;
    Actor310100Vec   pos;
    TmdObject*       extra;

    work = (Actor310100Work*)task->work;
    switch (task->state) {
        case 0:
            func_actor_310100_801625E4(task, 0x6D);
            Gp_UpdateCoord(&((TmdObject*)task->extra)->coords[1]);
            extra      = (TmdObject*)task->extra;
            pos.vec.vx = extra->coords[1].workm.t[0];
            pos.vec.vy = ((TmdObject*)task->extra)->coords[1].workm.t[1];
            pos.vec.vz = ((TmdObject*)task->extra)->coords[1].workm.t[2];
            func_800D7A9C(extra, &pos.vec, 0, 3);
            task->state++;
            break;
        case 1:
            switch (work->field_4F0) {
                case 0:
                case 1:
                    pos.rot.vx = 0;
                    pos.rot.vy = 0x380;
                    pos.rot.vz = 0;
                    Gp_DrawFloorQuad(&((TmdObject*)task->extra)->coords[1], 0x300, &pos.rot);
                    break;
            }
            break;
    }
}

INCLUDE_RODATA("actors/nonmatchings/actor_310100/actor_310100", D_actor_310100_80161E20);
