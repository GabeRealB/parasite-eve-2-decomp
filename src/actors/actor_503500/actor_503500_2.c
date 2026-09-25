#include "common.h"

#include <psyq/abs.h>

#include "actors/actor_503500.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message payload at `D_actor_503500_8017655C`. `func_actor_503500_80132DEC`
/// fills it from the player actor: the three words are the translation of the
/// `GsCOORDINATE2` at `TmdObject::coords` (`MATRIX.t`), the three
/// halfwords the rotation triple at +0x50/+0x52/+0x54 of that task's `work`
/// block. `func_actor_503500_80132DD4` clears the position;
/// `func_actor_503500_80132E7C` hands the record to `Gp_DispatchMsg` as
/// message 0x3E9 while any position word is non-zero.
///
/// Size is bounded by the next `.bss` symbol in the overlay
/// (`D_actor_503500_80176574`, 0x18 bytes later).
typedef struct Actor503500MsgPos {
    /* 0x00 */ s32     x;
    /* 0x04 */ s32     y;
    /* 0x08 */ s32     z;
    /* 0x0C */ byte    pad_C[0x4];
    /* 0x10 */ SVECTOR rot;
} Actor503500MsgPos;
STATIC_ASSERT_SIZEOF(Actor503500MsgPos, 0x18);

/// Work block `func_actor_503500_80132778` allocates (`memCalloc(0xC)`) and
/// parks in `Task::work`. Each spawn packs `field_0 & 0xFFF` and
/// `field_4 & 0xF000` into the `Gp_SpawnEff` argument; `field_8` is a 16.16
/// period whose integer half is the `Task::killCountdown` limit between
/// spawns. Flag nibble 0x12A states 2..4 decay the first two and stretch the
/// period until it passes 0x10 and the task dies.
typedef struct Actor503500EffWork {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    union {
        /* 0x8 */ s32 w;
        struct {
            /* 0x8 */ s16 lo;
            /* 0xA */ s16 hi;
        } h;
    } field_8;
} Actor503500EffWork;
STATIC_ASSERT_SIZEOF(Actor503500EffWork, 0xC);

/// Spawn positions `func_actor_503500_80132778` indexes by `Task::spawnArg1`.
extern SVECTOR  D_actor_503500_8014B97C[];
extern Task*    D_actor_503500_80176558;
extern TaskDesc D_actor_503500_8014B964;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_503500_8014CD98[];
extern u8 D_actor_503500_8014D098[];
/// Lives in the room overlay slot: whatever room overlay is resident owns the
/// body.
extern void              func_8017E27C(s32 arg0);
extern Actor503500MsgPos D_actor_503500_8017655C;
/// Player-facing flag byte in the main executable; no module header owns it yet.
extern u8 D_80073BA9;
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
/// Player-facing mode byte in the main executable, also written by the
/// acropolis helicopter landing pad room.
extern s8 D_801153F1;

void func_actor_503500_80132778(Task* task)
{
    GsCOORDINATE2*      coord;
    GpMtxWords*         rot;
    Actor503500EffWork* work;
    SVECTOR*            pos;
    u8                  done;

    coord = ((TmdObject*)task->extra)->coords;
    if (task->state == 0) {
        pos               = &D_actor_503500_8014B97C[task->spawnArg1];
        coord->coord.t[0] = pos->vx;
        coord->coord.t[1] = pos->vy;
        coord->coord.t[2] = pos->vz;
        rot               = (GpMtxWords*)&coord->coord;
        rot->w0           = 0x1000;
        rot->w1           = 0;
        rot->w2           = 0x1000;
        rot->w3           = 0;
        rot->h4           = 0x1000;
        coord->flg        = 0;
        work              = memCalloc(0xC, false);
        if (work == NULL) {
            taskKill(task);
            return;
        }
        task->work      = (TaskIdMap*)work;
        work->field_0   = 0xC00;
        work->field_4   = 0x4000;
        work->field_8.w = 0x60000;
        task->state++;
    }
    work = (Actor503500EffWork*)task->work;
    if (D_801153F4 == 0) {
        if (work->field_8.h.hi < ++task->killCountdown) {
            task->killCountdown = 0;
            Gp_SpawnEff(0x6018C, coord,
                        (work->field_4 & 0xF000) | 0x03800000 | (work->field_0 & 0xFFF), NULL);
        }
    }
    switch (GameFlag_GetNibble(0x12A)) {
        case 0:
        case 1:
            if (gGameSession->eventState == 0) {
                taskKill(task);
                return;
            }
            done = gGameSession->evtSkipped;
            break;
        case 2:
        case 3:
            if (gGameSession->eventState != 0) {
                return;
            }
            work->field_4 -= 0x20;
            if (work->field_4 < 0x1000) {
                work->field_4 = 0x1000;
            }
            work->field_0 -= 0x10;
            if (work->field_0 < 0x100) {
                work->field_0 = 0x100;
            }
        case 4:
            work->field_8.w += 0x1000;
            done             = work->field_8.w > 0x100000;
            break;
        default:
            taskKill(task);
            return;
    }
    if (done) {
        taskKill(task);
    }
}

