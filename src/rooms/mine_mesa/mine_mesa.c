#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/areaplace.h"
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
#include "main/stream.h"
#include "main/task.h"
#include "main/tmd.h"
#include "main/unknown_syms.h"
#include "rooms/mine_mesa.h"
#include "rooms/room.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include "gte.h"
#include <psyq/libgpu.h>
#include <psyq/libgs.h>
#include <psyq/libgte.h>
#include <psyq/stdio.h>

/// A place an enemy can be spawned at: its position and the yaw it faces.
typedef struct {
    s16 x;
    s16 y;
    s16 z;
    s16 yaw;
} _MineMesaSpawnPoint;

/// One of the room's four wall segments: the two corners along its
/// base. The trailing bytes are not read by the code that builds the walls.
typedef struct {
    SVECTOR start;
    SVECTOR end;
    u8      unk10[8];
} _MineMesaWall;

extern s16 D_80071076;
extern s16 D_80072830;
extern s8  D_8007106B;
extern s8  D_8007217B;
extern s8  D_8007218B;
extern s8  D_80114C12;
extern u8  D_80115690;
extern u8  D_801156F9;
extern s32 D_8011572C;
extern s32 D_80115750;
extern s32 D_80115758;

extern TaskDesc D_8014D8A4;

extern s32  func_80179A04(GpSaveLoc* in, GpSaveLoc* out);
extern void func_800E8614(s32 arg0, s32 arg1);
extern void func_800E8634(s32 arg0, s32 arg1, s32 arg2);

extern TaskDesc   D_mine_mesa_801818F8;
extern GpMsgEntry D_mine_mesa_80181904[];
extern TaskDesc   D_mine_mesa_80181990;

/// The mesa's run: one `SVECTOR` position per frame, sent as a `GpXformArg`.
extern SVECTOR D_mine_mesa_80184184[];

extern s32 D_mine_mesa_80184664;
extern s32 D_mine_mesa_80184BA4;
extern s32 D_mine_mesa_80184D9C;
extern s32 D_mine_mesa_80184FF4;
extern s32 D_mine_mesa_801850E4;
extern s32 D_mine_mesa_801854BC;
extern s32 D_mine_mesa_801856B4;
extern s32 D_mine_mesa_8018578C;
extern s32 D_mine_mesa_801861DC;

/// The mesa's emitter placements, one `SVECTOR` per position, 8 bytes apart.
/// The runs overlap: `864F0`'s fourth and seventh positions are `864F0` itself
/// plus 0x18 and 0x30, which lie inside the `86508` run, and view 8's single
/// position is `864F0[0]` while view 4 draws the same base's 0x30.
extern SVECTOR D_mine_mesa_801864C8[];
extern SVECTOR D_mine_mesa_801864D0[];
extern SVECTOR D_mine_mesa_801864D8[];
extern SVECTOR D_mine_mesa_801864F0[];
extern SVECTOR D_mine_mesa_80186508[];

/// The two offsets `func_mine_mesa_8017FC94` places its trail origins at, from
/// the task's parent coordinate. `D_mine_mesa_80186530` is the second entry
/// under its own name: the per-frame path addresses it directly.
extern SVECTOR D_mine_mesa_80186528[];
extern SVECTOR D_mine_mesa_80186530;

extern GpObj4A             D_mine_mesa_801890EC[4];
extern _MineMesaWall       D_mine_mesa_80189A9C[4];
extern _MineMesaSpawnPoint D_mine_mesa_80189AFC[];
extern s32                 D_mine_mesa_80189B1C;
extern TaskDesc            D_mine_mesa_80189B2C;
extern GpFadeWork          D_mine_mesa_80189B38;
extern Task*               D_mine_mesa_80189B4C;
extern s32                 D_mine_mesa_80189B50;
extern Task*               D_mine_mesa_80189B58;
extern u32                 D_mine_mesa_80189B64;
extern u8                  D_mine_mesa_80189B6A;
extern s16                 D_mine_mesa_80189B6C;
extern s16                 D_mine_mesa_80189B6E;
extern GpEnemy*            D_mine_mesa_80189B74[2];

#define MINE_MESA_RAND() ((Gp_LcgState = Gp_LcgState * 5 + 0x71357911) >> 16)

void func_mine_mesa_8017DD44(void);
void func_mine_mesa_8017EB38(void);
void func_mine_mesa_80180184(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3);
void func_mine_mesa_801817BC(void);

/// Runs this room's pending event once the request for it has been accepted.
/// State 0 plays the caption command recorded in `D_mine_mesa_80189B60` and
/// saves a point; state 1 spawns the helper task 0x31 the request asked for;
/// state 2 queues the stage sound and state 3 waits for that voice to end,
/// either of which falls through to state 4 - the commit, which plays the
/// event's sound, copies the saved location into `Mc_SaveData` and loads it.
void func_mine_mesa_8017D670(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            Gp_StateF0.field_4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_mine_mesa_80189B60.capCmd, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_mine_mesa_80189B6A != 0) {
                    D_mine_mesa_80189B38.field_0 = 0;
                    D_mine_mesa_80189B38.field_1 = 0;
                    D_mine_mesa_80189B38.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_mine_mesa_80189B38);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_mine_mesa_80189B64 != 0) {
                Gp_EnqueueStageSnd6(D_mine_mesa_80189B64, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_mine_mesa_80189B64)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_mine_mesa_80189B40.field_0;
            Mc_SaveData.at4.loc.warp = D_mine_mesa_80189B40.field_2;
            Mc_SaveData.at4.loc.room = D_mine_mesa_80189B40.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

void func_mine_mesa_8017D808(Task* task)
{
    u8  field9;
    s32 nibble;

    if ((gGameSession->eventState == 0) && (D_80114C12 != 1) && (field9 = gGameSession->at4.loc.place, field9 == 1)) {
        if (GameFlag_GetNibble(0x90) == 0) {
            if (gameGetPtrSlot(0xA) != NULL) {
                func_800E8634((s32)&D_mine_mesa_8018578C, 0, (s32)&D_mine_mesa_801861DC);
            }
            func_800E3FAC(0xA2, 0x1B);
            GameFlag_SetNibble(0x90, 1);
            return;
        }
        nibble = GameFlag_GetNibble(0xCD);
        if ((nibble == field9) && (D_mine_mesa_80189B50 == nibble)) {
            func_800E8634((s32)&D_mine_mesa_80184664, 0, (s32)&D_mine_mesa_80184BA4);
            D_mine_mesa_80189B50 = 2;
        }
    }
}

s32 func_mine_mesa_8017D8F0(void)
{
    return 0;
}

static __inline__ s32 MineMesa_StartEvent(GpSaveLoc* dst, RoomLatchedEvent* event)
{
    D_mine_mesa_80189B48 = 0;
    if (GameFlag_GetNibble(event->flagId) == 0 || event->flagId == 0) {
        if (dst->field_5 == 0) {
            D_mine_mesa_80189B40 = *dst;
            D_mine_mesa_80189B60 = *event;
            if (event->flagId != 0) {
                GameFlag_SetNibble(event->flagId, 1);
            }
            Task_SpawnFromTable(&D_mine_mesa_801818F8, 0, 0, 0);
            D_mine_mesa_80189B48 = 1;
        }
        return 2;
    }
    return 1;
}

/// Handler id 0x13EE of this room's copy of the `GpMsgEntry` table
/// `D_mine_mesa_80181904`: copies the requested location to `dst` and forwards
/// both to `func_80179A04`. A stage-3 request latches the outgoing location and
/// the event parameters below into the room's pending event and starts the
/// controller task; `field_5` set only suppresses that side effect. Answers 0
/// without side effects while the request is already in flight (`field_9` is 1
/// and `Gp_StateF0.field_0` agrees with it), 2 for a stage-3 request and 1 for
/// every other one.
s32 func_mine_mesa_8017D8F8(s32 arg0, s32 arg1, GpSaveLoc* in, GpSaveLoc* out)
{
    RoomLatchedEvent event;
    u8               field9;

    *out = *in;
    func_80179A04(in, out);
    if (*(u16*)in != 3) {
        return 1;
    }
    field9 = gGameSession->at4.loc.place;
    if (field9 == 1 && Gp_StateF0.field_0 == field9) {
        return 0;
    }
    event.capCmd   = 0xE;
    event.stageSnd = 0x54010001;
    event.flagId   = 0x171;
    event.fade     = 0;
    return MineMesa_StartEvent(out, &event);
}

s32 func_mine_mesa_8017DA7C(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0xD) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0x11A) >= 2 ? 0xD : 0xC);
    }
    return 0;
}

