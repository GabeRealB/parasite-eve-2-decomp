#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/inline_c.h>
#include "gte.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/mem.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/wipsys.h"

#include "rooms/room.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_80181d28.h"

/// Sign pair for one corner of the promenade's ground-glow quad
/// (`func_acropolis_promenade_8017ED44`). The four entries of
/// `D_acropolis_promenade_80181AE4` are the +/-1 combinations, scaled by 0x300
/// into the quad's `vx` / `vz`; the quad is flat, so there is no `y` component
/// to sign.
typedef struct ApmGlowCorner {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
} ApmGlowCorner;
STATIC_ASSERT_SIZEOF(ApmGlowCorner, 0x4);

/// Work block of the prop task, at `Task::work`. The task allocates it with
/// `memCalloc(4, 0)` and clears its one word; nothing else in the room reads it.
typedef struct {
    s32 field_0;
} ApmPropWork;
STATIC_ASSERT_SIZEOF(ApmPropWork, 0x4);

extern s16 D_8007107A;
extern s8  D_8007272D;

extern s32          D_acropolis_promenade_80180F00;
extern s32          D_acropolis_promenade_80181068;
extern s32          D_acropolis_promenade_80181140;
extern s32          D_acropolis_promenade_80181144;
extern RoomEventMsg D_acropolis_promenade_801862D0;
extern Task*        D_acropolis_promenade_801862D8;

/// The room's message table, installed on the room task by its setup state.
extern GpMsgEntry D_acropolis_promenade_80180E74[];
/// Spawn table holding the prop task, terminated by an all-ones `flags`.
extern TaskDesc D_acropolis_promenade_80180EA4[];
/// The room's task table: the streamed-scene task is entry 2, and entries 3
/// and 4 are the fade tasks it spawns.
extern TaskDesc D_acropolis_promenade_80181148[];

/// Per-frame path the promenade's streamed scene walks the player's matrix
/// along, indexed backwards by `0x45 - CdCmd_Queue.field_1EA`, plus the script
/// pair the scene runs.
extern SVECTOR D_acropolis_promenade_80181184[];
extern s32     D_acropolis_promenade_80186224;
extern s32     D_acropolis_promenade_8018623C;

extern ApmGlowCorner D_acropolis_promenade_80181AE4[];

extern SVECTOR D_acropolis_promenade_80181AFC[];
extern SVECTOR D_acropolis_promenade_80181B0C[];
extern SVECTOR D_acropolis_promenade_80181B14[];
extern u16     D_acropolis_promenade_80181B74;
extern u16     D_acropolis_promenade_80181B76;
extern u16     D_acropolis_promenade_80181B78[];

void func_acropolis_promenade_8017D9E0(Task* arg0);
void func_acropolis_promenade_8017DAA4(Task* task);
void func_acropolis_promenade_8017DB48(Task* task);
void func_acropolis_promenade_8017F434(SVECTOR* arg0, s32 arg1, s32 arg2);

/// Per-frame state of the room task. The first frame the session's warp is 4
/// it spawns the streamed-scene task (entry 2 of the task table), once. While
/// the location's place is 1 it keeps `flowFlags` at 0xA and runs a latch on
/// `Gp_StateF0.field_0`: when that flag drops after having been 1, a sound
/// event is queued, and once the session's `field_126` is then non-zero,
/// `func_800E8634` is called with the room's two data blocks.
void func_acropolis_promenade_8017D5E4(Task* task)
{
    u8 temp;
    u8 f0;

    if (D_acropolis_promenade_80181140 == 0) {
        if (gGameSession->at4.loc.warp == 4) {
            D_acropolis_promenade_80181140 = 1;
            Task_SpawnFromTable(D_acropolis_promenade_80181148, 2, 0, 0);
        }
    }
    if (Mc_SaveData.sceneEvent == 6) {
        Mc_SaveData.sceneEvent = 5;
    }
    temp = gGameSession->at4.loc.place;
    if (temp == 1) {
        gGameSession->flowFlags = 0xA;
        f0                      = Gp_StateF0.field_0;
        if (f0 == temp) {
            D_acropolis_promenade_80181144 = f0;
        }
        if ((D_acropolis_promenade_80181144 == temp) && (f0 != D_acropolis_promenade_80181144)) {
            D_acropolis_promenade_80181144 = 2;
            SndEvt_EnqueueType2(0, 0x3C);
        }
        if ((D_acropolis_promenade_80181144 == 2) && (gGameSession->field_126 != 0)) {
            D_acropolis_promenade_80181144 = 0;
            func_800E8634((s32)&D_acropolis_promenade_80180F00, 0, (s32)&D_acropolis_promenade_80181068);
        }
    }
}

