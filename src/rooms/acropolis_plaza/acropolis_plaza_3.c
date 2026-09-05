#include "common.h"
#include "gameplay/gameplay.h"
#include "main/fs.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/stream.h"
#include "main/task.h"
#include "rooms/acropolis_plaza.h"

/// Two four-vertex quads facing each other across the plaza's scene object:
/// one at x - 0xBB8, one at x + 0x7D0, each spanning y .. y + 0x3E8 and
/// z - 0x1000 .. z + 0x3000. The second quad's vertices run in the opposite
/// z order, flipping its winding.
extern SVECTOR D_acropolis_plaza_80198B90[8];

/// Rebuilds the eight box vertices in `D_acropolis_plaza_80198B90` around the
/// scene work's world position.
void func_acropolis_plaza_8017DD90(Task* arg0)
{
    AcropolisPlazaSceneWork* work = (AcropolisPlazaSceneWork*)arg0->idMap;
    s32                      x    = work->pos.vx;
    s32                      y    = work->pos.vy;
    s32                      z    = work->pos.vz;
    s16                      near = x - 0xBB8;
    s16                      top;
    s16                      left;
    s16                      right;
    s16                      far;

    D_acropolis_plaza_80198B90[0].vx = near;
    D_acropolis_plaza_80198B90[1].vx = near;
    D_acropolis_plaza_80198B90[2].vx = near;
    D_acropolis_plaza_80198B90[3].vx = near;

    top   = y + 0x3E8;
    left  = z - 0x1000;
    right = z + 0x3000;
    far   = x + 0x7D0;

    D_acropolis_plaza_80198B90[0].vy = y;
    D_acropolis_plaza_80198B90[1].vy = y;
    D_acropolis_plaza_80198B90[2].vy = top;
    D_acropolis_plaza_80198B90[3].vy = top;

    D_acropolis_plaza_80198B90[0].vz = left;
    D_acropolis_plaza_80198B90[1].vz = right;
    D_acropolis_plaza_80198B90[2].vz = left;
    D_acropolis_plaza_80198B90[3].vz = right;

    D_acropolis_plaza_80198B90[4].vx = far;
    D_acropolis_plaza_80198B90[5].vx = far;
    D_acropolis_plaza_80198B90[6].vx = far;
    D_acropolis_plaza_80198B90[7].vx = far;

    D_acropolis_plaza_80198B90[4].vy = y;
    D_acropolis_plaza_80198B90[5].vy = y;
    D_acropolis_plaza_80198B90[6].vy = top;
    D_acropolis_plaza_80198B90[7].vy = top;

    D_acropolis_plaza_80198B90[4].vz = right;
    D_acropolis_plaza_80198B90[5].vz = left;
    D_acropolis_plaza_80198B90[6].vz = right;
    D_acropolis_plaza_80198B90[7].vz = left;
}

/// The plaza's view tables, one per camera set. Each entry is a *pair* of
/// `GpViewRec`s -- the two shots the stream alternates between -- indexed by
/// `CdCmd_Queue.field_1EE - 1`, so a table row is 0x48 bytes.
extern GpViewRec D_acropolis_plaza_801838B8[][2];
extern GpViewRec D_acropolis_plaza_8018A938[][2];
extern GpViewRec D_acropolis_plaza_8018CAFC[][2];
extern GpViewRec D_acropolis_plaza_8018F530[][2];
extern GpViewRec D_acropolis_plaza_8018F9B4[][2];

/// Applies the plaza camera for view set `arg0`.
///
/// Sets 0..3 all share the opening table and pick within a row directly:
/// while `field_1FA` is clear the row's first shot is used, otherwise
/// `field_1F2` steps forward or back from it depending on `field_1F0`.
/// Sets 4..7 (and any out-of-range value, which leaves the table whatever the
/// caller left in place) instead index the table flat, one shot per step, and
/// clamp the backwards walk at the start of the table.
void func_acropolis_plaza_8017DE24(s32 arg0)
{
    CdCmdQueue* q = &CdCmd_Queue;
    GpViewRec(*tbl)[2];
    GpViewRec* view;
    s16        idx;

    switch ((u16)arg0) {
        case 0:
        case 1:
        case 2:
        case 3:
            tbl = D_acropolis_plaza_801838B8;
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else if (q->field_1F0 == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] + q->field_1F2;
            } else {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair] - q->field_1F2;
            }
            break;
        case 4:
            tbl = D_acropolis_plaza_8018A938;
            goto common;
        case 5:
            tbl = D_acropolis_plaza_8018CAFC;
            goto common;
        case 6:
            tbl = D_acropolis_plaza_8018F530;
            goto common;
        case 7:
            tbl = D_acropolis_plaza_8018F9B4;
        default:
        common:
            if (q->field_1FA == 0) {
                s32 pair = q->field_1EE - 1;
                view     = tbl[pair];
            } else {
                if (q->field_1F0 == 0) {
                    idx = ((q->field_1EE - 1) * 2) + q->field_1F2 + 1;
                } else {
                    idx = ((q->field_1EE - 1) * 2) - q->field_1F2 - 1;
                    if (idx < 0) {
                        idx = 0;
                    }
                }
                view = *tbl + idx;
            }
            break;
    }
    Gp_ApplyView(view);
}