s32 func_mine_mesa_8017DABC(Task* task, s32 msgId, GpMsg13EF* msg, s32 arg3)
{
    switch (msg->field_2) {
        case 1:
            if (GameFlag_GetNibble(0x71) == 0) {
                if (gameGetPtrSlot(0xA) != NULL) {
                    func_800E8614((s32)&D_mine_mesa_801850E4, 0);
                }
                func_800E3FAC(0xA2, 0x1C);
                GameFlag_SetNibble(0x71, 1);
                func_mine_mesa_8017DD44();
            }
            break;
        case 2:
            if (GameFlag_GetNibble(0x71) <= 0) {
                if (GameFlag_GetNibble(0x91) == 0) {
                    if (gameGetPtrSlot(0xA) != NULL) {
                        func_800E8634((s32)&D_mine_mesa_80184D9C, 1, (s32)&D_mine_mesa_80184FF4);
                    }
                    GameFlag_SetNibble(0x91, 1);
                } else if (gameGetPtrSlot(0xA) != NULL) {
                    func_800E8634((s32)&D_mine_mesa_801854BC, 1, (s32)&D_mine_mesa_801856B4);
                }
            }
            break;
    }
    return 0;
}

s32 func_mine_mesa_8017DBC4(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    u8 field9;

    field9 = gGameSession->at4.loc.place;
    if (field9 == 1) {
        if (GameFlag_GetNibble(0xCD) == 0) {
            if (gameGetPtrSlot(0xA) != NULL) {
                Gp_StateC08.field_6 |= 1;
                Gp_PulseState1C();
                D_mine_mesa_80189B50 = field9;
                GameFlag_SetNibble(0xCD, 1);
            }
        } else if (D_mine_mesa_80189B4C != NULL) {
            Gp_DispatchMsg(D_mine_mesa_80189B4C, 0x13F4, arg2, arg3);
        }
    }
    return 0;
}

void func_mine_mesa_8017DC80(Task* arg0)
{
    arg0->msgTable = D_mine_mesa_80181904;
    Game_SetPtrSlot(arg0, 7);
    if (GameFlag_GetNibble(0x90) == 0) {
        if (gameGetPtrSlot(0xA) != NULL) {
            D_80072830 = 5;
            Task_SpawnFromTable(&D_mine_mesa_80181990, 0, 0, 0);
        }
        GameFlag_SetNibble(0x1BD, 0);
    } else {
        func_mine_mesa_8017DD44();
    }
    D_80062735 = 1;
    func_mine_mesa_8017EB38();
    D_mine_mesa_80189B4C = NULL;
    func_mine_mesa_801817BC();
    arg0->state          = arg0->state + 1;
    D_mine_mesa_80189B50 = 0;
}

void func_mine_mesa_8017DD44(void)
{
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[0]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[1]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[2]);
    Gp_UnlinkObj4A(0, &D_mine_mesa_801890EC[3]);
}

/// State handlers of the room task `func_mine_mesa_8017DD98` drives: the
/// set-up tick, the per-frame tick and `taskKill`.
const TaskFuncTable3 D_mine_mesa_8017D5D8 = {
    { func_mine_mesa_8017DC80, func_mine_mesa_8017D808, taskKill },
};

void func_mine_mesa_8017DD98(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_mine_mesa_8017D5D8;
    sp.funcs[task->state](task);
}

void func_mine_mesa_8017DDF0(void)
{
    if (D_mine_mesa_80189B4C == NULL) {
        D_mine_mesa_80189B4C = Task_SpawnFromTable(&D_mine_mesa_80189B2C, 0, 0, 0);
    }
}

void func_mine_mesa_8017DE38(Task* arg0)
{
    u8          slotParam[4];
    GameLoc     key;
    CdCmdQueue* queue;
    Task*       task;

    task  = arg0;
    queue = &CdCmd_Queue;
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
    }
    return;

L_case0:
    SetDispMask(0);
    Mem_AllocAuxWithImages(1);
    goto advance;

L_case1:
    key          = gGameSession->at4;
    key.loc.view = 0x64;
    slotParam[0] = Stream_FindSlot(key.raw.data, 0, 0);
    CdCmd_Enqueue(0x61, 0, slotParam);
    goto advance;

L_case2:
    if (queue->field_1FA == 0) {
        return;
    }
    SetDispMask(1);
    goto advance;

L_case3:
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

L_case4:
    if ((CdCmd_IsIdle() & 0xFFFF) == 0) {
        return;
    }
    Stream_ResetRestoreState();
advance:
    task->state = task->state + 1;
    return;

L_case5:
    if ((Stream_RestoreAfterLoad(0, 1) & 0xFFFF) == 0) {
        return;
    }
    taskKill(task);
    Display_ResetHeapWrapper();
}

void func_mine_mesa_8017DFC4(Task* arg0)
{
    u16 temp_v0;

    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
    temp_v0             = arg0->killCountdown + 4;
    arg0->killCountdown = temp_v0;
    if ((s16)temp_v0 >= 0x100) {
        taskKill(arg0);
    }
}

void func_mine_mesa_8017E024(Task* arg0)
{
    Display_SpawnWithOt(&D_mine_mesa_80181990, 1, 0, 0);
    D_8007106B = 1;
    Gp_SpawnViewTasks();
    taskKill(arg0);
}

/// Walks the mesa one step along `D_mine_mesa_80184184` per frame: sends slot 3
/// that entry as a `GpXformArg` -- the table position with x pulled back
/// 0x64 and z pushed out 0xC8 -- and advances `killCountdown`. At 0x2E the mesa
/// has finished its run, and the task kills itself; the session's overlay-wait
/// gate cuts the run short the same way.
void func_mine_mesa_8017E074(Task* arg0)
{
    GpXformArg rec;

    if (arg0->killCountdown >= 0x2E || gGameSession->evtSkipped != 0) {
        taskKill(arg0);
        return;
    }
    rec.pos.vx  = D_mine_mesa_80184184[arg0->killCountdown].vx;
    rec.pos.vy  = D_mine_mesa_80184184[arg0->killCountdown].vy;
    rec.pos.vz  = D_mine_mesa_80184184[arg0->killCountdown].vz;
    rec.pos.vx -= 0x64;
    rec.pos.vz += 0xC8;
    rec.rot.vx  = 0;
    rec.rot.vy  = 0x311;
    rec.rot.vz  = 0;
    arg0->killCountdown++;
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3E9, (s32)&rec, 0);
}