/// Message gate for the promenade's three hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 0xA answers with the `field_2` refusal code 1 while the disc has no
/// stream file open (`D_8007107A < 0 || D_8006AC30.sector == 0`) or nibble 1 is
/// not yet at 4; the first pass at 4 advances it to 5 instead of refusing.
/// Message 0xC, while nibble 2 is still 0, refuses with code 3, latches the
/// answered record into `D_acropolis_promenade_801862D0` for the room's own
/// script to pick up, and arms `D_8007272D` with 4. Message 0xE spawns the
/// capsule sequence the first time (nibble 2 still 0) and afterwards reports
/// through `field_3` whether nibble 2 has reached 3.
///
/// `field_5` non-zero means "report only", which suppresses every side effect.
s32 func_acropolis_promenade_8017D70C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    u16          msgId;

    *out = *in;
    if (in->msgId == 0xA && in->field_5 == 0) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            out->field_2 = 1;
        }
        if (GameFlag_GetNibble(1) == 4) {
            GameFlag_SetNibble(1, 5);
        } else {
            out->field_2 = 1;
        }
    }
    if (in->msgId == 0xC && GameFlag_GetNibble(2) == 0) {
        if (in->field_5 == 0) {
            out->field_2                   = 3;
            D_acropolis_promenade_801862D0 = *out;
            D_8007272D                     = 4;
        }
        return 1;
    }
    msgId = in->msgId;
    if (msgId == 0xE) {
        if (GameFlag_GetNibble(2) == 0) {
            if (in->field_5 == 0) {
                Gp_SpawnIfCapIdle(2, 1);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
        if (in->msgId == msgId) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(2) == 3) {
                    out->field_3 = 2;
                } else {
                    out->field_3 = 1;
                }
            }
        }
    }
    return 1;
}

/// Handler for message 0x13F1 in the room's message table: does nothing and
/// returns 0.
s32 func_acropolis_promenade_8017D8D8(void)
{
    return 0;
}

