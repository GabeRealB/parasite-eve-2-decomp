#include "common.h"

#include "actors/actor_120500.h"
#include "gameplay/1BC.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

extern TaskDesc D_actor_120500_80138418;

extern u32 D_actor_120500_80138088[];
extern u32 D_actor_120500_80138408[];

void func_actor_120500_80131E58(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    s16         slot;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            SetDispMask(0);
            Mem_AllocAuxWithImages(1);
            goto advance;
        case 1:
            key          = gGameSession->at4;
            key.loc.view = 0x64;
            slot         = Stream_FindSlot(key.raw.data, 0, 0);
            slotParam[0] = slot;
            CdCmd_Enqueue(0x61, 0, slotParam);
            goto advance;
        case 2:
            if (queue->field_1FA == 0) {
                return;
            }
            SndEvt_EnqueueType6(0x521E0007, 0, 0);
            SetDispMask(1);
            goto advance;
        case 3:
            if (CdCmd_IsIdle() & 0xFFFF) {
                SetDispMask(0);
                goto advance;
            }
            if (Pad_CheckFlag800() == 0) {
                return;
            }
            SetDispMask(0);
            CdCmd_ActivatePhase1();
            goto advance;
        case 4:
            if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
                return;
            }
            Stream_ResetRestoreState();
        advance:
            task->state = task->state + 1;
            return;
        case 5:
            if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
                return;
            }
            Task_SpawnOnDefaultList(&D_actor_120500_80138418, 1, 8, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Per-frame request handler for the pair at `field_4B8` / `field_4BA`, stepped
/// by the tick body and armed by `func_actor_120500_801328C0`. Every request
/// first tells the task at `field_4B4` to stop (message 0x3ED), then dispatches
/// on the code: 2 raises the override vector and installs animation set 0 once
/// before sending placement record `D_actor_120500_80138090` on every tick --
/// the only code that does not clear itself; 3 spawns table entry 1, drops the
/// override and installs set 1 with `D_actor_120500_801380A8`; 4 installs set 2
/// with the 8-frame blend; 5 broadcasts the actor's own visibility message
/// 0x7D5; and 6 sends the player the equipped-weapon animation set, picked the
/// same way `func_actor_120500_8013241C` picks it. Every other code, 0 and 1
/// included, just clears the request.
void func_actor_120500_80132028(Task* arg0)
{
    Actor120500Work* work;
    Actor120500Work* w;
    Actor120500Work* w2;
    Actor120500Work* w3;
    SVECTOR          vec;
    GpAnimArg        msg;
    GpAnimArg*       p;
    s32              anim;
    s32              base;

    work = (Actor120500Work*)arg0->work;
    if (work->field_4B4 != NULL) {
        Gp_DispatchMsg(work->field_4B4, 0x3ED, 0, 0);
    }
    switch ((u16)work->field_4B8) {
        case 0:
        case 1:
            break;
        case 2:
            switch ((u16)work->field_4BA) {
                case 0:
                    vec.vx = 0x960;
                    vec.vy = 0x960;
                    vec.vz = 0x960;
                    Gp_SetOverrideVec(&vec);
                    w3 = (Actor120500Work*)arg0->work;
                    p  = &msg;
                    if (w3->field_4B4 != NULL) {
                        msg.field_0 = D_actor_120500_8013807C;
                        msg.field_4 = 0;
                        msg.field_8 = 0;
                        msg.field_C = 0;
                        p->field_10 = 1;
                        Gp_DispatchMsg(w3->field_4B4, 0x3F4, (s32)p, 0);
                    }
                    work->field_4BA = work->field_4BA + 1;
                    /* fallthrough */
                case 1:
                    Gp_DispatchMsg(((Actor120500Work*)arg0->work)->field_4B4, 0x3E9,
                                   (s32)&D_actor_120500_80138090, 0);
                    return;
            }
            return;
        case 3:
            Task_SpawnFromTable(&D_actor_120500_80138418, 1, 8, 0);
            w = (Actor120500Work*)arg0->work;
            Gp_SetOverrideVec(NULL);
            Gp_DispatchMsg(w->field_4B4, 0x3F3, 1, 0);
            Gp_DispatchMsg(w->field_4B4, 0x3E9, (s32)&D_actor_120500_801380A8, 0);
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.field_0 = D_actor_120500_8013807C;
                p->field_4  = 1;
                msg.field_8 = 0;
                msg.field_C = 0;
                p->field_10 = 1;
                Gp_DispatchMsg(w2->field_4B4, 0x3F4, (s32)p, 0);
            }
            break;
        case 4:
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.field_0 = D_actor_120500_8013807C;
                p->field_4  = 2;
                p->field_8  = 1;
                p->field_C  = 8;
                p->field_10 = 1;
                Gp_DispatchMsg(w2->field_4B4, 0x3F4, (s32)p, 0);
            }
            break;
        case 5:
            Gp_DispatchMsg(arg0, 0x7D5, 2, 0);
            break;
        case 6:
            base = D_80073BA9;
            if (D_8007218A == 1) {
                anim = base + 1;
            } else {
                anim = base + 0x22;
            }
            msg.field_0  = (void*)anim;
            msg.field_4  = 1;
            msg.field_8  = 0;
            msg.field_C  = 0;
            msg.field_10 = 0;
            Gp_DispatchMsg(work->field_4B4, 0x3E8, (s32)&msg, 0);
            break;
    }
    work->field_4B8 = 0;
}