/// Head-aim state of the mesa's run task, run only while `D_801156F9` is clear:
/// a missing slot-3 or slot-0xA task parks the state machine on -1. State 0
/// allocates the `GpHeadAim` record into `Task::work` and seeds its
/// clamps to 0x300 yaw and 0x200 pitch; state 1 ramps its `rate` up toward
/// 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it is
/// not, then hands the record to `func_800B17D4` between the slot-3 task whose
/// head turns and the slot-0xA task it turns toward -- the mirror of
/// `func_mine_mesa_8017E2A4`, which looks from slot 0xA. Every other state
/// kills the task and clears `D_mine_mesa_80189B54`, and a state-0 NULL
/// allocation falls out of its own `if` into that same kill.
void func_mine_mesa_8017E15C(Task* arg0)
{
    Task*      turner;
    Task*      looker;
    GpHeadAim* aim;
    s32        state;
    u16        rateUp;
    u16        rateDown;

    turner = gameGetPtrSlot(3);
    looker = gameGetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if ((turner == NULL) || (looker == NULL)) {
            arg0->state = -1;
        }
        state = arg0->state;
        switch (state) {
            case 0:
                aim = memCalloc(sizeof(GpHeadAim), false);
                if (aim != NULL) {
                    arg0->work      = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x200;
                    arg0->state++;
                        /* fallthrough */
                    case 1:
                        aim = (GpHeadAim*)arg0->work;
                        if (arg0->spawnArg1 != 0) {
                            rateUp    = aim->rate + 0x100;
                            aim->rate = rateUp;
                            if ((s16)rateUp >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rateDown  = aim->rate - 0x100;
                            aim->rate = rateDown;
                            if ((s16)rateDown < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(turner, looker, aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(arg0);
                D_mine_mesa_80189B54 = NULL;
                break;
        }
    }
}

/// Head-aim state of the mesa's tracked task, run only while `D_801156F9` is
/// clear: a missing `gameGetPtrSlot(0xA)` task parks the state machine on -1.
/// State 0 allocates the `GpHeadAim` record into `Task::work` and seeds
/// its clamps to 0x300 yaw and 0x100 pitch; state 1 ramps its `rate` up toward
/// 0x1000 while `Task::spawnArg1` is set and back down toward 0 while it is
/// not, then hands the record to `func_800B17D4` between the
/// `gameGetPtrSlot(0xA)` task whose head turns and the slot-3 task it turns
/// toward -- the reverse of `func_mine_mesa_8017E15C` and of
/// `func_actor_450200_80131FA8`, which look from slot 3. Every other state
/// kills the task and clears `D_mine_mesa_80189B58`, and a state-0 NULL
/// allocation falls out of its own `if` into that same kill.
void func_mine_mesa_8017E2A4(Task* arg0)
{
    Task*      looker;
    GpHeadAim* aim;
    u16        rate;

    looker = gameGetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if (looker == NULL) {
            arg0->state = -1;
        }
        switch (arg0->state) {
            case 0:
                aim = memCalloc(sizeof(GpHeadAim), false);
                if (aim != NULL) {
                    arg0->work      = (TaskIdMap*)aim;
                    aim->yawLimit   = 0x300;
                    aim->pitchLimit = 0x100;
                    arg0->state++;
                        /* fallthrough */
                    case 1:
                        aim = (GpHeadAim*)arg0->work;
                        if (arg0->spawnArg1 != 0) {
                            rate      = aim->rate + 0x100;
                            aim->rate = rate;
                            if ((s16)rate >= 0x1001) {
                                aim->rate = 0x1000;
                            }
                        } else {
                            rate      = aim->rate - 0x100;
                            aim->rate = rate;
                            if ((s16)rate < 0) {
                                aim->rate = 0;
                            }
                        }
                        func_800B17D4(looker, gameGetPtrSlot(3), aim);
                        return;
                }
                /* fallthrough */
            default:
                taskKill(arg0);
                D_mine_mesa_80189B58 = NULL;
                break;
        }
    }
}

/// State machine of the mesa's flare: a full-screen semi-transparent tile
/// whose colour is `Task::killCountdown`, dimmed 8 a frame from state 2 until
/// it goes negative and the task kills itself; the other states are the run-in
/// (0 seeds the countdown at 0xFF, 1 waits out `Task::spawnArg1`) and anything
/// else kills the task outright. The tile and a `DR_TPAGE` for it are carved
/// off `gGpuPrimCursor` and linked into `gGpuCurrentOt[3]` every frame,
/// including the frames the switch kills the task on -- only the colours differ
/// there, since `r`/`g`/`b` are read before the switch.
///
/// This is `func_actor_503500_80132990` minus its `Gp_StateF0.field_4` gate and minus
/// the `gGameSession->evtSkipped != 0` term of its state-1 test; the tile packet
/// itself is built byte-for-byte the same way.
void func_mine_mesa_8017E3E0(Task* arg0)
{
    TILE*     tile;
    DR_TPAGE* dr;
    u8        r, g, b;

    r = g = b = arg0->killCountdown;
    switch (arg0->state) {
        case 0:
            arg0->killCountdown = 0xFF;
            arg0->state++;
            break;
        case 1:
            if (--arg0->spawnArg1 < 0) {
                arg0->state++;
            }
            break;
        case 2:
            arg0->killCountdown -= 8;
            if (arg0->killCountdown < 0) {
                taskKill(arg0);
            }
            break;
        default:
            taskKill(arg0);
            break;
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

void func_mine_mesa_8017E5A0(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}

void func_mine_mesa_8017E5C0(void)
{
    CdCmd_EnqueueOverlay81();
}

void func_mine_mesa_8017E5E0(void)
{
    Gp_RestoreStreamRng();
}

/// Room script callback: clear the queued CD command and restart the CD queue.
void func_mine_mesa_8017E600(void)
{
    CdCmd_CancelReplaceAndActivate();
}

void func_mine_mesa_8017E620(void)
{
    Task_SpawnFromTable(&D_mine_mesa_801842F4, 0, 0, 0);
}

void func_mine_mesa_8017E650(void)
{
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&D_mine_mesa_801842F4, 1, 0, 0);
}

/// Hands `arg0` to the task in `D_mine_mesa_80189B54` as its `spawnArg1` when
/// it is 0 or 1; any other value kills the task and clears the handle.
void func_mine_mesa_8017E684(s32 arg0)
{
    Task* t = D_mine_mesa_80189B54;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    taskKill(D_mine_mesa_80189B54);
    D_mine_mesa_80189B54 = NULL;
}

void func_mine_mesa_8017E6D8(void)
{
    D_mine_mesa_80189B58 = Task_SpawnFromTable(&D_mine_mesa_801842F4, 2, 0, 0);
}

void func_mine_mesa_8017E70C(s32 arg0)
{
    if (D_mine_mesa_80189B58 != NULL) {
        if (arg0 < 2) {
            if (arg0 >= 0) {
                D_mine_mesa_80189B58->spawnArg1 = arg0;
                return;
            }
        }
        taskKill(D_mine_mesa_80189B58);
        D_mine_mesa_80189B58 = NULL;
    }
}

void func_mine_mesa_8017E760(void)
{
    if (D_mine_mesa_80189B54 != NULL) {
        taskKill(D_mine_mesa_80189B54);
    }
    D_mine_mesa_80189B54 = Task_SpawnFromTable(&D_mine_mesa_801842F4, 3, 0, 0);
}

/// Head-aim driver for the slot-3 skeleton: turns its head toward the slot-A
/// task, with a 0x300 yaw and 0x10 pitch limit and a step of
/// `killCountdown / 0x1000` of the remaining angle. `killCountdown` ramps up
/// while `spawnArg1` is set and back down otherwise. While the pause flag
/// `D_801156F9` is clear, a missing slot forces `state` to -1, which kills the
/// task and clears the spawner's pointer (`func_mine_mesa_8017E760`).
void func_mine_mesa_8017E7B0(Task* task)
{
    void* slot3;
    void* slotA;
    u16   tick;

    slot3 = gameGetPtrSlot(3);
    slotA = gameGetPtrSlot(0xA);
    if (D_801156F9 == 0) {
        if ((slot3 == NULL) || (slotA == NULL)) {
            task->state = -1;
        }
        if (task->state == 0) {
            if (task->spawnArg1 != 0) {
                tick                = task->killCountdown + 0x100;
                task->killCountdown = tick;
                if ((s16)tick >= 0x1001) {
                    task->killCountdown = 0x1000;
                }
            } else {
                tick                = task->killCountdown - 0x100;
                task->killCountdown = tick;
                if ((s16)tick < 0) {
                    task->killCountdown = 0;
                }
            }
            func_800B0928(slot3, slotA, 0x300, 0x10, task->killCountdown);
            return;
        }
        taskKill(task);
        D_mine_mesa_80189B54 = NULL;
    }
}

void func_mine_mesa_8017E8B0(s32 arg0)
{
    D_mine_mesa_80189B5C = Task_SpawnFromTable(&D_mine_mesa_801842F4, 4, arg0, 0);
    Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
}

void func_mine_mesa_8017E8FC(s32 arg0)
{
    if (D_mine_mesa_80189B5C != NULL) {
        D_mine_mesa_80189B5C->state = arg0;
    }
}

void func_mine_mesa_8017E91C(void)
{
    gGameSession->flowFlags |= 3;
}

void func_mine_mesa_8017E93C(u8 arg0)
{
    D_80062735 = arg0;
}

void func_mine_mesa_8017E948(void)
{
    Task_SpawnFromTable(&D_mine_mesa_801842F4, 5, 0, 0);
}

void func_mine_mesa_8017E978(Task* arg0)
{
    u16 temp_v0;

    temp_v0             = arg0->killCountdown + 1;
    arg0->killCountdown = temp_v0;
    switch ((s16)temp_v0) {
        case 0x2F:
            SndEvt_EnqueueType6(0x10000039, 0, 0);
            break;
        case 0x39:
            SndEvt_EnqueueType6(0x1000003A, 0, 0);
            break;
    }
    if ((gGameSession->eventState == 0) || ((s16)arg0->killCountdown >= 0x39)) {
        taskKill(arg0);
    }
}

void func_mine_mesa_8017EA24(void)
{
    if (GameFlag_GetNibble(0x4C) != 0) {
        GameFlag_SetNibble(0x4C, 0);
        D_8007217B = 0;
        Task_CallExit(gameGetPtrSlot(0xA));
        Game_SetPtrSlot(NULL, 0xA);
    }
}

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_mine_mesa_8017EA78(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}

void func_mine_mesa_8017EAAC(void)
{
    gGameSession->viewDirty = 1;
}

void func_mine_mesa_8017EAC0(void)
{
    Task* slot;

    slot = gameGetPtrSlot(0xA);
    if (slot != NULL) {
        Gp_SpawnEff(0x6002B, &((TmdObject*)slot->extra)->coords[8], 0x21, NULL);
        SndEvt_EnqueueType6(0x40650001, 0, 0);
    }
}

void func_mine_mesa_8017EB18(void)
{
    Gp_HaltPadScripts();
}

void func_mine_mesa_8017EB38(void)
{
    D_mine_mesa_80189B54 = NULL;
    D_mine_mesa_80189B58 = NULL;
    D_mine_mesa_80189B5C = NULL;
}

/// Refreshes the live layout from the template. It copies the three `field_4`
/// entries with their `field_C` records and the eight `field_8` entries, then
/// shifts every `field_8` entry by (0x1838, -0xB4, 0x9F6), or by
/// (0x1838, 0x7D0, 0x9F6) when `arg0` is non-zero.
void func_mine_mesa_8017EB54(s32 arg0)
{
    GpGridParams* dst = &D_mine_mesa_8018700C;
    GpGridParams* src = &D_mine_mesa_801864A4;
    SVECTOR       ofs;
    s32           i;

    for (i = 0; i < 3; i++) {
        dst->field_4[i].vx = src->field_4[i].vx;
        dst->field_4[i].vy = src->field_4[i].vy;
        dst->field_4[i].vz = src->field_4[i].vz;
        dst->field_C[i]    = src->field_C[i];
    }
    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx = src->field_8[i].vx;
        dst->field_8[i].vy = src->field_8[i].vy;
        dst->field_8[i].vz = src->field_8[i].vz;
    }
    if (arg0 == 0) {
        ofs.vx = 0x1838;
        ofs.vy = -0xB4;
    } else {
        ofs.vx = 0x1838;
        ofs.vy = 0x7D0;
    }
    ofs.vz = 0x9F6;
    for (i = 0; i < 8; i++) {
        dst->field_8[i].vx += ofs.vx;
        dst->field_8[i].vy += ofs.vy;
        dst->field_8[i].vz += ofs.vz;
    }
}

/// Publishes the mesa's three effect ids as `Gp_State1C->roomEffectMode` variant `2`
/// on the task's first tick, then draws every emitter the current camera view
/// shows: one `func_mine_mesa_8017EFA8` quad per position, texture column 1
/// and half-extent 0x200, except the column-0, 0x300 positions of views 2 and
/// 5.
void func_mine_mesa_8017ED08(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758                 = 0x600E9;
        D_8011572C                 = 0x600EB;
        D_80115750                 = 0x600EC;
        Gp_State1C->roomEffectMode = 2;
        arg0->state                = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2: {
            SVECTOR* p = D_mine_mesa_801864D0;
            func_mine_mesa_8017EFA8(&p[0], 0, 0x300);
            func_mine_mesa_8017EFA8(&p[1], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[2], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[3], 1, 0x200);
            break;
        }
        case 4: {
            SVECTOR* p = D_mine_mesa_801864F0;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[1], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[2], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[3], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[6], 1, 0x200);
            break;
        }
        case 5: {
            SVECTOR* p = D_mine_mesa_801864C8;
            func_mine_mesa_8017EFA8(&p[0], 0, 0x300);
            func_mine_mesa_8017EFA8(&p[1], 0, 0x300);
            func_mine_mesa_8017EFA8(&p[5], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[6], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[8], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[9], 1, 0x200);
            break;
        }
        case 6: {
            SVECTOR* p = D_mine_mesa_801864F0;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[1], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[4], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[5], 1, 0x200);
            break;
        }
        case 8: {
            SVECTOR* p = D_mine_mesa_801864F0;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            break;
        }
        case 9: {
            SVECTOR* p = D_mine_mesa_80186508;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[1], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[2], 1, 0x200);
            break;
        }
        case 10: {
            SVECTOR* p = D_mine_mesa_801864D8;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[2], 1, 0x200);
            break;
        }
        case 11: {
            SVECTOR* p = D_mine_mesa_801864F0;
            func_mine_mesa_8017EFA8(&p[0], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[1], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[2], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[3], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[4], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[5], 1, 0x200);
            func_mine_mesa_8017EFA8(&p[6], 1, 0x200);
            break;
        }
    }
}