s32 func_acropolis_promenade_8017D8E0(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 5) {
        if (Gp_GetCurBit2Flag(0x15) != 2) {
            Gp_StartCapSlot(5, 1, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_acropolis_promenade_8017D930(void)
{
}

s32 func_acropolis_promenade_8017D938(s32 arg0, s32 arg1, s32 arg2)
{
    switch (arg2) {
        case 0xA:
            SndEvt_EnqueueType6(0x510B0009, 0, 0);
            break;
        case 0x67:
            SndEvt_EnqueueType6(0x510B000A, 0, 0);
            break;
    }
    return 0;
}

/// State table of the room task, run by `func_acropolis_promenade_8017DA4C`.
const TaskFuncTable3 D_acropolis_promenade_8017D5C4 = {
    { func_acropolis_promenade_8017D9E0, func_acropolis_promenade_8017D5E4, taskKill },
};

/// State table of the prop task, run by `func_acropolis_promenade_8017D988`.
const TaskFuncTable3 D_acropolis_promenade_8017D5D0 = {
    { func_acropolis_promenade_8017DAA4, func_acropolis_promenade_8017DB48, taskKill },
};

/// Runs the prop task's current state (`func_acropolis_promenade_8017DAA4`,
/// `func_acropolis_promenade_8017DB48`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017D988(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5D0;
    sp.funcs[task->state](task);
}

void func_acropolis_promenade_8017D9E0(Task* arg0)
{
    arg0->msgTable = D_acropolis_promenade_80180E74;
    Game_SetPtrSlot(arg0, 7);
    D_acropolis_promenade_801862D8 = Task_SpawnFromTable(D_acropolis_promenade_80180EA4, 0, 0, 0);
    arg0->state                    = (s32)(arg0->state + 1);
    D_80115598                     = 1;
}

/// Runs the room task's current state (`func_acropolis_promenade_8017D9E0`,
/// `func_acropolis_promenade_8017D5E4`, then `taskKill`) through a copy of its
/// handler table on the stack.
void func_acropolis_promenade_8017DA4C(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_acropolis_promenade_8017D5C4;
    sp.funcs[task->state](task);
}

/// First state of the prop task whose model sits under the room's view
/// coordinate system (`D_acropolis_promenade_8017D5D0`): allocates its work
/// block, shows the model, places it at (-0x23F0, 0x12C, -0xAF0) under
/// `gGfxViewCoord` and moves to the next state.
void func_acropolis_promenade_8017DAA4(Task* task)
{
    TmdObject*     extra;
    GsCOORDINATE2* coord;
    ApmPropWork*   work;

    extra = (TmdObject*)task->extra;
    coord = extra->coords;
    work  = (ApmPropWork*)memCalloc(sizeof(ApmPropWork), 0);
    if (work == NULL) {
        taskKill(task);
        return;
    }
    task->work        = (TaskIdMap*)work;
    work->field_0     = 0;
    extra->flags      = 0;
    coord->sub        = &gGfxViewCoord;
    coord->coord.t[0] = -0x23F0;
    coord->coord.t[1] = 0x12C;
    coord->coord.t[2] = -0xAF0;
    coord->flg        = 0;
    task->state++;
}

void func_acropolis_promenade_8017DB48(Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    if (Gp_GetViewIndex() == 5) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    coord->flg = 0;
}

/// The promenade's streamed-scene task. State 0 allocates the `RoomStreamWork`
/// block, cues the stream (slot-6 msg 0xFA4), captures slot 3 and the player's
/// coordinate matrix in the block, and republishes the player's weapon to slot
/// 3 with a 0x3E8 record. State 1 waits for the stream to come up
/// (`CdCmd_Queue::field_1FA`), then starts the script pair and reparents this
/// task under it. State 2 drives the ride: every frame it moves the player's
/// matrix to the path entry the stream's countdown selects, offers the pad
/// prompt once (`Pad_CheckFlag800`, entry 3 of the room's task table) and, when
/// the prompt task reports back, warps slot 3 with a 0x3E9 placement and spawns
/// entry 4 instead; once the countdown is within 6 frames of the end it sends
/// the same placement as a 0x3F2 and moves on either way. State 3 waits for
/// slot 3 to go idle (msg 0x3F0), releases it (0x3F1), stops the stream
/// (0xFA5), records the room in the save and kills the task.
void func_acropolis_promenade_8017DB9C(Task* task)
{
    GpAnimArg       rec;
    GpXformArg      place;
    s32             killed;
    RoomStreamWork* work;
    RoomStreamWork* blk;
    RoomStreamWork* dest;
    CdCmdQueue*     queue;
    s32             weaponId;

    queue = &CdCmd_Queue;
    work  = (RoomStreamWork*)task->work;
    switch (task->state) {
        case 0:
            blk        = memCalloc(0x14, 0);
            task->work = (TaskIdMap*)blk;
            if (blk == NULL) {
                taskKill(task);
                break;
            }
            Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA4, 0, 0);
            ((RoomStreamWork*)task->work)->target = gameGetPtrSlot(3);
            ((RoomStreamWork*)task->work)->mtx    = Player_Status.coordMtx;
            weaponId                              = Player_Status.weapon;
            rec.animBlock.index                   = (Mc_SaveData.characterId == 1) ? weaponId + 1 : weaponId + 0x22;
            rec.field_4                           = 1;
            rec.field_8                           = 0;
            rec.field_C                           = 0;
            rec.field_10                          = 0;
            Gp_DispatchMsg(((RoomStreamWork*)task->work)->target, 0x3E8, (s32)&rec, 0);
            func_800E9BDC(3, 0x9FF);
            Gp_StateF0.field_4 = 1;
            task->state        = task->state + 1;
            break;

        case 1:
            if (queue->field_1FA != 0) {
                work->script                  = Gp_SpawnScript18((s32)&D_acropolis_promenade_80186224,
                                                                 (s32)&D_acropolis_promenade_8018623C);
                gGameSession->padScriptFlags |= 0x80;
                Task_Reparent(task, work->script);
                task->state = task->state + 1;
            }
            break;

        case 2:
            work->mtx->t[0] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vx;
            work->mtx->t[1] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vy;
            work->mtx->t[2] = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
            if (work->spawned != 0) {
                if (Task_PollKill(work->child, &killed) != 0) {
                    place.pos.vx = 0x282;
                    place.pos.vy = 0x29;
                    place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
                    place.rot.vz = 0;
                    place.rot.vx = 0;
                    place.rot.vy = 0xC00;
                    dest         = (RoomStreamWork*)task->work;
                    Gp_DispatchMsg(dest->target, 0x3E9, (s32)&place, 0);
                    Task_SpawnFromTable(D_acropolis_promenade_80181148, 4, 0, 0);
                    task->state = task->state + 1;
                    break;
                }
            } else if (Pad_CheckFlag800() != 0) {
                work->child   = Task_SpawnFromTable(D_acropolis_promenade_80181148, 3, 0, 0);
                work->spawned = 1;
            }
            if ((0x45 - queue->field_1EA) < 6) {
                place.pos.vx = 0x282;
                place.pos.vy = 0x29;
                place.pos.vz = D_acropolis_promenade_80181184[0x45 - queue->field_1EA].vz - 0xC8;
                dest         = (RoomStreamWork*)task->work;
                Gp_DispatchMsg(dest->target, 0x3F2, (s32)&place, 0);
                task->state = task->state + 1;
            }
            break;

        case 3:
            if (Gp_DispatchMsg(work->target, 0x3F0, 0, 0) == 0) {
                Gp_DispatchMsg(work->target, 0x3F1, 0, 0);
                Gp_DispatchMsg(gameGetPtrSlot(6), 0xFA5, 0, 0);
                Mc_SaveData.at4.loc.view = 2;
                func_800E9BDC(2, 0x9FF);
                Gp_StateF0.field_4            = 0;
                gGameSession->padScriptFlags &= 0x7F;
                taskKill(task);
            }
            break;
    }
}

/// Entry 3 of the room's task table: draws `Fade_DrawOverlay` at the grey
/// level `killCountdown`, which rises by 0x20 a frame; at 0x100 the task asks
/// to be killed with `Task_RequestKill`, which the streamed-scene task that
/// spawned it polls for.
void func_acropolis_promenade_8017DF74(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = (u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        Task_RequestKill(arg0, 0);
    }
}

/// Entry 4 of the room's task table: the reverse ramp of entry 3, drawing the
/// overlay at `~killCountdown`, and killing itself outright at the end.
void func_acropolis_promenade_8017DFD4(Task* arg0)
{
    u8  fade;
    s16 temp_v0;

    fade = ~(u8)arg0->killCountdown;
    Fade_DrawOverlay(fade, fade, fade, 2);
    temp_v0             = (u16)arg0->killCountdown + 0x20;
    arg0->killCountdown = temp_v0;
    if (temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

/// Per-frame effect spawner for the promenade. `D_acropolis_promenade_80181B74`
/// / `_80181B76` and the twelve-entry mask table `_80181B78` are per-view bit
/// masks: bit `view - 1` of an entry says whether that emitter is visible from
/// the camera `Gp_GetViewIndex` reports, and the parallel twelve-entry
/// `_80181B14` array holds each emitter's offset from the room's coordinate
/// frame. View 7 spawns nothing.
void func_acropolis_promenade_8017E03C(Task* task)
{
    GsCOORDINATE2* coord;
    GpEffWork*     work;
    u8             view;
    s32            i;
    s32            mask;
    s16            prev;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    view  = Gp_GetViewIndex();
    if (Gp_State1C->eventState >= 4) {
        return;
    }
    work->age++;
    if (view == 7) {
        return;
    }
    mask = 1 << (view - 1);
    if (D_acropolis_promenade_80181B74 & mask) {
        Gp_SpawnEff(0x8006004B, coord, work->age, &D_acropolis_promenade_80181AFC[0]);
        Gp_SpawnEff(0x8006004B, coord, work->age, &D_acropolis_promenade_80181AFC[1]);
        Gp_SpawnEff(0x60057, coord, work->age, &D_acropolis_promenade_80181B0C[0]);
        func_acropolis_promenade_8017F434(&D_acropolis_promenade_80181AFC[-1], 0x100, 0x5C40);
    }
    for (i = 0; i < 3; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 0, &D_acropolis_promenade_80181B14[i]);
        }
    }
    for (i = 3; i < 5; i++) {
        if (D_acropolis_promenade_80181B78[i] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i]);
        }
        if (D_acropolis_promenade_80181B78[i + 2] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 2]);
        }
        if (D_acropolis_promenade_80181B78[i + 4] & mask) {
            Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[i + 4]);
        }
        if (D_acropolis_promenade_80181B78[i + 6] & mask) {
            Gp_SpawnEff(0x60062, coord, 2, &D_acropolis_promenade_80181B14[i + 6]);
        }
    }
    if (D_acropolis_promenade_80181B78[11] & mask) {
        Gp_SpawnEff(0x60062, coord, 1, &D_acropolis_promenade_80181B14[11]);
    }
    if (D_acropolis_promenade_80181B76 & mask) {
        prev = work->scale;
        if (prev != view) {
            for (i = 0; i < 0x28; i++) {
                Gp_SpawnEff(0x60056, coord, view, NULL);
            }
        } else {
            Gp_SpawnEff(0x60056, coord, prev, NULL);
            Gp_SpawnEff(0x60056, coord, prev, NULL);
        }
    }
    work->scale = view;
}