/// The plaza's camera table: one world position per stream view, indexed by
/// `CdCmd_Queue.field_1EE - 1`.
extern VECTOR3 D_acropolis_plaza_801907C4[];

/// The view matrix the scene task measures the player against.
extern MATRIX* D_80073B8C;

/// Recomputes the scene's two "the player has walked off this shot" flags from
/// how far the shot's anchor sits ahead of the camera. Past 0xC8 the player has
/// gone forward, before -0x14 they have gone back, and the first view has no
/// shot behind it so the backwards flag never fires there. A `field_958` of 3
/// (the player running) picks the 2 variant of either flag; anything else
/// picks 1.
static __inline__ void plaza_updateEdgeFlags(AcropolisPlazaSceneWork* w)
{
    CdCmdQueue* cq   = &CdCmd_Queue;
    s32         dist = w->distX;

    w->fwd  = 0;
    w->back = 0;
    if (dist >= 0xC9) {
        if ((u16)w->actor->field_958 == 3) {
            w->fwd = 2;
        } else {
            w->fwd = 1;
        }
    } else if (dist < -0x14) {
        if (cq->field_1EE != 1) {
            if ((u16)w->actor->field_958 == 3) {
                w->back = 2;
            } else {
                w->back = 1;
            }
        }
    }
}

/// The plaza's scene task: it plays the room's pre-rendered camera stream and
/// re-seeks it whenever the player walks past the end of the current shot.
///
/// State 0 allocates the work block, seeds `CdCmd_Queue` from the spawn
/// argument and (unless the argument suppresses it) asks for the opening
/// stream; state 1 caches the slot-3 task once the CD is idle and turns the
/// display on. State 2 is the running state: it refreshes the edge flags, and
/// when one fires it enqueues a 0x71 seek to the neighbouring view -- forwards
/// from `baseView`, backwards from `baseView + 1` -- with the target frame
/// packed into the request's last two bytes. States 3..5 wait for that seek to
/// land and return to state 2, and state 6 is the parked state the task sits in
/// while `field_1F6` blocks seeking, leaving on the first flag that matches the
/// current view.
///
/// `loMask` holds 0xFF in a local on purpose: masking with a literal lets GCC
/// fold the `andi` into the byte store, and the original build keeps it.
void func_acropolis_plaza_8017DFE0(Task* task)
{
    u8                       slot[4];
    s32                      frameOfs;
    u32                      seekFrame;
    u32                      openFrame;
    s32                      loMask = 0xFF;
    s32                      side;
    TaskIdMap*               block;
    CdCmdQueue*              q;
    AcropolisPlazaSceneWork* work;
    AcropolisPlazaSceneArg*  arg;
    Task*                    slot3;
    u16                      view;
    u16                      startView;

    q    = &CdCmd_Queue;
    work = (AcropolisPlazaSceneWork*)task->idMap;

    if (task->state != 0) {
        switch (q->field_1F8) {
            case 0:
            case 1:
            case 2:
            case 3:
                work->pos.vx = D_acropolis_plaza_801907C4[q->field_1EE - 1].vx;
                work->pos.vy = D_acropolis_plaza_801907C4[q->field_1EE - 1].vy;
                work->pos.vz = D_acropolis_plaza_801907C4[q->field_1EE - 1].vz;
                work->distX  = work->pos.vx - work->mtx->t[0];
                break;
        }
    }

    switch (task->state) {
        case 0:
            goto L_case0;
        case 1:
            goto L_case1;
        case 2:
            goto L_case2;
        case 3:
            goto L_case3;
        case 4:
            goto L_case4;
        case 5:
            goto L_case5;
        case 6:
            goto L_case6;
    }
    goto L_tail;

L_case0:
    block       = (TaskIdMap*)Mem_Malloc(0x34, 0);
    task->idMap = block;
    if (block == NULL) {
        Task_Kill(task);
        return;
    }
    Mem_Set(block, 0, 0x34);
    arg            = (AcropolisPlazaSceneArg*)task->spawnArg2;
    work           = (AcropolisPlazaSceneWork*)task->idMap;
    startView      = arg->view;
    q->field_1F8   = 0;
    q->field_1EE   = startView;
    q->field_1EA   = startView;
    work->prevSide = 0;
    q->field_1F0   = 0;
    q->field_1FA   = 0;
    q->field_1E8   = 0;
    if (((AcropolisPlazaSceneArg*)task->spawnArg2)->noStream == 0) {
        slot[0]   = Stream_FindSlot(&Game_Session->field_4, q->field_1F8, 0);
        frameOfs  = (q->field_1EA - 1) * 10;
        openFrame = frameOfs & 0xFFFF;
        slot[1]   = openFrame >> 8;
        slot[2]   = openFrame & loMask;
        CdCmd_Enqueue(0x72, 0, slot);
    } else {
        q->field_1F6 = 0;
    }
    ((AcropolisPlazaSceneWork*)task->idMap)->mtx = D_80073B8C;
    work->field_2E                               = 1;
    task->state                                  = task->state + 1;
    goto L_tail;

L_case1:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto L_tail;
    }
    slot3       = (Task*)Game_GetPtrSlot(3);
    work->slot3 = slot3;
    work->actor = (GameActor*)slot3->idMap;
    SetDispMask(1);
    task->state = task->state + 1;
    goto L_tail;