void func_actor_503500_80132990(Task* task)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        r, g, b;

    r = g = b = task->killCountdown;
    if (D_801153F4 == 0) {
        switch (task->state) {
            case 0:
                task->killCountdown = 0xFF;
                task->state++;
                break;
            case 1:
                if (--task->spawnArg1 < 0 || gGameSession->evtSkipped != 0) {
                    task->state++;
                }
                break;
            case 2:
                task->killCountdown -= 8;
                if (task->killCountdown < 0) {
                    taskKill(task);
                }
                break;
            default:
                taskKill(task);
                break;
        }
    }
    tile           = (TILE*)gGpuPrimCursor;
    gGpuPrimCursor = tile + 1;
    setTile(tile);
    SetSemiTrans(tile, 1);
    tile->x0 = -160;
    tile->y0 = -120;
    tile->w  = 320;
    tile->h  = 240;
    setRGB0(tile, r, g, b);
    addPrim(gGpuCurrentOt + 3, tile);
    dr             = gGpuPrimCursor;
    gGpuPrimCursor = dr + 1;
    setDrawTPage(dr, 1, 0, getTPage(0, 2, 320, 0));
    addPrim(gGpuCurrentOt + 3, dr);
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_503500_80132B78(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_503500_80132B98(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_503500_80132BB8(void)
{
    Gp_RestoreStreamRng();
}

/// Record handler (opcode 0x0D) of the actor's script data: cancels the queued
/// CD command and restarts the CD queue.
void func_actor_503500_80132BD8(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_actor_503500_80132BF8(void)
{
    Mc_SaveData.at4.loc.area = 0x16;
    Mc_SaveData.at4.loc.warp = 1;
    Mc_SaveData.at4.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_actor_503500_80132C40(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_503500_8014B964, 0, arg0, 0);
}

void func_actor_503500_80132C70(s32 arg0)
{
    D_actor_503500_80176558 = Task_SpawnFromTable(&D_actor_503500_8014B964, 1, arg0, 0);
}

/// Record handler (opcode 0x0D) of the actor's script data: calls
/// `Gp_PulseState1C`.
void func_actor_503500_80132CA4(void)
{
    Gp_PulseState1C();
}

void func_actor_503500_80132CC4(s8 arg0)
{
    Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 0x23);
    D_801153F1 = arg0;
}

/// Record handler (opcode 0x0D) of the actor's script data, taking the
/// record's argument word: ORs it into `GameSession::flowFlags` (the script
/// passes 1 and 2).
void func_actor_503500_80132D00(s32 bits)
{
    gGameSession->flowFlags |= bits;
}

void func_actor_503500_80132D20(Task* arg0)
{
    func_800E8634((s32)D_actor_503500_8014CD98, 0, (s32)D_actor_503500_8014D098);
    taskKill(arg0);
}

void func_actor_503500_80132D60(void)
{
    Gp_StateC08.field_6 |= 1;
}

void func_actor_503500_80132D7C(void)
{
    gGameSession->viewDirty = 1;
}

void func_actor_503500_80132D90(s32 arg0)
{
    GameFlag_SetNibble(0x100, arg0);
}

void func_actor_503500_80132DB4(s32 arg0)
{
    func_8017E27C(arg0 & 0xFF);
}

void func_actor_503500_80132DD4(void)
{
    D_actor_503500_8017655C.x = 0;
    D_actor_503500_8017655C.y = 0;
    D_actor_503500_8017655C.z = 0;
}

void func_actor_503500_80132DEC(void)
{
    Task*          slot3;
    GsCOORDINATE2* coord;
    SVECTOR*       rot;

    slot3 = gameGetPtrSlot(3);
    coord = ((TmdObject*)slot3->extra)->coords;

    D_actor_503500_8017655C.x = coord->coord.t[0];
    D_actor_503500_8017655C.y = coord->coord.t[1];
    D_actor_503500_8017655C.z = coord->coord.t[2];

    /* Anchoring the rotation pointer *after* the three word stores is what
     * makes cse keep the plain symbol as the base address; taking it first
     * anchors the whole function on `D_actor_503500_8017655C + 0x10`. */
    rot = &D_actor_503500_8017655C.rot;

    rot->vx = ((GameActor*)slot3->work)->field_50;
    rot->vy = ((GameActor*)slot3->work)->field_52;
    rot->vz = ((GameActor*)slot3->work)->field_54;
}

void func_actor_503500_80132E7C(void)
{
    Task* slot3;

    slot3 = gameGetPtrSlot(3);
    if ((D_actor_503500_8017655C.x != 0) || (D_actor_503500_8017655C.y != 0) ||
        (D_actor_503500_8017655C.z != 0)) {
        Gp_DispatchMsg(slot3, 0x3E9, (s32)&D_actor_503500_8017655C, 0);
    }
}

void func_actor_503500_80132EE8(u8 arg0)
{
    D_80115768 = arg0;
}

void func_actor_503500_80132EF4(void)
{
    func_80106350((Task*)gameGetPtrSlot(3), D_80073BA9, 0);
}

void func_actor_503500_80132F28(void)
{
    Gp_HaltPadScripts();
    gGameSession->padScriptFlags = 0;
}

void func_actor_503500_80132F58(void)
{
    D_actor_503500_80176558 = NULL;
}