/// One falling water drip on the promenade, drawn as a `DR_MOVE` that smears a
/// one-pixel-tall strip of the frame buffer down by a pixel. The first frame
/// rolls the whole drip out of `Gp_LcgState`: `move.vx` is the column
/// (0..0xEF), `move.vy` the row it starts on (0xB0..0xEF), `scale` the
/// lifetime in frames, `angle` the width and `period` the number of frames
/// each row of fall takes. `gDisplayState.drawBuffer` picks the buffer half, and
/// the OT slot is the row scaled into the 0x500-deep range so a drip sorts
/// against the room behind it. The task releases itself once the camera turns
/// away, the lifetime runs out, or the drip falls off the bottom of the screen.
void func_acropolis_promenade_8017E394(Task* task)
{
    GpEffWork* work;
    RECT       rect;
    DR_MOVE*   mv;
    u16        rnd;
    s32        bufferY;
    s32        x;
    s32        y;
    s32        onScreen;
    s32        depth;

    work    = task->spawnArg2;
    bufferY = gDisplayState.drawBuffer * 0x110;
    if ((u8)Gp_GetViewIndex() == task->spawnArg1) {
        if (work->age == 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = ((u32)Gp_LcgState >> 16) % 240;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = (((u32)Gp_LcgState >> 16) & 0x3F) + 0xB0;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            rnd           = (u32)Gp_LcgState >> 16;
            work->scale   = (u32)rnd % 90 + 0x1E;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->angle   = (((u32)Gp_LcgState >> 16) & 0x3F) + 0x10;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->period  = (((u32)Gp_LcgState >> 16) & 3) + 1;
            task->state++;
        }
        y        = work->move.vy + work->age / work->period;
        x        = work->move.vx;
        depth    = 0x500 - (y - 0xB0) * 10;
        onScreen = y < 0xEF;
        if (onScreen) {
            rect.x         = x;
            rect.y         = y + bufferY;
            rect.w         = work->angle;
            rect.h         = 1;
            mv             = (DR_MOVE*)gGpuPrimCursor;
            gGpuPrimCursor = mv + 1;
            SetDrawMove(mv, &rect, x, y + bufferY + 1);
            addPrim(gGpuCurrentOt + (depth >> 4), mv);
        }
        work->age++;
        if (work->age <= work->scale && onScreen) {
            return;
        }
    }
    Gp_ReleaseState1CMem(work, task);
}