/// Builds the actor's work block for its scene: `Mem_Malloc(0x4CC, 0)`, and
/// on failure it kills the task and returns.  The allocation's `gameGetPtrSlot(3)`
/// task goes to `field_4B4`, the task itself is published in
/// `D_actor_120500_80138454`, and the model in `Task::extra` gets its
/// coordinate parented to `gGfxViewCoord` plus the light and colour matrices at
/// 0x474 / 0x494.  The area placement record is then walked to the one whose id
/// is 0x65 -- as in the sibling actors, the search stops on either that id or
/// the 0xFF terminator -- and its two texture bytes are handed to
/// `Gp_SetTmdBytes` before the animation banks seed the work block.
/// Finally `Task::msgTable` takes the message table and all twenty animation
/// slots are armed, slot 0 excepted.
void func_actor_120500_801322A0(Task* arg0)
{
    Actor120500Work* work;
    Actor120500Work* slotsWork;
    TaskIdMap*       map;
    TmdObject*       tmd;
    GsCOORDINATE2*   coord;
    GpAreaPlace*     place;
    s32              i;
    u8               id;

    tmd        = arg0->extra;
    coord      = tmd->coords;
    map        = Mem_Malloc(0x4CC, 0);
    arg0->work = map;
    if (map == NULL) {
        taskKill(arg0);
        return;
    }
    work = (Actor120500Work*)map;
    Mem_Set(work, 0, 0x4CC);
    work->field_4B4         = gameGetPtrSlot(3);
    D_actor_120500_80138454 = (Actor120500*)arg0;
    coord->sub              = &gGfxViewCoord;
    tmd->lightMtx           = &work->field_474;
    tmd->flags              = 0;
    tmd->colorMtx           = &work->field_494;
    place                   = (GpAreaPlace*)Gp_GetNestedAreaRec((GpAreaKey*)&gGameSession->at4.loc)->field_0;
    id                      = place->entryId;
    while (id != 0xFF) {
        if (id == 0x65) {
            break;
        }
        place++;
        id = place->entryId;
    }
    Gp_SetTmdBytes(tmd, ((s8*)place)[0xD], ((s8*)place)[0xE]);
    func_800B3F84(&work->anim, D_actor_120500_80138088, tmd, work->field_334, work->slots);
    slotsWork      = (Actor120500Work*)arg0->work;
    arg0->msgTable = &D_actor_120500_80138408;
    i              = 1;
    do {
        slotsWork->slots[(u16)i].rate = 0x10;
        Gp_AnimResetSlot(&slotsWork->anim, (u16)i, 1);
        i++;
    } while ((u16)i < 0x14U);
}

