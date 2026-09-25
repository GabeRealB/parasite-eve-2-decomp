#include "common.h"

#include "actors/actor.h"
#include "actors/actors_shared_8013411c.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"

/// The actor's work block, hung off `Task::work`. `func_actor_120500_801322A0`
/// allocates it with `Mem_Malloc(0x4CC, 0)` and zeroes it with `Mem_Set`.
///
/// It opens with the animation state `func_800B3F84` is handed: the
/// `GpAnimCtx`, the twenty `GpAnimSlot`s the tick walks and the pose buffer.
/// The two `MATRIX`es are the model's light and colour matrices, published
/// through `TmdObject::lightMtx` / `colorMtx`. The three code/phase pairs at
/// the end are requests the setters arm and the tick consumes.
typedef struct Actor120500Work {
    /* 0x000 */ GpAnimCtx  anim;             // `func_800B3F84` arg0
    /* 0x014 */ GpAnimSlot slots[0x14];
    /* 0x334 */ byte       field_334[0x140]; // pose buffer, `func_800B3F84` arg3
    /* 0x474 */ MATRIX     field_474;        // light matrix, into TmdObject::lightMtx
    /* 0x494 */ MATRIX     field_494;        // colour matrix, into TmdObject::colorMtx
    /* 0x4B4 */ Task*      field_4B4;        // task in pointer slot 3, the animation messages' target
    /* 0x4B8 */ s16        field_4B8;
    /* 0x4BA */ s16        field_4BA;
    /* 0x4BC */ byte       pad_4BC[0x4];
    /* 0x4C0 */ u16        field_4C0;
    /* 0x4C2 */ s16        field_4C2;
    /* 0x4C4 */ byte       pad_4C4[0x4];
    /* 0x4C8 */ u16        field_4C8;
    /* 0x4CA */ s16        field_4CA;
} Actor120500Work;
STATIC_ASSERT_SIZEOF(Actor120500Work, 0x4CC);

/// Scratch buffer `func_actor_120500_8013241C` fills twice in one tick: as the
/// `GpAnimArg` it hands the task in pointer slot 3 with message 0x3E8, then as
/// the model's part-1 translation `func_800D7A9C` draws with. The two uses
/// cannot overlap, and the frame keeps them in one 0x14-byte stack slot.
typedef union Actor120500Args {
    /* 0x0 */ GpAnimArg msg; // message 0x3E8 payload
    /* 0x0 */ VECTOR    pos; // model part-1 translation
} Actor120500Args;

/// The actor task, published by `func_actor_120500_801322A0` so the setters,
/// which take no task, can reach its work block.
extern Task* D_actor_120500_80138454;

/// The actor's five-entry task table: 0 the streamed sequence
/// (`func_actor_120500_80131E58`), 1 the fade from black, 2 the fade to black,
/// 3 `taskKill`, 4 the actor itself.
extern TaskDesc D_actor_120500_80138418[];

/// Animation banks `func_800B3F84` seeds the work block from.
extern u32 D_actor_120500_80138088[];

/// Message table the actor answers with: 0x7D5 shows or hides the model, 0x7D4
/// places it.
extern u32 D_actor_120500_80138408[];

/// Equipped-weapon id and the flag that selects which block of animation sets
/// it indexes (`+1` when set to 1, `+0x22` otherwise).
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Flags the tick checks before bringing the actor up (`D_80114C12` /
/// `D_80071075`), and the one it raises alongside the view tasks
/// (`D_8007106B`).
extern s8 D_80114C12;
extern u8 D_80071075;
extern s8 D_8007106B;

/// Animation-set table handed to the task in pointer slot 3 as message 0x3F4's
/// `GpAnimArg::animBlock`; the messages select sets 0, 1 and 2 of it.
extern GpAnimSet* D_actor_120500_8013807C[];

/// Placement records sent to that same task as message 0x3E9, passed by
/// address.
extern s32 D_actor_120500_80138090;
extern s32 D_actor_120500_801380A8;

/// Pair of blocks `func_actor_120500_8013241C` passes to `func_800E8634`.
extern u8 D_actor_120500_801380D8[];
extern u8 D_actor_120500_80138318[];

/// The actor's own placement, sent to itself as message 0x7D4.
extern ActorShared8013411cPlacement D_actor_120500_801380C0;

/// Entry 0 of the task table: plays a streamed sequence, then restores the
/// scene. It looks up the stream slot for the current location with view 0x64
/// and enqueues CD command 0x61 on it, turns the display on with sound cue
/// 0x521E0007 once the queue reports ready, and waits for the stream to end or
/// for the pad to cut it short. After the restore it spawns the fade from
/// black, clears the image buffers, and kills itself.
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
            Task_SpawnOnDefaultList(D_actor_120500_80138418, 1, 8, 0);
            Mem_Set(Fs_ImgBuffers, 0, 0x25800);
            SetDispMask(1);
            taskKill(task);
            Display_ResetHeapWrapper();
            return;
    }
}