/// Projects the world-space point `arg0` through `Gfx_ViewWorldMtx` and, when
/// `gte_stflg` is non-negative, queues one semi-transparent `POLY_FT4` (tpage
/// 0x2B, clut `(arg1 & 0x3F) | 0x4380`). `arg1` selects the 40-texel UV column
/// `(s16)arg1 * 40` at v=0..0x27. `arg2` is a signed half-extent; the
/// on-screen radius is `(s16)arg2 * 39 / otz`. RGB is the frame-counter blend
/// byte `((animFrame & 1) * 16) + 0x20` on all three channels.
void func_mine_mesa_8017EFA8(SVECTOR* arg0, s32 arg1, s32 arg2)
{
    void**             scratch;
    u8*                head;
    u8*                tmp;
    RoomDraw13Scratch* block;
    POLY_FT4*          prim;
    DisplayState*      ds;
    s32                tex;
    s32                idx;
    s32                u0;
    s32                u1;
    register s32       sarg asm("v1");
    s32                t;
    s32                field8;
    s32                blend;
    s32                v;
    u8                 code;
    s16                xy;

    tex = arg1;
    CLOBBER_REG(a1);
    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    tmp     = head - 0x10;
    block   = (RoomDraw13Scratch*)tmp;
    SOFT_TOUCH_REG(block);
    *scratch = tmp;

    gte_SetTransMatrix(&Gfx_ViewWorldMtx);
    gte_SetRotMatrix(&Gfx_ViewWorldMtx);
    gte_ldv0(arg0);
    gte_rtps();
    gte_stsxy(&((RoomDraw13Scratch*)(head - 0x10))->sx);
    gte_stflg(&((RoomDraw13Scratch*)(head - 0x10))->flag);
    if (((RoomDraw13Scratch*)tmp)->flag >= 0) {
        gte_stszotz(&block->otz);
        prim           = (POLY_FT4*)gGpuPrimCursor;
        ds             = &gDisplayState;
        gGpuPrimCursor = prim + 1;
        setlen(prim, 9);
        setcode(prim, 0x2C);
        idx         = (s16)tex;
        field8      = (u8)ds->animFrame;
        prim->tpage = 0x2B;
        prim->clut  = (idx & 0x3F) | 0x4380;
        u0          = idx * 40;
        u1          = u0 + 0x27;
        prim->u0    = u0;
        prim->u2    = u0;
        SOFT_USE_REG(u0);
        v        = 0x27;
        prim->u1 = u1;
        prim->u3 = u1;
        SOFT_USE_REG(u1);
        sarg     = arg2 << 16;
        prim->v2 = v;
        prim->v3 = v;
        SCHED_BARRIER();
        code     = prim->code;
        sarg     = sarg >> 16;
        prim->v0 = 0;
        prim->v1 = 0;
        blend    = ((field8 & 1) * 16) + 0x20;
        COMPILER_BARRIER();
        prim->code = code | 2;
        t          = sarg * 40;
        setRGB0(prim, blend, blend, blend);
        ((RoomDraw13Scratch*)tmp)->radius =
            (t - sarg) / ((RoomDraw13Scratch*)(head - 0x10))->otz;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x2 = xy;
        prim->x0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sx + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->x3 = xy;
        prim->x1 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy - *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y1 = xy;
        prim->y0 = xy;
        xy       = *(u16*)&((RoomDraw13Scratch*)tmp)->sy + *(u16*)&((RoomDraw13Scratch*)tmp)->radius;
        prim->y3 = xy;
        prim->y2 = xy;
        addPrim((u_long*)(((((u32)((RoomDraw13Scratch*)(head - 0x10))->otz << ds->otDepthShift) >> 2) & 0xFFC) +
                          (s32)gGpuCurrentOt),
                prim);
    }
    SCRATCH_POP_BYTES_AT(scratch, 0x10);
}