/// One frame of the promenade's twinkling star: two semi-transparent
/// `POLY_FT4`s stacked on the same screen point, centred on the task's own
/// coordinate frame. The frame's translation is projected through `GsWSMATRIX`
/// into a 0x18-byte `G_SCRATCH_HEAD` block, and both quads are dropped
/// entirely inside `otz` 0x11.
///
/// The lower quad is upright, of half-extent `0x1680 / otz`, and animates
/// through six 0x10x0x10 cells at v = 0 on tpage 0x2B by stepping `u` with
/// `work->age % 6`; it is drawn `code |= 3`, so semi-transparent *and*
/// unshaded. The upper quad is the 0x27x0x27 flare at v = 0x10 with clut
/// 0x4381, drawn at `0x3A80 / otz` from the centre along the spin angle
/// `work->scale` and its quarter-turn (`+ 0x400`), so it rotates a frame at
/// a time. Its colour is a fresh random grey (0x20..0x7F, equal on all three
/// channels) every frame, which is what makes the star flicker.
///
/// Like the promenade's other glows, the task is one-shot: the work block is
/// released as soon as both quads have been queued, so the room respawns it
/// every frame it wants the star.
void func_acropolis_promenade_8017E634(Task* task)
{
    GsCOORDINATE2*        coord;
    GpEffWork*            work;
    void**                scratch;
    u8*                   head;
    OverlaySpriteScratch* blk;
    s32*                  otzp;
    POLY_FT4*             prim;
    s32                   grey;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    work->age   = task->spawnArg1;
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (OverlaySpriteScratch*)(head - 0x18);
    otzp        = &blk->otz;
    blk->vec.vx = coord->workm.t[0];
    blk->vec.vy = coord->workm.t[1];
    *scratch    = blk;
    blk->vec.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->vec);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(otzp);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4380;
        prim->code |= 3;
        prim->u0    = (work->age % 6) * 16;
        prim->v0    = 0;
        prim->u1    = (work->age % 6) * 16 + 0xF;
        prim->v1    = 0;
        prim->u2    = (work->age % 6) * 16;
        prim->v2    = 0xF;
        prim->u3    = (work->age % 6) * 16 + 0xF;
        prim->v3    = 0xF;
        blk->dx     = 0x1680 / blk->otz;
        blk->dy     = 0x1680 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->dx;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->dx;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->dy;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);

        prim           = (POLY_FT4*)gGpuPrimCursor;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        prim->clut  = 0x4381;
        prim->tpage = 0x2B;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) % 96 + 0x20;
        prim->u0    = 0;
        prim->v0    = 0x10;
        prim->u1    = 0x27;
        prim->v1    = 0x10;
        prim->u2    = 0;
        prim->v2    = 0x37;
        prim->u3    = 0x27;
        prim->v3    = 0x37;
        prim->code |= 2;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;

        work->scale = gDisplayState.animFrame + work->age;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale)) >> 12;
        prim->x0    = blk->sxy.vx + blk->dx;
        prim->x3    = blk->sxy.vx - blk->dx;
        prim->y0    = blk->sxy.vy - blk->dy;
        prim->y3    = blk->sxy.vy + blk->dy;
        blk->dx     = ((0x3A80 / blk->otz) * rsin(work->scale + 0x400)) >> 12;
        blk->dy     = ((0x3A80 / blk->otz) * rcos(work->scale + 0x400)) >> 12;
        prim->x1    = blk->sxy.vx + blk->dx;
        prim->x2    = blk->sxy.vx - blk->dx;
        prim->y1    = blk->sxy.vy - blk->dy;
        prim->y2    = blk->sxy.vy + blk->dy;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x18);
    Gp_ReleaseState1CMem(work, task);
}