/// Per-frame request handler for the pair at `field_4B8` / `field_4BA`, stepped
/// by the tick body and armed by `func_actor_120500_801328C0`. Every tick first
/// sends message 0x3ED to the task at `field_4B4`, then dispatches on the code:
/// 2 raises the override vector and installs animation set 0 once before
/// sending placement record `D_actor_120500_80138090` on every tick -- the only
/// code that does not clear itself; 3 spawns the fade from black, drops the
/// override and installs set 1 with `D_actor_120500_801380A8`; 4 installs set 2
/// with the 8-frame blend; 5 sends the actor its own message 0x7D5 with payload
/// 2, hiding the model; and 6 sends message 0x3E8 with the equipped-weapon
/// animation, picked the same way `func_actor_120500_8013241C` picks it. Every
/// other code, 0 and 1 included, just clears the request.
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
                        msg.animBlock.ptr = D_actor_120500_8013807C;
                        msg.field_4       = 0;
                        msg.field_8       = 0;
                        msg.field_C       = 0;
                        p->field_10       = 1;
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
            Task_SpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
            w = (Actor120500Work*)arg0->work;
            Gp_SetOverrideVec(NULL);
            Gp_DispatchMsg(w->field_4B4, 0x3F3, 1, 0);
            Gp_DispatchMsg(w->field_4B4, 0x3E9, (s32)&D_actor_120500_801380A8, 0);
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.animBlock.ptr = D_actor_120500_8013807C;
                p->field_4        = 1;
                msg.field_8       = 0;
                msg.field_C       = 0;
                p->field_10       = 1;
                Gp_DispatchMsg(w2->field_4B4, 0x3F4, (s32)p, 0);
            }
            break;
        case 4:
            w2 = (Actor120500Work*)arg0->work;
            p  = &msg;
            if (w2->field_4B4 != NULL) {
                msg.animBlock.ptr = D_actor_120500_8013807C;
                p->field_4        = 2;
                p->field_8        = 1;
                p->field_C        = 8;
                p->field_10       = 1;
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
            msg.animBlock.index = anim;
            msg.field_4         = 1;
            msg.field_8         = 0;
            msg.field_C         = 0;
            msg.field_10        = 0;
            Gp_DispatchMsg(work->field_4B4, 0x3E8, (s32)&msg, 0);
            break;
    }
    work->field_4B8 = 0;
}

/// Builds the actor's work block for its scene: `Mem_Malloc(0x4CC, 0)`, and
/// on failure it kills the task and returns. The task in pointer slot 3 goes
/// to `field_4B4`, the task itself is published in `D_actor_120500_80138454`,
/// and the model in `Task::extra` is shown, its coordinate parented to
/// `gGfxViewCoord` and its light and colour matrices pointed at the work
/// block. The area's placement records are then walked to the one whose id is
/// 0x65, stopping at the 0xFF terminator, and its two texture bytes are handed
/// to `Gp_SetTmdBytes` before the animation banks seed the work block. Finally
/// `Task::msgTable` takes the message table and every animation slot but slot
/// 0 is reset at rate 0x10.
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
    D_actor_120500_80138454 = arg0;
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

/// Per-frame body of the actor task, entry 4 of the task table. State 0 waits
/// until `D_80114C12` is not 1 and `D_80071075` is clear, then brings the actor
/// up through `func_actor_120500_801322A0`, sends the task in pointer slot 3
/// the equipped-weapon animation as message 0x3E8 and installs the two
/// `func_800E8634` blocks; state 1 kills the actor once the session's
/// `eventState` clears.
///
/// Every state then steps the request handler, ticks the nineteen animation
/// slots past slot 0 and walks the slots to the first whose `flags` bit 0 is
/// clear. Request code 1 at 0x4C0 allocates the model's buffers, spawns the
/// fade from black and places the actor with its own placement record. At
/// 0x4C8, code 1 spawns the fade to black and code 2 sends message 0x3F3,
/// spawns the streamed sequence, raises `D_8007106B` and spawns the view
/// tasks. The model's part-1 translation goes to `func_800D7A9C` last.
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
                args.msg.animBlock.index = anim;
                args.msg.field_4         = 1;
                args.msg.field_8         = 1;
                args.msg.field_C         = 10;
                args.msg.field_10        = 0;
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
            Tmd_AllocBuffers(arg0->extra);
            Task_SpawnFromTable(D_actor_120500_80138418, 1, 8, 0);
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
    Task_SpawnFromTable(D_actor_120500_80138418, 2, 8, 0);
    w->field_4C8 = 0;
    goto done_4C8;
do_4C8_case2:
    Gp_DispatchMsg(w->field_4B4, 0x3F3, 2, 0);
    Display_SpawnWithOt(D_actor_120500_80138418, 0, 0, 0);
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