/// Drives one expanding flash burst over `spawnArg1` frames: state 1 draws two
/// growing discs and a closing ring each frame, then whites the screen out;
/// state 2 draws a fading glow until it is dim enough, and the task then hands
/// its `GpEffWork` block back. It releases the block early while
/// `Gp_State1C->eventState` is 4 or more, and draws nothing while it is
/// non-zero.
void func_mine_mesa_8017F230(Task* task)
{
    GpEffWork*     work;
    GsCOORDINATE2* coord;
    u8             rgb[3];

    work  = task->spawnArg2;
    coord = ((TmdObject*)task->extra)->coords;
    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
    } else {
        Gp_UpdateCoord(coord);
        work->age++;
        switch (task->state) {
            case 0:
                work->scale = 0;
                work->angle = 0x80;
                work->step  = 0x100 / task->spawnArg1;
                task->state = 1;
                break;
            case 1:
                work->scale += work->step;
                work->angle += work->step;
                task->spawnArg1--;
                rgb[0] = work->scale;
                rgb[1] = work->scale >> 2;
                rgb[2] = work->scale >> 1;
                func_mine_mesa_8017F900(coord, work->angle, rgb);
                rgb[0] >>= 1;
                rgb[1] >>= 1;
                rgb[2] >>= 1;
                func_mine_mesa_8017F900(coord, (s16)((u16)work->angle * 2), rgb);
                func_mine_mesa_8017F4D4(coord, (s16)(0x300 - (u16)work->angle * 2), 0x80, rgb);
                if (task->spawnArg1 == 0) {
                    work->scale = 0xFF;
                    task->state = 2;
                    rgb[0]      = work->scale;
                    rgb[1]      = work->scale >> 2;
                    rgb[2]      = work->scale >> 1;
                    Gp_DrawFadeQuad(rgb, 1);
                }
                break;
            case 2:
                if (work->scale >= 0x11) {
                    rgb[0] = work->scale;
                    rgb[1] = work->scale >> 2;
                    rgb[2] = work->scale >> 1;
                    func_mine_mesa_80180804(coord, (s16)(work->angle * 3), rgb);
                    work->scale -= 0x10;
                    work->angle -= 8;
                    break;
                }
                /* fallthrough */
            case 3:
                Gp_ReleaseState1CMem(work, task);
                break;
        }
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues sixteen gouraud `POLY_G4` wedges that
/// form a ring. `arg1` is the inner half-extent and `arg2` the extra outer
/// width; on-screen radii are `(s16)arg1 * 64 / (otz + 1)` and
/// `(s16)(arg1 + arg2) * 64 / (otz + 1)`. The RGB triple tints the edge at the
/// second radius, and each wedge fades to black at the first.
void func_mine_mesa_8017F4D4(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb)
{
    RoomDraw02Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    register s32       saved asm("t1");
    register u8*       head asm("t0");
    s32                sum;
    s32                otz;
    register s32       rOuter asm("v0");
    s32                rInner;
    u8*                color;
    s32                t;
    u16                vz;
    u16                vx;
    u32                maskLo;
    u32                maskHi;

    saved   = arg1;
    scratch = (void**)G_SCRATCH_HEAD;
    color   = rgb;
    head    = *scratch;
    USE_REG(head);
    vx = *(u16*)&arg0->workm.t[0];
    USE_REG(vx);
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomDraw02Scratch*)tmp;
    }
    block->vec.vx = vx;
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    sum           = saved + arg2;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&((RoomDraw02Scratch*)(head - 0x1C))->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw02Scratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomDraw02Scratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&block->otz);
        USE_REG(head);
        otz                                      = ((RoomDraw02Scratch*)(head - 0x1C))->otz + 1;
        rOuter                                   = ((s16)saved * 64) / otz;
        ((RoomDraw02Scratch*)(head - 0x1C))->otz = otz;
        rInner                                   = (s16)sum * 64;
        block->rOuter                            = rOuter;
        rInner                                   = rInner / ((RoomDraw02Scratch*)(head - 0x1C))->otz;
        ang                                      = 0;
        block->rInner                            = rInner;

        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, color[0], color[1], color[2]);
            setRGB3(prim, color[0], color[1], color[2]);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            t        = ang + 0x100;
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            prim->x2 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y2 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(t)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(t)) >> 12);
            ang      = t;
            maskLo   = 0xFFFFFF;
            maskHi   = 0xFF000000;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG2(maskLo, maskHi);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues eight gouraud `POLY_G4` wedges around
/// the projected centre. `arg1` is a signed half-extent; the on-screen radius
/// is `(s16)arg1 * 64 / (otz + 1)`. The RGB triple in `rgb` lights only the
/// inner vertex so each wedge fades to black.
void func_mine_mesa_8017F900(GsCOORDINATE2* arg0, s32 arg1, u8* rgb)
{
    RoomDraw04Scratch* block;
    POLY_G4*           prim;
    s32                ang;
    register void**    scratch asm("a1");
    u8*                head;
    s32                otz;
    s32                radius;
    s32                t;
    s32                t2;
    u16                vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    USE_REG(head);
    {
        register u16 vx asm("v0");
        vx                                          = *(u16*)&arg0->workm.t[0];
        ((RoomDraw04Scratch*)(head - 0x18))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x18;
        block = (RoomDraw04Scratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomDraw04Scratch*)(head - 0x18))->sx);
    gte_stflg(&((RoomDraw04Scratch*)(head - 0x18))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomDraw04Scratch*)(head - 0x18))->otz);
        USE_REG(head);
        otz           = block->otz + 1;
        radius        = ((s16)arg1 * 64) / otz;
        block->otz    = otz;
        block->radius = radius;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, rgb[0], rgb[1], rgb[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->radius * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->radius * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->radius * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->radius * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->radius * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->radius * rcos(t2)) >> 12);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
            SOFT_USE_REG(t2);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x18);
}