/// Draws one frame of the promenade's ground glow: a semi-transparent textured
/// quad lying flat under the task's coordinate frame. The four corner signs in
/// `D_acropolis_promenade_80181AE4` are scaled to +/-0x300 in `vx` / `vz` (with
/// `vy` left at zero, so the quad is horizontal), rotated by the task's own
/// `workm`, offset by that matrix's translation and then projected through
/// `GsWSMATRIX` into an `RoomQuadScratch` block taken from `G_SCRATCH_HEAD`. The
/// first corner goes through `rtps` and the other three through `rtpt`, the
/// same split the sanctuary's mosaic tiles use.
///
/// The depth is biased by 0x20 before the near-plane test, so the glow survives
/// a little closer to the camera than the 0x11 cutoff alone would allow. Its
/// colour is a fresh random grey (0..0xF, equal on all three channels) every
/// frame, which is what makes it flicker; the quad is drawn semi-transparent
/// (`code |= 2`) from the 0x27x0x27 patch at v = 0x10 on tpage 0x2B.
///
/// The task is one-shot: the work block is released as soon as the quad has
/// been queued, so the room respawns it each frame it wants the glow.
void func_acropolis_promenade_8017ED44(Task* task)
{
    GsCOORDINATE2*   coord;
    GpEffWork*       work;
    void**           scratch;
    u8*              head;
    RoomQuadScratch* blk;
    POLY_FT4*        prim;
    SVECTOR*         sv;
    s32              i;
    s32              grey;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch   = (void**)G_SCRATCH_HEAD;
    head      = *scratch;
    work->age = task->spawnArg1;
    *scratch  = head - 0x24;
    blk       = (RoomQuadScratch*)(head - 0x24);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_acropolis_promenade_80181AE4[i].x * 0x300;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(RoomQuadScratch, v));
        sv->vy = 0;
        sv->vz = D_acropolis_promenade_80181AE4[i].y * 0x300;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt();
    prim->u0 = 0;
    prim->v0 = 0x10;
    prim->u1 = 0x27;
    prim->v1 = 0x10;
    prim->u2 = 0;
    prim->v2 = 0x37;
    prim->u3 = 0x27;
    prim->v3 = 0x37;
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    blk->otz += 0x20;
    if (blk->otz >= 0x11) {
        prim->tpage = 0x2B;
        prim->clut  = 0x4381;
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        grey        = ((u32)Gp_LcgState >> 16) & 0xF;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        prim->code |= 2;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x24);
    Gp_ReleaseState1CMem(work, task);
}

