#include "common.h"

#include <psyq/libgte.h>
#include <psyq/abs.h>

#include "actors/actor.h"
#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3E9C.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"

/// Work block of the overlay's walker, allocated zeroed by its spawn routine
/// and kept at `Task::work`: a nineteen-part rig, the walk state, and
/// `freeCountdown`, the frames until the model buffers are freed, -1
/// disabling the countdown.
typedef struct Actor335800Work {
    ActorAnimRig19  rig;
    ActorModelState model;
    ActorWalkState  walk;
    s16             freeCountdown;
    byte            pad_4C6[0x2];
} Actor335800Work;
STATIC_ASSERT_SIZEOF(Actor335800Work, 0x4C8);

/// Work block of the overlay's parent walker, allocated zeroed by its spawn
/// routine and kept at `Task::work`: a twenty-part rig and the walk state,
/// the two child tasks the spawn routine starts, whose models the visibility
/// command drives alongside the walker's, `freeCountdown`, the frames until
/// the model buffers are freed, -1 disabling the countdown, and `field_508`,
/// the flag bits of the last animation record the tick latched.
typedef struct Actor335800MainWork {
    ActorAnimRig20  rig;
    ActorModelState model;
    ActorWalkState  walk;
    Task*           child0;
    Task*           child1;
    s16             field_504;
    s16             freeCountdown;
    s32             field_508;
} Actor335800MainWork;
STATIC_ASSERT_SIZEOF(Actor335800MainWork, 0x50C);

/// Optional start animation for `func_actor_335800_80162C80`: the preset's
/// `field_4` and the `model.nextAnimId` byte. Absent, the defaults are 0xD and 1.
typedef struct Actor335800SpawnAnim {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u8  field_4;
} Actor335800SpawnAnim;

/// Start pose `func_actor_335800_80161E88` places its root part at: a
/// translation plus the Euler angles for `RotMatrix`.
typedef struct Actor335800Pose {
    /* 0x00 */ s32     x;
    /* 0x04 */ s32     y;
    /* 0x08 */ s32     z;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR rot;
} Actor335800Pose;

extern Actor335800Pose D_actor_335800_80164F80;
/// Second label on `D_actor_335800_80164F80.z`: the height the rising part
/// starts decelerating past.
extern s32 D_actor_335800_80164F88;

void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);

extern s8       D_8007272D;
extern u8       D_8007216D;
extern TaskDesc D_80182834;

extern TaskDesc  D_actor_335800_80164DE0;
extern GpAnimArg D_actor_335800_80164E7C;

/// The warp-payload table the two dispatchers reach by entry:
/// `func_actor_335800_801621B4` selects `n * 3` 8-byte units of it.
extern GpXformArg D_actor_335800_80164EA4[];
extern s32        D_actor_335800_80164EBC;
extern s32        D_actor_335800_80164ED4;
extern s32        D_actor_335800_80165FC0;
extern s32        D_actor_335800_80166098;

/// Animation bank tables of the parent and the child block.
extern void* D_actor_335800_8016EAD8[];
extern void* D_actor_335800_80172E98[];

/// The two part tasks the parent block spawns, and its message table; both
/// live in this overlay's trailing data.
extern TaskDesc   D_actor_335800_8016EADC;
extern GpMsgEntry D_actor_335800_8016EB00[];

/// `Gp_DispatchMsg` handler table installed at `Task::msgTable` by
/// `func_actor_335800_80163AA0`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_335800_80172EA8[];

void func_actor_335800_80162640(Task* arg0);
void func_actor_335800_80162844(Task* task);
void func_actor_335800_80162B3C(Task* arg0);
void func_actor_335800_80162E8C(Task* task);
void func_actor_335800_80162F08(Task* task);
void func_actor_335800_80162F7C(Task* arg0);
void func_actor_335800_80162F9C(Task* arg0);
void func_actor_335800_80162FF4(Task* arg0);
void func_actor_335800_80162FFC(Task* task);
void func_actor_335800_80163064(Task* task);
void func_actor_335800_80163124(Task* task);
void func_actor_335800_801631A4(Task* arg0);
s32  func_actor_335800_801632A4(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3);
void func_actor_335800_80163568(Task* task);
void func_actor_335800_8016373C(Task* arg0);
void func_actor_335800_80163AA0(Task* arg0);
void func_actor_335800_80163B34(Task* arg0);
void func_actor_335800_80163B54(Task* arg0);
void func_actor_335800_80163B70(Task* arg0);
void func_actor_335800_80163B78(Task* arg0);
void func_actor_335800_80163BE0(Task* task);
void func_actor_335800_80163CA0(Task* task);
void func_actor_335800_80163D20(Task* arg0);
s32  func_actor_335800_80163E20(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3);

/// Spawn, tick and teardown handlers of the two part tasks the parent block
/// spawns, dispatched by `func_actor_335800_80162E34`.
const TaskFuncTable3 D_actor_335800_80161E24 = { {
    func_actor_335800_80162E8C,
    func_actor_335800_80162F08,
    taskKill,
} };