/// Drives a twin trail: state 0 allocates sixteen coordinates, eight per trail,
/// and seeds them all from the two origins; state 1 moves one coordinate of
/// each trail per frame, cycling through the eight, and draws the pair with
/// `func_mine_mesa_80180184`. The task releases its `GpEffWork` block once
/// `age` reaches `spawnArg1`, and idles while `Gp_State1C->eventState` is 2 or
/// more.
void func_mine_mesa_8017FC94(Task* task)
{
    GsCOORDINATE2  coord;
    GsCOORDINATE2* coords;
    GsCOORDINATE2* objCoord;
    GsCOORDINATE2* dst;
    GpEffWork*     work;
    SVECTOR*       vec;
    s32            i;

    coords   = (GsCOORDINATE2*)task->work;
    work     = (GpEffWork*)task->spawnArg2;
    objCoord = ((TmdObject*)task->extra)->coords;

    if (Gp_State1C->eventState < 2) {
        work->age++;
        switch (task->state) {
            case 0:
                coords = (GsCOORDINATE2*)memCalloc(0x500, 0);
                if (coords == NULL) {
                    work->age = 0;
                    return;
                }
                task->work           = (TaskIdMap*)coords;
                objCoord->sub        = work->parent;
                objCoord->coord.t[0] = D_mine_mesa_80186528[0].vx;
                objCoord->coord.t[1] = D_mine_mesa_80186528[0].vy;
                objCoord->coord.t[2] = D_mine_mesa_80186528[0].vz;
                objCoord->flg        = 0;
                Gp_UpdateCoord(objCoord);
                task->state      = 1;
                coord.sub        = work->parent;
                vec              = &D_mine_mesa_80186528[1];
                coord.coord.t[0] = vec->vx;
                coord.coord.t[1] = vec->vy;
                coord.coord.t[2] = vec->vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                for (i = 0; i < 8; i++) {
                    dst        = &coords[i];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = objCoord->workm;
                    gte_SetRotMatrix(&objCoord->workm);
                    gte_SetTransMatrix(&objCoord->workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                    dst        = &coords[i + 8];
                    dst->sub   = &gGfxViewCoord;
                    dst->workm = coord.workm;
                    gte_SetRotMatrix(&coord.workm);
                    gte_SetTransMatrix(&coord.workm);
                    Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                }
                break;

            case 1:
                objCoord->flg = 0;
                Gp_UpdateCoord(objCoord);
                coord.sub        = work->parent;
                coord.coord.t[0] = D_mine_mesa_80186530.vx;
                coord.coord.t[1] = D_mine_mesa_80186530.vy;
                coord.coord.t[2] = D_mine_mesa_80186530.vz;
                coord.flg        = 0;
                Gp_UpdateCoord(&coord);
                dst        = &coords[work->age & 7];
                dst->sub   = &gGfxViewCoord;
                dst->workm = objCoord->workm;
                gte_SetRotMatrix(&objCoord->workm);
                gte_SetTransMatrix(&objCoord->workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                dst        = &coords[(work->age & 7) + 8];
                dst->sub   = &gGfxViewCoord;
                dst->workm = coord.workm;
                gte_SetRotMatrix(&coord.workm);
                gte_SetTransMatrix(&coord.workm);
                Gp_WorldToLocal(&gGfxViewCoord.workm, &dst->workm, &dst->coord);
                for (i = 0; i < 8; i++) {
                    dst      = &coords[i];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                    dst      = &coords[i + 8];
                    dst->flg = 0;
                    Gp_UpdateCoord(dst);
                }
                func_mine_mesa_80180184(coords, &coords[8], work->age & 7, 0x123);
                if (work->age == task->spawnArg1 && work->age != 0) {
                    Gp_ReleaseState1CMem(work, task);
                }
                break;
        }
    }
}

/// Draws the two eight-slot coordinate trails as seven gouraud `POLY_G4`
/// quads, walking backwards from `arg2`. Each quad spans `workm.t` of two
/// adjacent slots on `arg0` and `arg1`. The leading edge is scaled by
/// `0x40 - 9 * i` and the trailing edge by nine less. `arg3` is the beam
/// colour whose channels multiply that fade: red is `arg3 >> 8`, green and
/// blue the 2-bit fields at bits 4 and 0. A quad is dropped when `gte_stflg`
/// is negative.
void func_mine_mesa_80180184(GsCOORDINATE2* arg0, GsCOORDINATE2* arg1, s16 arg2, s16 arg3)
{
    RoomDraw03Scratch* blk;
    GsCOORDINATE2*     a;
    GsCOORDINATE2*     b;
    POLY_G4*           prim;
    s32                i;
    s32                j;
    s32                i0;
    s32                i1;
    s32                hi;
    s32                lo;
    s32                fade;
    s32                r;
    s32                g;
    s32                bl;
    s32                r2;
    s32                g2;
    s32                b2;

    {
        register u8* tmp asm("v0");

        tmp                = SCRATCH_HEAD(u8) - sizeof(RoomDraw03Scratch);
        blk                = (RoomDraw03Scratch*)tmp;
        SCRATCH_HEAD(void) = tmp;
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    i = 0;
    do {
        j            = arg2 - i;
        i0           = j & 7;
        a            = &arg0[i0];
        blk->v[0].vx = *(u16*)&a->workm.t[0];
        j            = j - 1;
        blk->v[0].vy = *(u16*)&a->workm.t[1];
        i1           = j & 7;
        blk->v[0].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i0];
        blk->v[1].vx = *(u16*)&b->workm.t[0];
        blk->v[1].vy = *(u16*)&b->workm.t[1];
        blk->v[1].vz = *(u16*)&b->workm.t[2];
        a            = &arg0[i1];
        blk->v[2].vx = *(u16*)&a->workm.t[0];
        blk->v[2].vy = *(u16*)&a->workm.t[1];
        blk->v[2].vz = *(u16*)&a->workm.t[2];
        b            = &arg1[i1];
        blk->v[3].vx = *(u16*)&b->workm.t[0];
        blk->v[3].vy = *(u16*)&b->workm.t[1];
        blk->v[3].vz = *(u16*)&b->workm.t[2];
        gte_ldv0(&blk->v[0]);
        gte_rtps();
        gte_stsxy(&blk->sx0);
        gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
        gte_rtpt();
        gte_stsxy3(&blk->sx1, &blk->sx2, &blk->sx3);
        gte_stflg(&blk->flag);
        if (blk->flag >= 0) {
            gte_stszotz(&blk->otz);
            fade           = 0x40 - i * 9;
            hi             = fade & 0xFF;
            r              = hi * (arg3 >> 8);
            g              = hi * ((arg3 >> 4) & 3);
            bl             = hi * (arg3 & 3);
            lo             = (fade - 9) & 0xFF;
            r2             = lo * (arg3 >> 8);
            g2             = lo * ((arg3 >> 4) & 3);
            prim           = (POLY_G4*)gGpuPrimCursor;
            blk->otz       = blk->otz + 1;
            gGpuPrimCursor = prim + 1;
            setlen(prim, 8);
            b2 = lo * (arg3 & 3);
            setcode(prim, 0x38);
            prim->r0 = r;
            prim->r1 = r;
            prim->g0 = g;
            prim->g1 = g;
            prim->b0 = bl;
            prim->b1 = bl;
            prim->r2 = r2;
            prim->r3 = r2;
            prim->g2 = g2;
            prim->g3 = g2;
            prim->b2 = b2;
            prim->b3 = b2;
            prim->x0 = blk->sx0;
            prim->y0 = blk->sy0;
            prim->x1 = blk->sx1;
            prim->y1 = blk->sy1;
            prim->x2 = blk->sx2;
            prim->y2 = blk->sy2;
            prim->x3 = blk->sx3;
            prim->y3 = blk->sy3;
            addPrim((u_long*)(((((u32)blk->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) +
                              (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, blk->otz);
        }
        i += 1;
    } while (i < 7);
    SCRATCH_POP_BYTES(sizeof(RoomDraw03Scratch));
}

void func_mine_mesa_8018057C(Task* task)
{
    GsCOORDINATE2* objCoord;
    GpEffWork*     work;
    u8             rgb[4];

    objCoord = ((TmdObject*)task->extra)->coords;
    work     = (GpEffWork*)task->spawnArg2;

    if (Gp_State1C->eventState != 0) {
        if (Gp_State1C->eventState >= 4) {
            Gp_ReleaseState1CMem(work, task);
        }
        return;
    }

    Gp_UpdateCoord(objCoord);
    work->age++;

    switch (task->state) {
        case 0:
            Gp_SpawnEff(0x60076, objCoord, 0x400, NULL);
            if (task->spawnArg1 != 0) {
                Gp_SpawnEff(0x60070, objCoord, 0x80004600, NULL);
                task->state = 1;
            } else {
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                Gp_SpawnEff(0x6007C, objCoord, 0x100, NULL);
                work->scale = 0x100;
                work->angle = 0xC0;
                task->state = 2;
            }
            break;

        case 1:
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vx = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vy = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            work->move.vz = 0x100 - (((u32)Gp_LcgState >> 16) & 0x1FF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            Gp_SpawnEff(0x60070, objCoord, (((u32)Gp_LcgState >> 16) & 0x1FF) | 0x82003400,
                        &work->move);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 2:
            work->angle -= 0x20;
            work->scale += 0x30;
            rgb[0]       = work->angle;
            rgb[1]       = work->angle >> 1;
            rgb[2]       = work->angle >> 2;
            func_mine_mesa_8017F4D4(objCoord, 0x100, 0x100, rgb);
            func_mine_mesa_8017F4D4(objCoord, work->scale, work->scale, rgb);
            if (work->age >= 7) {
                task->state = 3;
            }
            break;

        case 3:
            Gp_ReleaseState1CMem(work, task);
            break;
    }
}

/// Projects the coordinate's world position through `GsWSMATRIX` and, when
/// the GTE flag is non-negative, queues gouraud `POLY_G4` wedges around the
/// projected centre: eight at the outer radius tinted at half brightness, eight
/// at half that radius at full brightness, then four spikes between the inner
/// and outer radii. `arg1` is a signed half-extent; on-screen radii are
/// `(s16)arg1 * 64 / (otz + 1)` (outer) and `(s16)arg1 * 8 / (otz + 1)`
/// (inner). Only the centre vertex is tinted, so each wedge fades to a black
/// rim.
void func_mine_mesa_80180804(GsCOORDINATE2* arg0, s16 arg1, u8* arg2)
{
    register RoomBillboardScratch* block asm("s3");
    register POLY_G4*              prim asm("s2");
    register s32                   ang asm("s4");
    register void**                scratch asm("a1");
    register u8*                   head asm("a2");
    s32                            t;
    s32                            t2;
    s32                            u;
    u16                            vz;

    scratch = (void**)G_SCRATCH_HEAD;
    head    = *scratch;
    {
        register u16 vx asm("v0");
        vx                                             = *(u16*)&arg0->workm.t[0];
        ((RoomBillboardScratch*)(head - 0x1C))->vec.vx = vx;
    }
    {
        register u8* tmp asm("v0");
        tmp   = head - 0x1C;
        block = (RoomBillboardScratch*)tmp;
    }
    block->vec.vy = *(u16*)&arg0->workm.t[1];
    vz            = *(u16*)&arg0->workm.t[2];
    *scratch      = block;
    block->vec.vz = vz;

    gte_SetTransMatrix(&GsWSMATRIX);
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&block->vec);
    gte_rtps();
    gte_stsxy(&((RoomBillboardScratch*)(head - 0x1C))->sx);
    gte_stflg(&((RoomBillboardScratch*)(head - 0x1C))->flag);
    if (block->flag >= 0) {
        gte_stszotz(&((RoomBillboardScratch*)(head - 0x1C))->otz);
        USE_REG(head);
        block->otz   += 1;
        block->rOuter = (arg1 * 64) / block->otz;
        block->rInner = (arg1 * 8) / block->otz;

        ang = 0;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            t        = ang + 0x100;
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 12);
            t2       = ang + 0x200;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 12);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0], arg2[1], arg2[2]);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(t)) >> 13);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(t)) >> 13);
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rOuter * rsin(t2)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rOuter * rcos(t2)) >> 13);
            ang      = t2;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);

        ang = 0x200;
        do {
            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            u        = ang - 0x400;
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(ang)) >> 12);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(ang)) >> 12);
            u        = ang + 0x400;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 13);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 13);
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);

            prim           = (POLY_G4*)gGpuPrimCursor;
            gGpuPrimCursor = prim + 1;
            setPolyG4(prim);
            setRGB0(prim, 0, 0, 0);
            setRGB1(prim, 0, 0, 0);
            setRGB2(prim, arg2[0] >> 1, arg2[1] >> 1, arg2[2] >> 1);
            setRGB3(prim, 0, 0, 0);
            prim->x0 = *(u16*)&block->sx + ((block->rInner * rsin(ang)) >> 12);
            prim->y0 = *(u16*)&block->sy + ((block->rInner * rcos(ang)) >> 12);
            prim->x1 = *(u16*)&block->sx + ((block->rOuter * rsin(u)) >> 11);
            prim->y1 = *(u16*)&block->sy + ((block->rOuter * rcos(u)) >> 11);
            u        = ang + 0x800;
            prim->x2 = *(u16*)&block->sx;
            prim->y2 = *(u16*)&block->sy;
            prim->x3 = *(u16*)&block->sx + ((block->rInner * rsin(u)) >> 12);
            prim->y3 = *(u16*)&block->sy + ((block->rInner * rcos(u)) >> 12);
            ang      = u;
            addPrim((u_long*)(((((u32)block->otz << gDisplayState.otDepthShift) >> 2) & 0xFFC) + (s32)gGpuCurrentOt),
                    prim);
            Gp_AddTpageShift((P_TAG*)prim, 1, block->otz);
        } while (ang < 0x1000);
    }
    SCRATCH_POP_BYTES(0x1C);
}