/// Fade from black, entry 1 of the actor's task table.
///
/// State 0 allocates the channel block and seeds all three channels at 0xFF;
/// a failed allocation kills the task. State 1 runs every frame: it draws a
/// subtractive `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never
/// drawn), then lowers all three channels by `Task::spawnArg1`, the fade rate.
/// Once `r` has gone negative the screen is clear and the task kills itself.
void func_actor_120500_80132708(Task* arg0)
{
    ActorFadeWork* fade;
    ActorFadeWork* alloc;

    fade = (ActorFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorFadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                taskKill(arg0);
                return;
            }
            fade         = alloc;
            fade->b      = 0xFF;
            fade->g      = 0xFF;
            fade->r      = 0xFF;
            arg0->state += 1;
            /* fallthrough */
        case 1:
            Fade_DrawOverlay((u8)fade->r, (u8)fade->g, (u8)fade->r, 2);
            fade->r = (s16)((u16)fade->r - (u16)arg0->spawnArg1);
            fade->g = (s16)((u16)fade->g - (u16)arg0->spawnArg1);
            fade->b = (s16)((u16)fade->b - (u16)arg0->spawnArg1);
            if (fade->r >= 0) {
                return;
            }
            taskKill(arg0);
            break;
    }
}

/// Fade to black, entry 2 of the actor's task table.
///
/// State 0 allocates the channel block and clears it; a failed allocation
/// kills the task. State 1 runs every frame: it draws a subtractive
/// `Fade_DrawOverlay` tinted `r`/`g`/`r` (`b` is stepped but never drawn),
/// then raises all three channels by `Task::spawnArg1`, the fade rate. Once
/// `r` reaches 0x100 the screen is black and the task kills itself.
void func_actor_120500_801327E4(Task* arg0)
{
    ActorFadeWork* work;
    ActorFadeWork* alloc;

    work = (ActorFadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (ActorFadeWork*)Mem_Malloc(8, 0);
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

/// Request setters, reached from the tables in the actor's data: each arms one
/// code/phase pair of the actor's work block with `arg0` and restarts its
/// phase. This one arms the pair `func_actor_120500_80132028` consumes; the
/// next two arm the pairs the tick consumes at 0x4C0 and 0x4C8.
void func_actor_120500_801328C0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4B8 = arg0;
    work->field_4BA = 0;
}

void func_actor_120500_801328E0(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4C0 = arg0;
    work->field_4C2 = 0;
}

void func_actor_120500_80132900(s16 arg0)
{
    Actor120500Work* work = D_actor_120500_80138454->work;

    work->field_4C8 = arg0;
    work->field_4CA = 0;
}

/// Puts the actor back to rest: plays sound cue `0x521E0007`, sends the actor
/// its own message 0x7D5 with payload 2, hiding the model, and clears the
/// three request codes the setters above arm. The task at `field_4B4` then
/// gets animation set 2 of `D_actor_120500_8013807C` (message 0x3F4), message
/// 0x3F3 with payload 1, and the placement record `D_actor_120500_801380A8`
/// as message 0x3E9, with the override vector cleared in between.
void func_actor_120500_80132920(void)
{
    Task*            actor;
    Actor120500Work* work;
    Actor120500Work* animWork;
    GpAnimArg        msg;

    actor = D_actor_120500_80138454;
    work  = actor->work;
    SndEvt_EnqueueType7(0x521E0007, 0xA);
    Gp_DispatchMsg(actor, 0x7D5, 2, 0);
    work->field_4B8 = 0;
    work->field_4C0 = 0;
    work->field_4C8 = 0;
    animWork        = actor->work;
    if (animWork->field_4B4 != NULL) {
        msg.animBlock.ptr = D_actor_120500_8013807C;
        msg.field_4       = 2;
        msg.field_8       = 0;
        msg.field_C       = 0;
        msg.field_10      = 1;
        Gp_DispatchMsg(animWork->field_4B4, 0x3F4, (s32)&msg, 0);
    }
    work = actor->work;
    Gp_SetOverrideVec(NULL);
    Gp_DispatchMsg(work->field_4B4, 0x3F3, 1, 0);
    Gp_DispatchMsg(work->field_4B4, 0x3E9, (s32)&D_actor_120500_801380A8, 0);
}

/// Message 0x7D5 handler: shows or hides the task's model. Payload 0 hides it
/// (sets `TmdObject` flag 0x80), 1 shows it and clears flag 0x4, and 2 hides
/// it and sets 0x4, which keeps `Tmd_AllocMissingBuffers` from giving it
/// buffers again. Payload 2 sets 0x4 and falls into payload 0, rather than
/// setting both bits at once, and the branch layout follows that. `arg1` is
/// the message id.
void func_actor_120500_80132A04(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* extra;

    extra = (TmdObject*)task->extra;
    switch (arg2) {
        case 2:
            extra->flags = extra->flags | 4;
            /* fallthrough */
        case 0:
            extra->flags = extra->flags | 0x80;
            return;
        case 1:
            extra->flags = extra->flags & 0xFF7B;
            return;
    }
}

/// Message 0x7D4 handler: places the task's model in the world. The model's
/// coordinate is parented to the view coordinate, takes `placement`'s
/// position as its translation and its rotation applied Y, then X, then Z.
/// `arg1` is the message id.
void func_actor_120500_80132A74(Task* task, s32 arg1, ActorShared8013411cPlacement* placement)
{
    GsCOORDINATE2* coord;
    MATRIX*        mtx;

    coord             = ((TmdObject*)task->extra)->coords;
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