/// Spawn, tick and teardown handlers of the parent block, dispatched by
/// `func_actor_335800_80162F10`.
const TaskFuncTable3 D_actor_335800_80161E30 = { {
    func_actor_335800_80162640,
    func_actor_335800_80162844,
    func_actor_335800_80162F7C,
} };

/// Step handlers of the parent block's motion sequence, indexed by
/// `ActorWalkState::motionStep`: turn to face `target`, start walking
/// forward, walk until arrival, then turn to the placement yaw.
const TaskFuncTable4 D_actor_335800_80161E3C = { {
    func_actor_335800_80163064,
    func_actor_335800_80163124,
    func_actor_335800_80162B3C,
    func_actor_335800_801631A4,
} };

/// The constant local-space offset `func_actor_335800_80163124` rotates for
/// the parent block: straight ahead along the part's own +Z.
const VECTOR D_actor_335800_80161E4C = { 0, 0, 0x200000, 0 };

/// Spawn, tick and teardown handlers of the child block, dispatched by
/// `func_actor_335800_80163A34`.
const TaskFuncTable3 D_actor_335800_80161E5C = { {
    func_actor_335800_80163AA0,
    func_actor_335800_80163568,
    func_actor_335800_80163B34,
} };

/// Step handlers of the child block's motion sequence, indexed by
/// `ActorWalkState::motionStep`, in the same order as the parent's.
const TaskFuncTable4 D_actor_335800_80161E68 = { {
    func_actor_335800_80163BE0,
    func_actor_335800_80163CA0,
    func_actor_335800_8016373C,
    func_actor_335800_80163D20,
} };

/// The child block's copy of the forward offset, rotated by
/// `func_actor_335800_80163CA0`.
const VECTOR D_actor_335800_80161E78 = { 0, 0, 0x200000, 0 };

/// Places the root part 1000 units short of its pose `D_actor_335800_80164F80`
/// and moves it along Z at `killCountdown` (100) per frame; once past the pose
/// height the velocity drops by 6 a frame until it falls below -60, which ends
/// the move. A ground shadow is drawn every frame. The
/// task kills itself once the session's `viewReady` flag is set, or a few
/// frames into state 2.
void func_actor_335800_80161E88(Task* task)
{
    GpCoord* coord;
    VECTOR3  pos;
    SVECTOR* rot;

    coord = task->extra.tmd->coords;
    switch (task->state) {
        case 0:
            coord->coord.t[0]   = D_actor_335800_80164F80.x;
            coord->coord.t[1]   = D_actor_335800_80164F80.y;
            coord->coord.t[2]   = D_actor_335800_80164F80.z - 1000;
            rot                 = &D_actor_335800_80164F80.rot;
            coord->param.rot.vx = rot->vx;
            coord->param.rot.vy = rot->vy;
            coord->param.rot.vz = rot->vz;
            RotMatrix(&coord->param.rot, &coord->coord);
            coord->flg          = 0;
            task->killCountdown = 100;
            task->state++;
        case 1:
            if (D_actor_335800_80164F88 < coord->coord.t[2]) {
                task->killCountdown -= 6;
                if (task->killCountdown < -60) {
                    task->killCountdown = 0;
                    task->state++;
                }
            }
            coord->coord.t[2] += task->killCountdown;
            if (gGameSession->viewReady != 0) {
                taskKill(task);
            }
            break;
        case 2:
            if (++task->killCountdown < 4) {
                if (gGameSession->viewReady != 0) {
                    taskKill(task);
                }
            } else {
                taskKill(task);
            }
            break;
        default:
            taskKill(task);
            break;
    }
    if (func_800EA1A8((VECTOR3*)task->extra.tmd->coords->workm.t, &pos) != 0) {
        Gp_DrawEffGroundQuad(&pos, 0x800, Gp_State1C->groundShade);
    }
}

/// Script callback: queues the replacement overlay load.
void func_actor_335800_80162040(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Script callback: queues the overlay load.
void func_actor_335800_80162060(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Script callback: restores the stream random state.
void func_actor_335800_80162080(void)
{
    Gp_RestoreStreamRng();
}

/// Script callback: cancels the pending overlay replacement and activates the
/// loaded one.
void func_actor_335800_801620A0(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_335800_801620C0(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 0, 0, 0);
}

void func_actor_335800_801620F0(u8 arg0)
{
    gGameSession->at4.loc.room = D_8007216D = arg0;
    gGameSession->roomObjsDirty             = 1;
}

void func_actor_335800_80162114(void)
{
    Task*      slot;
    TmdObject* extra;
    GpCoord*   coord;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        extra = slot->extra.tmd;
        coord = extra->coords;
        if ((u32)(coord->coord.t[2] - 0xC53) < 0x96F) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164ED4, 0);
        }
        if ((u32)(coord->coord.t[2] - 0x3E9) < 0x86A) {
            Gp_DispatchMsg(slot, 0x3E9, (s32)&D_actor_335800_80164EBC, 0);
        }
    }
}