/// Rebuilds collision faces 3-6 of the room's grid as vertical walls, one per
/// entry of `D_mine_mesa_80189A9C`. Each face is a quad whose lower corners are
/// the wall's two base points and whose upper corners sit `height` above them
/// (the grid's y axis points down), with a horizontal normal perpendicular to
/// the base.
void func_mine_mesa_801811C4(s32 height)
{
    SVECTOR*    normals;
    SVECTOR*    verts;
    GpGridFace* faces;
    s16         i;
    s16         face;

    normals = Gp_GridParams->field_4;
    verts   = Gp_GridParams->field_8;
    faces   = Gp_GridParams->field_C;
    for (i = 0; i < 4; i++) {
        face               = i + 3;
        verts[face * 4].vx = verts[face * 4 + 2].vx = D_mine_mesa_80189A9C[i].start.vx;
        verts[face * 4].vy = verts[face * 4 + 2].vy = D_mine_mesa_80189A9C[i].start.vy;
        verts[face * 4].vz = verts[face * 4 + 2].vz = D_mine_mesa_80189A9C[i].start.vz;
        verts[face * 4 + 1].vx = verts[face * 4 + 3].vx = D_mine_mesa_80189A9C[i].end.vx;
        verts[face * 4 + 1].vy = verts[face * 4 + 3].vy = D_mine_mesa_80189A9C[i].end.vy;
        verts[face * 4 + 1].vz = verts[face * 4 + 3].vz = D_mine_mesa_80189A9C[i].end.vz;
        verts[face * 4 + 2].vy                         -= height;
        verts[face * 4 + 3].vy                         -= height;
        faces[face].verts[1]                            = face * 4 + 1;
        faces[face].verts[0]                            = face * 4;
        faces[face].verts[2]                            = face * 4 + 2;
        faces[face].verts[3]                            = face * 4 + 3;
        faces[face].field_A                             = 3;
        faces[face].field_8                             = face;
        normals[face].vx                                = D_mine_mesa_80189A9C[i].start.vz - D_mine_mesa_80189A9C[i].end.vz;
        normals[face].vy                                = 0;
        normals[face].vz                                = D_mine_mesa_80189A9C[i].end.vx - D_mine_mesa_80189A9C[i].start.vx;
        VectorNormalSS(&normals[face], &normals[face]);
    }
}