/// Glow sprite task: queues one camera-facing, semi-transparent `POLY_FT4`
/// centred on the task's coordinate frame. The frame's translation is
/// projected through `GsWSMATRIX` into a 0x14-byte `G_SCRATCH_HEAD` block, and
/// the quad is a square of half-extent `0x6180 / otz` around the projected
/// point, so it shrinks with distance; nothing is drawn at `otz` 0x10 or less.
///
/// `Task::spawnArg1` (0..2) selects the 0x27x0x27 texture cell at
/// `u = (arg + 1) * 0x28`, `v = 0x10` on tpage 0x2B, the clut
/// `0x4380 | ((arg + 2) & 0x3F)`, and the grey level: a base of
/// 0x20 / 0x60 / 0x20, plus 0x08 / 0x10 / 0x0C on odd
/// `gDisplayState.animFrame`s.
///
/// The work block in `spawnArg2` is released after the quad is queued, so
/// each spawn draws a single frame.
void func_acropolis_promenade_8017F0BC(Task* task)
{
    GsCOORDINATE2*         coord;
    GpEffWork*             work;
    void**                 scratch;
    u8*                    head;
    RoomGlowSpriteScratch* blk;
    // The `gte_stszotz` operand is a second register holding the same pointer;
    // reload only emits that copy for a hard-register local (see
    // DECOMPILATION_LEARNINGS.md, "A `move` between two registers holding the
    // same pointer is a pin").
    register RoomGlowSpriteScratch* p asm("a0");
    POLY_FT4*                       prim;
    s32                             grey;
    s32                             clut;

    coord = ((TmdObject*)task->extra)->coords;
    work  = task->spawnArg2;
    Gp_UpdateCoord(coord);
    scratch     = (void**)G_SCRATCH_HEAD;
    head        = *scratch;
    blk         = (RoomGlowSpriteScratch*)(head - 0x14);
    blk->pos.vx = coord->workm.t[0];
    blk->pos.vy = coord->workm.t[1];
    *scratch    = blk;
    p           = blk;
    blk->pos.vz = coord->workm.t[2];
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->pos);
    gte_rtps();
    prim           = (POLY_FT4*)gGpuPrimCursor;
    gGpuPrimCursor = prim + 1;
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&blk->sxy);
    gte_stszotz(&p->otz);
    if (blk->otz >= 0x11) {
        u8 base[3] = { 0x20, 0x60, 0x20 };
        u8 step[3] = { 0x08, 0x10, 0x0C };

        grey        = base[task->spawnArg1] + (gDisplayState.animFrame & 1) * step[task->spawnArg1];
        prim->code |= 2;
        prim->tpage = 0x2B;
        prim->r0    = grey;
        prim->g0    = grey;
        prim->b0    = grey;
        // Assigning through an `s32` keeps the load of `spawnArg1` in SImode;
        // storing the expression straight into the `u16` field lets the front
        // end shorten the whole chain and the load becomes an `lhu`.
        clut       = ((task->spawnArg1 + 2) & 0x3F) | 0x4380;
        prim->clut = clut;
        prim->u0   = (task->spawnArg1 + 1) * 0x28;
        prim->v0   = 0x10;
        prim->u1   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v1   = 0x10;
        prim->u2   = (task->spawnArg1 + 1) * 0x28;
        prim->v2   = 0x37;
        prim->u3   = (task->spawnArg1 + 1) * 0x28 + 0x27;
        prim->v3   = 0x37;
        blk->half  = 0x6180 / blk->otz;
        prim->x0 = prim->x2 = blk->sxy.vx - blk->half;
        prim->x1 = prim->x3 = blk->sxy.vx + blk->half;
        prim->y0 = prim->y1 = blk->sxy.vy - blk->half;
        prim->y2 = prim->y3 = blk->sxy.vy + blk->half;
        addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES(0x14);
    Gp_ReleaseState1CMem(work, task);
}