void func_actor_335800_801621B4(s32 arg0)
{
    Task*      slot;
    TmdObject* extra;
    GpCoord*   coord;
    s32        msgId;
    s32        lowIdx;
    s32        highIdx;
    s32        unit;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        extra  = slot->extra.tmd;
        coord  = extra->coords;
        lowIdx = 1;
        if (arg0 != 0) {
            highIdx = 2;
        } else {
            lowIdx  = 3;
            highIdx = 4;
        }
        msgId = 0x3E9;
        if (coord->coord.t[2] >= 0xC53) {
            unit = highIdx * 3;
        } else {
            unit = lowIdx * 3;
            SOFT_USE_REG(msgId);
        }
        Gp_DispatchMsg(slot, msgId, (s32)((unit * 8) + (s32)D_actor_335800_80164EA4), 0);
    }
}

void func_actor_335800_8016224C(void)
{
    register u8 areaId asm("a0");
    Task*       slot;
    TmdObject*  extra;
    GpCoord*    coord;

    slot = gameGetPtrSlot(3);
    if (slot != NULL) {
        areaId = 6;
        extra  = slot->extra.tmd;
        coord  = extra->coords;
        if (coord->coord.t[2] >= 0xC53) {
            areaId = 5;
        }
        Mc_SaveData.at4.loc.view    = areaId;
        gGameSession->at4.loc.view  = areaId;
        gGameSession->viewDirty     = 1;
        gGameSession->roomObjsDirty = 1;
    }
}

/// Sets `GpSprtCmd::field_4` on two sprite commands of the area's 39th view
/// record: 1 keeps their sprites out of the ordering table and also sets game
/// flag 0x7F's nibble to 1, 0 draws them again.
void func_actor_335800_801622C0(s32 arg0)
{
    GameSession* g;
    GpAreaKey*   sess;
    GpSprtRec*   rec;
    GpSprtCmd*   cmd;

    g    = gGameSession;
    sess = &g->at4.loc;
    rec  = Gp_SprtTables[sess->stage - 1][g->sprtVariant - 1].field_0[sess->area - 1];
    switch (arg0) {
        case 0:
            cmd            = rec[38].field_4;
            cmd[2].field_4 = 0;
            cmd[3].field_4 = 0;
            break;
        case 1:
            cmd            = rec[38].field_4;
            cmd[2].field_4 = arg0;
            cmd[3].field_4 = arg0;
            GameFlag_SetNibble(0x7F, 1);
            break;
    }
}

void func_actor_335800_80162364(Task* arg0)
{
    if (arg0->state == 0) {
        if (arg0->spawnArg1 != 0) {
            func_800E8614((s32)&D_actor_335800_80166098, 0);
        } else {
            func_800E8614((s32)&D_actor_335800_80165FC0, 0);
        }
        arg0->state += 1;
        return;
    }
    taskKill(arg0);
}

void func_actor_335800_801623D8(void)
{
    Task_SpawnFromTable(&D_80182834, 0, 0, 0);
}

void func_actor_335800_80162408(void)
{
    SetDispMask(1);
}

void func_actor_335800_80162428(s8 arg0)
{
    D_8007272D = arg0;
}

void func_actor_335800_80162434(s32 arg0)
{
    SndEvt_EnqueueType2(D_80062735, arg0 & 0xFFFF);
}

void func_actor_335800_80162460(void)
{
    func_800E3FAC(0xA2, 0x18);
}