/// Per-frame body of the actor task. State 0 brings it up through
/// `func_actor_120500_801322A0` -- unless `D_80114C12` is 1 or `D_80071075` is
/// set -- sends the player task the equipped-weapon animation set as message
/// 0x3E8 and installs the two `func_800E8634` blocks; state 1 kills the actor
/// once the session's field_1 clears.
///
/// Every state then steps the tick handler, ticks the nineteen animation slots
/// past slot 0 and walks the slots to the first whose `flags` bit 0 is
/// clear. The request/phase pair at 0x4C0 spawns effect 0x601B4 (table entry 1)
/// plus the placement record, and the one at 0x4C8 spawns table entry 2 or
/// raises `D_8007106B` and the view tasks before clearing the code. The model's
/// part-1 translation goes to `func_800D7A9C` last.
///
/// The request 0x4C8 dispatch is written with gotos: the labels reproduce
/// retail's block layout, where the three clear sites sit at the end of their
/// own arms.
void func_actor_120500_8013241C(Task* arg0)
{
    Actor120500Work* work;
    Actor120500Work* slotsWork;
    Actor120500Work* w;
    Actor120500Args  args;
    TmdObject*       mdl;
    s32              anim;
    s32              code;
    s32              i;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 != 1 && D_80071075 == 0) {
                func_actor_120500_801322A0(arg0);
                anim = D_80073BA9;
                if (D_8007218A == 1) {
                    anim = anim + 1;
                } else {
                    anim = anim + 0x22;
                }
                args.msg.field_0  = (u8*)anim;
                args.msg.field_4  = 1;
                args.msg.field_8  = 1;
                args.msg.field_C  = 10;
                args.msg.field_10 = 0;
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E8, (s32)&args.msg, 0);
                func_800E3FAC(0xA2, 0xD);
                func_800E8634((s32)D_actor_120500_801380D8, 0, (s32)D_actor_120500_80138318);
                arg0->state += 1;
                break;
            }
            return;
        case 1:
            if (gGameSession->eventState == 0) {
                Task_RequestKill(arg0, 0);
                return;
            }
            break;
    }

    func_actor_120500_80132028(arg0);
    work      = (Actor120500Work*)arg0->work;
    slotsWork = work;

    i = 1;
    do {
        Gp_AnimTickIndex(&slotsWork->anim, (u16)i);
        i++;
    } while ((u16)i < 0x14U);

    i = 1;
loop_slots:
    if ((slotsWork->slots[(u16)i].flags & 1) != 0) {
        i++;
        if ((u16)i < 0x14U) {
            goto loop_slots;
        }
    }

    if (work->field_4C0 != 0) {
        if (work->field_4C0 == 1) {
            Tmd_AllocBuffers((TmdObject*)arg0->extra);
            Task_SpawnFromTable(&D_actor_120500_80138418, 1, 8, 0);
            Gp_DispatchMsg(arg0, 0x7D4, (s32)&D_actor_120500_801380C0, 0);
        }
    }
    work->field_4C0 = 0;

    w    = (Actor120500Work*)arg0->work;
    code = w->field_4C8;
    if (code != 1) {
        if (code >= 2) {
            if (code != 2) {
                w->field_4C8 = 0;
                goto done_4C8;
            } else {
                goto do_4C8_case2;
            }
        } else {
            goto clear_4C8;
        }
    } else {
        goto do_4C8_case1;
    }
do_4C8_case1:
    Task_SpawnFromTable(&D_actor_120500_80138418, 2, 8, 0);
    w->field_4C8 = 0;
    goto done_4C8;
do_4C8_case2:
    Gp_DispatchMsg(w->field_4B4, 0x3F3, 2, 0);
    Display_SpawnWithOt(&D_actor_120500_80138418, 0, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
clear_4C8:
    w->field_4C8 = 0;
done_4C8:

    mdl         = (TmdObject*)arg0->extra;
    args.pos.vx = ((TmdObject*)arg0->extra)->coords[1].workm.t[0];
    args.pos.vy = ((TmdObject*)arg0->extra)->coords[1].workm.t[1];
    args.pos.vz = ((TmdObject*)arg0->extra)->coords[1].workm.t[2];
    func_800D7A9C(mdl, &args.pos, 0, 3);
}