/// Draws a radial glow at the world-space point `arg0`: the point is
/// projected through `Gfx_ViewWorldMtx` and, when the GTE flag is
/// non-negative, sixteen gouraud wedges (eight at the outer radius in the
/// half-brightness colour, eight at half that radius in the full colour) plus
/// four cross wedges reaching out from the inner radius are queued around the
/// projected centre, each fading to black at its rim. `arg1` is a signed
/// half-extent: the radii are `(s16)arg1 * 64 / otz` (outer) and
/// `(s16)arg1 * 8 / otz` (inner). `arg2` packs the colour one nibble per
/// channel - bits 8..11 red, 4..7 green, 0..3 blue, each scaled by 16 - with
/// bits 12..15 the shift of a `gDisplayState.animFrame & 1` flicker added to
/// every channel.
void func_acropolis_promenade_8017F434(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    RoomDraw05Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    s32                t;
    s32                t2;
    s32                ua;
    s32                ub;
    s32                uc;
    s32                frame;
    s32                packed;
    s32                blend;
    s32                r;
    s32                g;
    s32                b;
    s32                outer;
    s32                inner;
    s32                hr;
    s32                hg;
    s32                hb;

    {
        void** scratch;
        u8*    tmp;

        scratch = (void**)G_SCRATCH_HEAD;
        tmp     = SCRATCH_PUSH_BYTES_AT(scratch, 0x14);
        block   = (RoomDraw05Scratch*)tmp;
    }

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&block->sx);
    gte_stflg(&block->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        arg1        <<= 16;
        arg1        >>= 16;
        outer         = (arg1 * 64) / block->otz;
        frame         = gDisplayState.animFrame;
        block->rOuter = outer;
        inner         = (arg1 * 8) / block->otz;
        ang           = 0;
        packed        = arg2 << 16;
        blend         = (frame & 1) << (packed >> 28);
        r             = blend + ((packed >> 20) & 0xF0);
        g             = blend + ((packed >> 16) & 0xF0);
        b             = blend + ((arg2 & 0xF) << 4);
        block->rInner = inner;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            hr = (u8)r >> 1;
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            hg = (u8)g >> 1;
            hb = (u8)b >> 1;
            setRGB2(prim, hr, hg, hb);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        r   = (u8)hr;
        g   = (u8)hg;
        b   = (u8)hb;
        do {
            ua             = ang - 0x400;
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ua)) >> 13);
            prim->y0 = block->sy + ((block->rInner * rcos(ua)) >> 13);
            prim->x1 = block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = block->sy + ((block->rOuter * rcos(ang)) >> 12);
            ub       = ang + 0x400;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(ub)) >> 13);
            prim->y3 = block->sy + ((block->rInner * rcos(ub)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, r, g, b);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = block->sx + ((block->rOuter * rsin(ub)) >> 11);
            prim->y1 = block->sy + ((block->rOuter * rcos(ub)) >> 11);
            uc       = ang + 0x800;
            prim->x2 = block->sx;
            prim->y2 = block->sy;
            prim->x3 = block->sx + ((block->rInner * rsin(uc)) >> 12);
            prim->y3 = block->sy + ((block->rInner * rcos(uc)) >> 12);
            ang      = uc;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x14);
}