void func_actor_335800_80162484(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_actor_335800_801624B8(s32 arg0)
{
    GameFlag_SetNibble(0x108, arg0);
}

void func_actor_335800_801624DC(Task* arg0)
{
    Task* slot;

    if (gGameSession->field_126 != 0) {
        slot = gameGetPtrSlot(3);
        Gp_PlayerWeaponId(&D_actor_335800_80164E7C.animBlock.index);
        Gp_DispatchMsg(slot, 0x3E8, (s32)&D_actor_335800_80164E7C, 0);
        taskKill(arg0);
    }
}

void func_actor_335800_80162558(void)
{
    Task_SpawnFromTable(&D_actor_335800_80164DE0, 3, 0, 0);
}

void func_actor_335800_80162588(Task* arg0)
{
    s8  var_a0;
    u8  temp_v1;
    s32 count;

    if ((gGameSession->eventState != 0) && (gGameSession->evtSkipped == 0)) {
        temp_v1 = gGameSession->padScriptFlags;
        var_a0  = 0;
        if (temp_v1 & 1) {
            count  = (u16)arg0->killCountdown;
            var_a0 = count & 1;
        }
        if ((temp_v1 & 2) && !(arg0->killCountdown & 1)) {
            var_a0 = -1;
        }
        Display_ClampField126(var_a0);
        arg0->killCountdown += 1;
        return;
    }
    Display_ClampField126(0);
    taskKill(arg0);
}

void func_actor_335800_80162640(Task* arg0)
{
    Actor335800MainWork* work;
    GpAreaKey            key;
    GpAreaKey*           sessionKey;
    u8*                  keyAddr;
    Task*                spawned;

    work = (Actor335800MainWork*)memCalloc(0x50C, false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }
    arg0->work          = (TaskIdMap*)work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;
    spawned             = Task_SpawnFromTable(&D_actor_335800_8016EADC, 1, 4, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->child0 = spawned;
        model        = spawned->extra.tmd;
        idx          = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        sessionKey   = (GpAreaKey*)&gGameSession->at4.loc;
        key.stage    = sessionKey->stage;
        key.area     = sessionKey->area;
        key.room     = sessionKey->room;
        key.view     = sessionKey->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    spawned = Task_SpawnFromTable(&D_actor_335800_8016EADC, 2, 8, (s32)arg0);
    if (spawned != NULL) {
        TmdObject*   model;
        GpAreaRec*   rec;
        GpAreaPlace* place;
        s32          idx;

        work->child1 = spawned;
        model        = spawned->extra.tmd;
        idx          = ((GpEnemy*)arg0->spawnArg2)->placeKey >> 12;
        sessionKey   = (GpAreaKey*)(keyAddr = (u8*)&gGameSession->at4.loc.view);
        key.stage    = sessionKey->stage;
        key.area     = sessionKey->area;
        key.room     = ((GpAreaKey*)keyAddr)->room;
        key.view     = ((GpAreaKey*)(&gGameSession->at4.loc.view))->view;
        Gp_SyncAreaKeyIndex(&key);
        rec          = Gp_GetNestedAreaRec(&key);
        place        = (GpAreaPlace*)((idx << 4) + (s32)rec->field_0);
        model->tpage = place->tpage;
        model->clut  = place->clut;
        if (model->buffer != NULL) {
            tmdProcessStream(model);
            tmdProcessStream(model);
        }
    }
    func_actor_335800_80162F9C(arg0);
    arg0->msgTable     = D_actor_335800_8016EB00;
    arg0->exitCallback = func_actor_335800_80162F7C;
    arg0->state       += 1;
}

void func_actor_335800_80162844(Task* task)
{
    TmdObject*           ext      = task->extra.tmd;
    Actor335800MainWork* work     = (Actor335800MainWork*)task->work;
    TaskFunc             funcs[2] = { func_actor_335800_80162FF4, func_actor_335800_80162FFC };
    VECTOR3              pos;
    GpCoord*             coord;
    GpAnimRec*           rec;
    s32                  i;
    s32                  j;

    funcs[work->walk.motion](task);
    coord                = task->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->flg           = 0;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (!(ext->flags & 0x80)) {
        if (work->model.ticking != 0) {
            for (i = 1; i < 0x14; i++) {
                Gp_AnimTickIndex(&work->rig.anim, i);
            }
            rec = Gp_AnimGetRec(&work->rig.anim, &work->rig.slots[1]);
            if (rec != NULL) {
                if (!(rec->flags & 0x20) && (work->field_508 & 0x20)) {
                    Gp_SpawnEff(0x600A1, &task->extra.tmd->coords[8], 0xD, NULL);
                }
                work->field_508 = rec->flags & 0x30;
            }
        }
        if (work->field_504 == 0) {
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 3; j++) {
                    work->model.color.m[i][j] >>= 1;
                    work->model.light.m[i][j] >>= 1;
                }
                work->model.color.t[i] >>= 1;
                work->model.light.t[i] >>= 1;
            }
            work->field_504 = -1;
        }
        if (work->field_504 > 0) {
            if (func_800EA1A8((VECTOR3*)task->extra.tmd->coords[1].workm.t, &pos) != 0) {
                Gp_DrawEffGroundQuad(&pos, 0x300, Gp_State1C->groundShade);
            }
        }
    }
    if (gGameSession->viewReady != 0) {
        work->field_504 = 1;
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// Arrival check for the parent block: once the X/Z distances from the root
/// coordinate to `target` stop shrinking below `limit`, plays anim 0x7D3,
/// clears `step` and advances `walk.motionStep`; otherwise records the distances.
void func_actor_335800_80162B3C(Task* arg0)
{
    Actor335800MainWork* work;
    GpCoord*             coord;
    SVECTOR              d;
    s32                  dx;
    s32                  dz;
    GpAnimArg            preset;

    work  = (Actor335800MainWork*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_335800_801632A4(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Placement handler for the parent block: stores the spawn position and
/// rotation, then applies a start preset exactly as `func_actor_335800_801632A4`
/// does (inlined here).
s32 func_actor_335800_80162C80(Task* task, s32 arg1, GpXformArg* place, Actor335800SpawnAnim* anim)
{
    Actor335800MainWork* work;
    Actor335800MainWork* w;
    GpAnimArg            preset;
    GpAnimArg*           msg;
    s32                  i;
    TmdObject*           ext;

    w                      = (Actor335800MainWork*)task->work;
    w->walk.motion         = 1;
    w->walk.motionStep     = 0;
    w->walk.target.vx      = place->pos.vx;
    w->walk.target.vy      = place->pos.vy;
    w->walk.target.vz      = place->pos.vz;
    w->walk.rotX           = place->rot.vx;
    w->walk.rotY           = place->rot.vy;
    w->walk.rotZ           = place->rot.vz;
    preset.animBlock.index = 0;
    if (anim != NULL) {
        preset.field_4      = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        preset.field_4      = 0xD;
        w->model.nextAnimId = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor335800MainWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_8016EAD8[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->field_4;
    if (msg->field_8 != 0 && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// State dispatcher of the child part tasks: copies the three-handler table
/// onto the stack and runs the entry `Task::state` selects.
void func_actor_335800_80162E34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E24;
    sp.funcs[task->state](task);
}

void func_actor_335800_80162E8C(Task* task)
{
    Task*      parent;
    s32        part;
    TmdObject* extra;
    TmdObject* parentExtra;
    GpCoord*   coord;
    GpCoord*   dest;

    parent          = (Task*)task->spawnArg2;
    part            = task->spawnArg1;
    extra           = task->extra.tmd;
    parentExtra     = parent->extra.tmd;
    coord           = extra->coords;
    dest            = &parentExtra->coords[part];
    coord->flg      = 0;
    coord->sub      = dest;
    extra->lightMtx = parentExtra->lightMtx;
    extra->colorMtx = parentExtra->colorMtx;
    Task_Reparent(parent, task);
    task->state += 1;
}

void func_actor_335800_80162F08(Task* task)
{
}

/// State dispatcher of the parent block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80162F10(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E30;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

void func_actor_335800_80162F7C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80162F9C(Task* arg0)
{
    TmdObject*           ext;
    Actor335800MainWork* work;

    work          = (Actor335800MainWork*)arg0->work;
    ext           = arg0->extra.tmd;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
    func_800D7A9C(ext, (VECTOR*)arg0->extra.tmd->coords[1].workm.t, 0, 3);
    work->field_504 = 1;
}

void func_actor_335800_80162FF4(Task* arg0)
{
}

/// Motion handler 1 of the parent block: copies the step-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
void func_actor_335800_80162FFC(Task* task)
{
    Actor335800MainWork* work;
    TaskFuncTable4       fns;

    work = (Actor335800MainWork*)task->work;
    fns  = D_actor_335800_80161E3C;
    fns.funcs[work->walk.motionStep](task);
}

/// Step 0: turns the root part to face `work->walk.target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances the step.
void func_actor_335800_80163064(Task* task)
{
    Actor335800MainWork* work;
    GpCoord*             coord;
    VECTOR               delta;
    SVECTOR              dir;
    SVECTOR              rot;

    work  = (Actor335800MainWork*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    work->walk.motionStep++;
}

/// Step 1: rotates the constant forward offset `D_actor_335800_80161E4C`
/// through the root part's matrix into `work->walk.step`, opens the per-axis stop
/// threshold to 0x7FFF, which disables it, and advances the step.
void func_actor_335800_80163124(Task* task)
{
    Actor335800MainWork* work;
    GpCoord*             coord;
    VECTOR               vec;

    coord = task->extra.tmd->coords;
    work  = (Actor335800MainWork*)task->work;

    vec = D_actor_335800_80161E4C;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// Turn-to-face handler: Euler-extracts the root coordinate into `vec`, and
/// while the yaw gap to `work->walk.rotY` is at least 0x41 it steps `vec.vy`
/// toward it by 0x40, taking the step on an `s32` widening of the extracted
/// yaw; otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `model.nextAnimId` byte, clearing the two body counters.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
void func_actor_335800_801631A4(Task* arg0)
{
    Actor335800MainWork* work;
    GpMtxWords*          words;
    GpCoord*             coord;
    SVECTOR              vec;
    GpAnimArg            preset;
    s32                  vy;
    s16                  diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor335800MainWork*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                 = work->walk.rotY;
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_335800_801632A4(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Animation preset handler for the parent block, the 20-slot twin of
/// `func_actor_335800_80163E20`: re-seeds the slot array off bank table
/// `D_actor_335800_8016EAD8` when the preset's bank index changes, then
/// restarts or resets every slot and ticks them.
s32 func_actor_335800_801632A4(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor335800MainWork* work;
    TmdObject*           ext;
    s32                  i;

    work = (Actor335800MainWork*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_8016EAD8[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->field_4;
    if (msg->field_8 != 0 && work->model.ticking != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// Message 0x7D4 handler of the parent block: places the root part at the
/// message's position and Euler angles, rebuilding the rotation from them and
/// clearing `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_335800_801633C0(Task* task, s32 arg1, GpXformArg* args)
{
    GpCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_335800_8016343C(Task* task, s32 arg1, s32 mode)
{
    Actor335800MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    s32                  ret;

    work = (Actor335800MainWork*)task->work;
    obj  = task->extra.tmd;
    objA = work->child0->extra.tmd;
    objB = work->child1->extra.tmd;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags         |= 0x80;
            work->freeCountdown = mode;
            obj->flags         |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    objA->flags = obj->flags;
    objB->flags = obj->flags;
    return ret;
}

s32 func_actor_335800_8016354C(Task* arg0, s32 arg1, GpCmdArg* arg2, s32 arg3)
{
    Actor335800MainWork* work;

    work = (Actor335800MainWork*)arg0->work;
    if (arg2->command == 0) {
        work->field_504 = 0;
    }
    return 0;
}

/// Per-frame tick of the child block: runs the motion handler `walk.motion`
/// selects, adds the 16.16 velocity `walk.step` onto the accumulator `walk.acc`,
/// moves the coordinate by the integer part and keeps only the fraction, then
/// ticks the animation slots, draws the ground shadow and rebuilds the colour
/// matrix while visible, and counts `freeCountdown` down to the buffer free.
void func_actor_335800_80163568(Task* task)
{
    TmdObject*       ext      = task->extra.tmd;
    Actor335800Work* work     = (Actor335800Work*)task->work;
    TaskFunc         funcs[2] = { func_actor_335800_80163B70, func_actor_335800_80163B78 };
    VECTOR3          pos;
    GpCoord*         coord;
    s32              i;

    funcs[work->walk.motion](task);
    coord                = task->extra.tmd->coords;
    work->walk.acc[0].w += work->walk.step.vx;
    work->walk.acc[1].w += work->walk.step.vy;
    work->walk.acc[2].w += work->walk.step.vz;
    coord->coord.t[0]   += (s16)(work->walk.acc[0].w >> 16);
    coord->coord.t[1]   += (s16)(work->walk.acc[1].w >> 16);
    coord->coord.t[2]   += (s16)(work->walk.acc[2].w >> 16);
    coord->flg           = 0;
    work->walk.acc[0].w  = (u16)work->walk.acc[0].w;
    work->walk.acc[1].w  = (u16)work->walk.acc[1].w;
    work->walk.acc[2].w  = (u16)work->walk.acc[2].w;
    if (work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimTickIndex(&work->rig.anim, i);
        }
    }
    if (!(ext->flags & 0x80)) {
        if (func_800EA1A8((VECTOR3*)task->extra.tmd->coords[1].workm.t, &pos) != 0) {
            Gp_DrawEffGroundQuad(&pos, 0x200, Gp_State1C->groundShade);
        }
        Gp_UpdateCoord(&task->extra.tmd->coords[1]);
        func_800D7A9C(ext, (VECTOR*)task->extra.tmd->coords[1].workm.t, 0, 3);
    }
    if (work->freeCountdown >= 0) {
        if (work->freeCountdown == 0) {
            Tmd_FreeBuffers(ext);
        }
        work->freeCountdown--;
    }
}

/// Approach test for the child block, the twin of `func_actor_335800_80162B3C`:
/// once the X/Z distance to `target` stops shrinking below `limit`, plays anim
/// 0x7D3, clears `step` and advances the state; otherwise records the distance.
void func_actor_335800_8016373C(Task* arg0)
{
    Actor335800Work* work;
    GpCoord*         coord;
    SVECTOR          d;
    s32              dx;
    s32              dz;
    GpAnimArg        preset;

    work  = (Actor335800Work*)arg0->work;
    coord = arg0->extra.tmd->coords;
    if (work->walk.target.vx - coord->coord.t[0] >= 0) {
        dx = (u16)work->walk.target.vx - (u16)coord->coord.t[0];
    } else {
        dx = (u16)coord->coord.t[0] - (u16)work->walk.target.vx;
    }
    d.vx = dx;
    if (work->walk.target.vz - coord->coord.t[2] >= 0) {
        dz = (u16)work->walk.target.vz - (u16)coord->coord.t[2];
    } else {
        dz = (u16)coord->coord.t[2] - (u16)work->walk.target.vz;
    }
    d.vz = dz;
    if (d.vx >= work->walk.limit.vx && d.vz >= work->walk.limit.vz) {
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_335800_80163E20(arg0, 0x7D3, &preset, 0);
        work->walk.step.vx = 0;
        work->walk.step.vy = 0;
        work->walk.step.vz = 0;
        work->walk.motionStep++;
        return;
    }
    work->walk.limit.vx = d.vx < 0 ? -d.vx : d.vx;
    work->walk.limit.vz = d.vz < 0 ? -d.vz : d.vz;
}

/// Placement handler for the child block, the twin of
/// `func_actor_335800_80162C80`: stores the spawn position and rotation, then
/// applies a start preset exactly as `func_actor_335800_80163E20` does
/// (inlined here).
s32 func_actor_335800_80163880(Task* task, s32 arg1, GpXformArg* place, Actor335800SpawnAnim* anim)
{
    Actor335800Work* work;
    Actor335800Work* w;
    GpAnimArg        preset;
    GpAnimArg*       msg;
    s32              i;
    TmdObject*       ext;

    w                      = (Actor335800Work*)task->work;
    w->walk.motion         = 1;
    w->walk.motionStep     = 0;
    w->walk.target.vx      = place->pos.vx;
    w->walk.target.vy      = place->pos.vy;
    w->walk.target.vz      = place->pos.vz;
    w->walk.rotX           = place->rot.vx;
    w->walk.rotY           = place->rot.vy;
    w->walk.rotZ           = place->rot.vz;
    preset.animBlock.index = 0;
    if (anim != NULL) {
        preset.field_4      = anim->field_0;
        w->model.nextAnimId = anim->field_4;
    } else {
        preset.field_4      = 0xD;
        w->model.nextAnimId = 1;
    }
    preset.field_8  = 1;
    preset.field_C  = 5;
    preset.field_10 = 1;

    msg  = &preset;
    work = (Actor335800Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->field_4;
    if (msg->field_8 != 0 && work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// State dispatcher of the child block: copies its three-handler table onto
/// the stack and, unless the game is frozen, runs the entry `Task::state`
/// selects.
void func_actor_335800_80163A34(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_actor_335800_80161E5C;
    if (Gp_StateF0.field_4 == 0) {
        sp.funcs[task->state](task);
    }
}

/// Spawn state of the enemy actor: allocates the 0x4C8-byte work block that
/// every later handler reads through `Task::work`, seeds the two -1 bytes,
/// the -1 halfword and three cleared words the work's own init expects,
/// republishes the light and colour matrices onto the display object, then
/// installs the message table and the exit handler. An allocation failure
/// ends the task instead of leaving a half-built actor behind.
void func_actor_335800_80163AA0(Task* arg0)
{
    Actor335800Work* work;

    work = memCalloc(sizeof(Actor335800Work), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->work          = (TaskIdMap*)work;
    work->model.animId  = -1;
    work->model.bank    = -1;
    work->freeCountdown = -1;
    work->walk.acc[0].w = 0;
    work->walk.acc[1].w = 0;
    work->walk.acc[2].w = 0;

    func_actor_335800_80163B54(arg0);

    arg0->msgTable     = D_actor_335800_80172EA8;
    arg0->exitCallback = func_actor_335800_80163B34;
    arg0->state       += 1;
}

void func_actor_335800_80163B34(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

void func_actor_335800_80163B54(Task* arg0)
{
    TmdObject*       ext;
    Actor335800Work* work;

    ext           = arg0->extra.tmd;
    work          = (Actor335800Work*)arg0->work;
    ext->lightMtx = &work->model.light;
    ext->colorMtx = &work->model.color;
}

void func_actor_335800_80163B70(Task* arg0)
{
}

/// Motion handler 1 of the child block: copies the four-handler table onto
/// the stack and runs the entry `walk.motionStep` selects.
void func_actor_335800_80163B78(Task* arg0)
{
    TaskFuncTable4   sp;
    Actor335800Work* work;

    work = (Actor335800Work*)arg0->work;
    sp   = D_actor_335800_80161E68;
    sp.funcs[(s16)work->walk.motionStep](arg0);
}

/// State handler 0 of the child block's table `D_actor_335800_80161E68`:
/// turns the root part to face `work->walk.target`, taking the yaw of the
/// normalised offset from the part's own translation with `ratan2` and
/// rebuilding the local matrix from that yaw alone, then advances
/// `walk.motionStep` to the next handler.
void func_actor_335800_80163BE0(Task* task)
{
    Actor335800Work* work;
    GpCoord*         coord;
    VECTOR           delta;
    SVECTOR          dir;
    SVECTOR          rot;

    work  = (Actor335800Work*)task->work;
    coord = task->extra.tmd->coords;

    delta.vx = work->walk.target.vx - coord->coord.t[0];
    delta.vy = work->walk.target.vy - coord->coord.t[1];
    delta.vz = work->walk.target.vz - coord->coord.t[2];
    VectorNormalS(&delta, &dir);

    rot.vx = 0;
    rot.vy = ratan2(dir.vx, dir.vz);
    rot.vz = 0;

    coord->param.rot.vx = rot.vx;
    coord->param.rot.vy = rot.vy;
    coord->param.rot.vz = rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    work->walk.motionStep++;
}

/// State handler 1 of the child block's table `D_actor_335800_80161E68`, the
/// step after the turn-to-face handler `func_actor_335800_80163BE0`: rotates
/// the constant forward offset `D_actor_335800_80161E78` through the root
/// part's matrix into `work->walk.step`, opens the per-axis stop threshold to
/// 0x7FFF, which disables it for the update loop, and advances `walk.motionStep` so
/// the dispatcher `func_actor_335800_80163B78` runs the next handler.
void func_actor_335800_80163CA0(Task* task)
{
    Actor335800Work* work;
    GpCoord*         coord;
    VECTOR           vec;

    coord = task->extra.tmd->coords;
    work  = (Actor335800Work*)task->work;

    vec = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->walk.step);
    work->walk.limit.vx = 0x7FFF;
    work->walk.limit.vy = 0x7FFF;
    work->walk.limit.vz = 0x7FFF;
    work->walk.motionStep++;
}

/// State handler 3 of `D_actor_335800_80161E68`, the child block's
/// turn-to-yaw step: Euler-extracts the root coordinate into `vec`, and while
/// the yaw gap to `work->walk.rotY` is at least 0x41 it steps `vec.vy` toward
/// it by 0x40, taking the step on an `s32` widening of the extracted yaw;
/// otherwise it snaps the yaw to the target and plays anim 0x7D3 with a
/// preset carrying the `model.nextAnimId` byte, clearing the motion index and step.
/// Either way the root coordinate is rebuilt as the identity matrix rotated by
/// `vec`.
void func_actor_335800_80163D20(Task* arg0)
{
    Actor335800Work* work;
    GpMtxWords*      words;
    GpCoord*         coord;
    SVECTOR          vec;
    GpAnimArg        preset;
    s32              vy;
    s16              diff;

    coord = arg0->extra.tmd->coords;
    work  = (Actor335800Work*)arg0->work;

    Gp_ExtractEuler(&vec, &coord->coord);
    diff = (u16)work->walk.rotY - (u16)vec.vy;
    if (ABS(diff) >= 0x41) {
        vy = vec.vy;
        if (diff < 0) {
            vec.vy = vy - 0x40;
        } else {
            vec.vy = vy + 0x40;
        }
    } else {
        vec.vy                 = work->walk.rotY;
        preset.animBlock.index = 0;
        preset.field_4         = work->model.nextAnimId;
        preset.field_8         = 1;
        preset.field_C         = 5;
        preset.field_10        = 0;
        func_actor_335800_80163E20(arg0, 0x7D3, &preset, 0);
        work->walk.motion     = 0;
        work->walk.motionStep = 0;
    }

    words          = (GpMtxWords*)&coord->coord;
    words->m00_m01 = ONE;
    words->m02_m10 = 0;
    words->m11_m12 = ONE;
    words->m20_m21 = 0;
    words->m22     = ONE;
    RotMatrix(&vec, &coord->coord);
    coord->flg = 0;
}

/// Message 0x7D3 handler of the child block, the 19-slot counterpart of
/// `func_actor_335800_801632A4`: re-seeds the slot array off bank table
/// `D_actor_335800_80172E98` when the preset's bank index changes, then
/// restarts or resets every slot and ticks them.
s32 func_actor_335800_80163E20(Task* task, s32 arg1, GpAnimArg* msg, s32 arg3)
{
    Actor335800Work* work;
    TmdObject*       ext;
    s32              i;

    work = (Actor335800Work*)task->work;
    ext  = task->extra.tmd;
    if (msg->animBlock.index != work->model.bank) {
        work->model.bank = msg->animBlock.index;
        func_800B3F84(&work->rig.anim, D_actor_335800_80172E98[work->model.bank], ext, work->rig.poses,
                      work->rig.slots);
    }
    work->model.animId = msg->field_4;
    if (msg->field_8 != 0 && work->model.ticking != 0) {
        for (i = 1; i < 0x13; i++) {
            func_800B4114(&work->rig.anim, i, work->model.animId, 0, msg->field_C);
        }
    } else {
        for (i = 1; i < 0x13; i++) {
            Gp_AnimResetSlot(&work->rig.anim, i, work->model.animId);
        }
    }
    for (i = 1; i < 0x13; i++) {
        Gp_AnimTickIndex(&work->rig.anim, i);
    }
    work->model.ticking = 1;
    return 0;
}

/// Message 0x7D4 handler of the child block: places the root part at the
/// message's position and Euler angles, rebuilding the rotation from them and
/// clearing `flg` so the world matrix is recomputed. Returns 0.
s32 func_actor_335800_80163F3C(Task* task, s32 arg1, GpXformArg* args)
{
    GpCoord* coord;

    coord               = task->extra.tmd->coords;
    coord->coord.t[0]   = args->pos.vx;
    coord->coord.t[1]   = args->pos.vy;
    coord->coord.t[2]   = args->pos.vz;
    coord->param.rot.vx = args->rot.vx;
    coord->param.rot.vy = args->rot.vy;
    coord->param.rot.vz = args->rot.vz;
    RotMatrix(&coord->param.rot, &coord->coord);
    coord->flg = 0;
    return 0;
}

s32 func_actor_335800_80163FB8(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra.tmd;
    ret = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags                                   |= 0x80;
            ((Actor335800Work*)task->work)->freeCountdown = mode;
            obj->flags                                   |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_335800_80164098(void)
{
    return 0;
}