L_case2:
    work->streamLen = Stream_GetSlotField1A(q->field_1F8);
    if (q->field_1F6 != 0) {
        task->state = 6;
        goto L_tail;
    }
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->idMap);
    if (work->fwd != 0) {
        work->prevSide = q->field_1F0;
        q->field_1F0   = 0;
    }
    if (work->back != 0) {
        work->prevSide = q->field_1F0;
        q->field_1F0   = 1;
    }
    if (work->fwd == 0 && work->back == 0) {
        goto L_tail;
    }
    q->field_1E8 = 1;
    side         = q->field_1F0;
    if (side == work->prevSide) {
        if (side == 0) {
            if (work->fwd != 1) {
                goto L_enqueue;
            }
            view = work->baseView;
        L_eqShared:
            q->field_1F8 = view;
            if (work->field_2E == 2) {
                frameOfs = q->field_1EA * 40;
                goto L_eqSetFlag;
            }
        } else {
            if (side != 1) {
                goto L_enqueue;
            }
            if (work->back != side) {
                goto L_enqueue;
            }
            view = work->baseView + 1;
            goto L_eqShared;
        }
        frameOfs = q->field_1EA * 10;
    L_eqSetFlag:
        work->field_2E = 1;
    } else {
        if (side == 0) {
            if (work->fwd != 1) {
                goto L_enqueue;
            }
            q->field_1F8 = work->baseView;
            if (work->field_2E == 2) {
                frameOfs = (work->streamLen - q->field_1EA) * 40;
            } else {
                frameOfs = (work->streamLen - q->field_1EA) * 10;
            }
            work->field_2E = 1;
        } else if (side == 1) {
            if (work->back != side) {
                goto L_enqueue;
            }
            q->field_1F8 = work->baseView + 1;
            if (work->field_2E == 2) {
                frameOfs = (work->streamLen - q->field_1EA) * 40;
            } else {
                frameOfs = (work->streamLen - q->field_1EA) * 10;
            }
            work->field_2E = 1;
        }
    }
L_enqueue:
    slot[0]   = Stream_FindSlot(&Game_Session->field_4, q->field_1F8, 0);
    seekFrame = frameOfs & 0xFFFF;
    slot[1]   = seekFrame >> 8;
    slot[2]   = seekFrame;
    CdCmd_Enqueue(0x71, 0, slot);
    q->field_1FA = 0;
    task->state  = task->state + 1;
    goto L_tail;

L_case3:
    if (q->field_1FA != 0) {
        task->state = task->state + 1;
    }
    goto L_tail;

L_case4:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->idMap);
    if (q->field_1F6 != 0) {
        task->state = 6;
        goto L_tail;
    }
    if ((work->fwd == 0 && q->field_1F0 == 0) || (work->back == 0 && q->field_1F0 == 1) ||
        work->fwd != work->prevFwd || work->back != work->prevBack) {
        q->field_1E8 = 0;
        task->state  = task->state + 1;
    }
    goto L_tail;

L_case5:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->idMap);
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        goto L_tail;
    }
    task->state = 2;
    goto L_case2;

L_case6:
    plaza_updateEdgeFlags((AcropolisPlazaSceneWork*)task->idMap);
    switch (q->field_1F8) {
        case 0:
        case 2:
            if (work->back == 0) {
                goto L_tail;
            }
            break;
        case 1:
        case 3:
            if (work->fwd == 0) {
                goto L_tail;
            }
            break;
        default:
            goto L_tail;
    }
    q->field_1F6 = 0;
    task->state  = 2;

L_tail:
    if (q->field_1FA != 0) {
        func_acropolis_plaza_8017DD90(task);
        func_acropolis_plaza_8017DE24(q->field_1F8);
    }
    work->prevBack = work->back;
    work->prevFwd  = work->fwd;
}

/// Pad word that closes this unit's `.rodata`. The original object aligned its
/// section to 8 after the jump table above, which is what puts the next unit's
/// tables at 0x8017D624; nothing reads it.
const u32 D_acropolis_plaza_8017D620 = 0;