/// Keeps the room's two enemy slots in `D_mine_mesa_80189B74` filled while the
/// kill counter `D_mine_mesa_80189B6C` is non-zero. An empty slot gets a new
/// enemy placed at one of the spawn points in `D_mine_mesa_80189AFC`, drawn at
/// random from the subset the current view allows, textured from the area's
/// place record; `D_mine_mesa_80189B6E` then delays the next spawn. When one
/// kill remains, nothing spawns until both slots are empty. Once the counter
/// is zero the task hands `Gp_ReleaseStateF0` an empty enemy record and
/// advances its state.
void func_mine_mesa_80181358(Task* arg0)
{
    GpAreaKey            key;
    GpEnemy              result;
    s32                  i;
    s32                  pick;
    u32                  rnd;
    _MineMesaSpawnPoint* table;
    _MineMesaSpawnPoint* pt;
    TmdObject*           tmd;
    GpAreaKey*           loc;
    GpAreaPlace*         place;
    GsCOORDINATE2*       coords;
    GpEnemy*             enemy;

    for (i = 0; i < 2; i++) {
        if (D_mine_mesa_80189B6E > 0) {
            D_mine_mesa_80189B6E--;
            break;
        }
        if (D_mine_mesa_80189B6C == 0) {
            goto end;
        }
        if (D_mine_mesa_80189B6C == 1 &&
            (D_mine_mesa_80189B74[0] != NULL || D_mine_mesa_80189B74[1] != NULL)) {
            break;
        }
        if (D_mine_mesa_80189B74[i] != NULL) {
            continue;
        }
        enemy                   = Gp_SpawnEnemyFromTable(&D_8014D8A4, 0, 0x30002, NULL);
        D_mine_mesa_80189B74[i] = enemy;
        if (enemy == NULL) {
            break;
        }
        enemy->workType                    = 0x900;
        D_mine_mesa_80189B74[i]->placeKey |= i << 12;
        switch (Gp_GetViewIndex() & 0xFF) {
            case 2:
                pick = MINE_MESA_RAND() % 3 + 1;
                break;
            case 3:
                pick = (MINE_MESA_RAND() & 1) | 2;
                break;
            case 4:
                pick = ((MINE_MESA_RAND() & 1) == 0) * 2;
                rnd  = MINE_MESA_RAND() & 1;
                if (rnd == 1) {
                    pick = rnd;
                }
                break;
            case 5:
                pick = MINE_MESA_RAND() & 1;
                break;
            case 8:
                pick = (MINE_MESA_RAND() & 1) | 2;
                break;
            default:
                pick = MINE_MESA_RAND() & 3;
                break;
        }
        table                                                                  = D_mine_mesa_80189AFC;
        pt                                                                     = &table[(s16)pick];
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[0] = pt->x;
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[1] = pt->y;
        ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord.t[2] = pt->z;
        tmd                                                                    = (TmdObject*)D_mine_mesa_80189B74[i]->task->extra;
        loc                                                                    = &gGameSession->at4.loc;
        key.stage                                                              = loc->stage;
        key.area                                                               = loc->area;
        key.room                                                               = loc->room;
        key.view                                                               = gGameSession->at4.loc.view;
        Gp_SyncAreaKeyIndex(&key);
        place      = (GpAreaPlace*)Gp_GetNestedAreaRec(&key)->field_0;
        tmd->tpage = place->tpage;
        tmd->clut  = place->clut;
        if (D_8007218B == 10) {
            printf("tpage=%x, clut=%x, eno=%x\n", (s8)place->tpage, (s8)place->clut, 0);
        }
        if (tmd->buffer != NULL) {
            tmdProcessStream(tmd);
            tmdProcessStream(tmd);
        }
        Gfx_RotMatrixY(&((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords->coord,
                       pt->yaw, 1);
        coords               = ((TmdObject*)D_mine_mesa_80189B74[i]->task->extra)->coords;
        D_mine_mesa_80189B6E = 0x50;
        coords->flg          = 0;
    }
    if (D_mine_mesa_80189B6C > 0) {
        return;
    }
end:
    D_8007217B         = 0;
    arg0->spawnArg2    = &result;
    result.param       = NULL;
    Gp_StateF0.field_6 = 1;
    Gp_ReleaseStateF0(arg0, 0);
    D_80062735 = 1;
    arg0->state++;
}

/// Picks the height the room's terrain updater subtracts from its mesh
/// vertices: 0x7D0 while `gGameSession->at4.loc.place` says the session is in area 1
/// or 7, 0x190 otherwise, then hands that to `func_mine_mesa_801811C4`.
void func_mine_mesa_801817BC(void)
{
    s32 offset;

    if (gGameSession->at4.loc.place == 1 || gGameSession->at4.loc.place == 7) {
        offset = 0x7D0;
    } else {
        offset = 0x190;
    }
    func_mine_mesa_801811C4(offset);
}

/// Message 0x13F4 handler, the only entry of the room's `GpMsgEntry` table
/// `D_mine_mesa_80189B1C`. Once the enemy parked in slot `slot` of
/// `D_mine_mesa_80189B74` is dead (`field_40` is its HP) the slot is emptied and
/// the room's remaining-enemy countdown ticks down. Always consumes the message.
s32 func_mine_mesa_80181800(Task* task, s32 msgId, s32 slot, s32 arg3)
{
    if (D_mine_mesa_80189B74[slot] != NULL && D_mine_mesa_80189B74[slot]->hp <= 0) {
        D_mine_mesa_80189B74[slot] = NULL;
        D_mine_mesa_80189B6C       = (u16)D_mine_mesa_80189B6C - 1;
    }
    return 1;
}

/// Starts the room's slot countdown: seeds `D_mine_mesa_80189B6C` to 10, clears
/// the two slots at `D_mine_mesa_80189B74`, points the task at the room's state
/// descriptor and advances a state. `func_mine_mesa_80181800` later empties a
/// slot and decrements the counter once the thing in it is gone.
void func_mine_mesa_80181848(Task* arg0)
{
    D_mine_mesa_80189B6C    = 10;
    D_mine_mesa_80189B74[1] = 0;
    D_mine_mesa_80189B74[0] = 0;
    arg0->msgTable          = &D_mine_mesa_80189B1C;
    arg0->state++;
}

void func_mine_mesa_80181880(Task* arg0)
{
    arg0->state = arg0->state + 1;
}

/// State handlers of the enemy-wave task `func_mine_mesa_80181894` drives: the
/// set-up tick, the spawner, a step past the wave and `taskKill`.
const TaskFuncTable4 D_mine_mesa_8017D660 = {
    { func_mine_mesa_80181848, func_mine_mesa_80181358, func_mine_mesa_80181880, taskKill },
};

void func_mine_mesa_80181894(Task* task)
{
    TaskFuncTable4 states;

    states = D_mine_mesa_8017D660;
    states.funcs[task->state](task);
}
