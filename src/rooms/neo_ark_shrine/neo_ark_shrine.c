#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/abs.h>

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/neo_ark_shrine.h"
#include "rooms/room_common.h"

extern u8 D_8007216D;

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Message table installed at `Task::msgTable` by the room task's state 0.
extern GpMsgEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C;

/// Task spawned in state 0, polled by `Task_PollKill` and cleared in state 1.
extern Task* D_neo_ark_shrine_80186864;

/// Cap event key (`Gp_StartCap`'s third argument) handed to the slot-7 event
/// this room starts, so the event's exit can tell which one it was.
extern s32 D_neo_ark_shrine_80181E74;

/// Per-slot group tables for the shrine's arrangement puzzle: five `s16` order
/// indices per slot, `0xFF` terminated, into `D_neo_ark_shrine_8018686C`.
extern s16 D_neo_ark_shrine_801825EC[][5];

extern NeoArkShrineSlot D_neo_ark_shrine_8018252C[16];
extern NeoArkShrineSlot D_neo_ark_shrine_801825AC[16];
extern NeoArkShrineSlot D_neo_ark_shrine_801868CC[16];

/// Steps the currently selected group and returns which kind of step it was.
s16 func_neo_ark_shrine_8017E254(void);

/// Always returns 0.
s32 func_neo_ark_shrine_8017D6A4(void)
{
    return 0;
}

s32 func_neo_ark_shrine_8017D6AC(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    *out = *in;
    func_80179B14(in, out);
    if (in->msgId != 0x11) {
        return 1;
    }
    if (GameFlag_GetNibble(0xDB) != 0) {
        return 1;
    }
    if (in->field_5 != 0) {
        return 0;
    }
    Gp_SetNibbleIf(in->field_6, 2);
    Gp_RunCapCmd1(4);
    return 0;
}

s32 func_neo_ark_shrine_8017D740(s32 arg0, s32 arg1, s32 arg2)
{
    s32 bit2;

    if (arg2 == 7) {
        bit2 = Gp_GetCurBit2Flag(7);
        if (bit2 == 1) {
            Gp_StartCapSlot(7, 1, (s16)D_neo_ark_shrine_80181E74);
            if (D_neo_ark_shrine_80181E74 == 2) {
                D_neo_ark_shrine_80181E74 = bit2;
            }
        } else {
            Gp_StartCapSlot(7, 1, 0);
        }
    }
    if (arg2 == 5) {
        Gp_RunCapCmd1(GameFlag_GetNibble(0xDE) == 0 ? 5 : 0xC);
    }
    return 0;
}

s32 func_neo_ark_shrine_8017D7F0(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 1) {
        if (GameFlag_GetNibble(0xDF) == 0) {
            Task_SpawnFromTable(&D_neo_ark_shrine_80181E5C, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(9);
        }
    }
    return 0;
}

void func_neo_ark_shrine_8017D84C(Task* task)
{
    s32 sp10;

    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_MsgPlayer3F3(0);
            D_neo_ark_shrine_80186864 = Task_SpawnFromTable(&D_neo_ark_shrine_80182508, 0, 0, 0);
            task->state++;
            return;
        case 1:
            if (Task_PollKill(D_neo_ark_shrine_80186864, &sp10) != 0) {
                D_neo_ark_shrine_80186864 = NULL;
                taskKill(task);
            }
            return;
    }
}

void func_neo_ark_shrine_8017D8F4(Task* task)
{
    task->msgTable = D_neo_ark_shrine_80181E34;
    Game_SetPtrSlot(task, 7);
    func_neo_ark_shrine_8017F448();
    task->state++;
}

/// Second state of the room task: nothing left to do but idle.
void func_neo_ark_shrine_8017D940(Task* task)
{
}

/// State handlers of the room task `func_neo_ark_shrine_8017D948`, indexed by
/// `Task::state`: the set-up tick, the idle tick, and `taskKill`.
const TaskFuncTable3 D_neo_ark_shrine_8017D5C4 = {
    { func_neo_ark_shrine_8017D8F4, func_neo_ark_shrine_8017D940, taskKill },
};

/// Room task: runs the state handler `D_neo_ark_shrine_8017D5C4` names for
/// `Task::state`, through a copy of the table taken onto the stack.
void func_neo_ark_shrine_8017D948(Task* task)
{
    TaskFuncTable3 sp;

    sp = D_neo_ark_shrine_8017D5C4;
    sp.funcs[task->state](task);
}

/// Idle state of the shrine's cap script: the hotspot the cursor sits on is
/// confirm-tested (`buttons[0].state == 2`) and its `id` / `promptKind` are
/// latched into the script state, with the 3-vs-6 split decided by hotspot id
/// 0x10 and the script's own `field_F`. The scan walks the hotspot table the
/// hit test `func_neo_ark_shrine_8017EC10` just marked, and
/// `buttons[1].state == 2` leaves the scan by advancing the task to state 5.
///
/// Both oddities below are allocator levers, not logic. The `do { } while (0)`
/// around the last state store folds away, but flow counts the reference at
/// loop depth 2, which is what lifts the parameter above the hotspot pointer in
/// global-alloc's rank; without it the two swap `$s2`/`$s4`. Passing `task` to
/// the per-frame helper is the sched1 counterpart: the extra `$a0` set makes
/// the hotspot scan's argument setup *not* a "birthing insn" in sched1's
/// `adjust_priority`, so it is not launched at `LAUNCH_PRIORITY` and the scan
/// block keeps `hs` in the branch delay slot. At entry `$a0` still holds
/// `task`, so the copy itself is dropped by the allocator.
void func_neo_ark_shrine_8017D9A0(Task* task)
{
    OverlayHotspot*     hs     = D_neo_ark_shrine_80182430;
    RoomActionPrompt*   prompt = &D_80114D28;
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->work;
    u16                 id;

    func_neo_ark_shrine_8017EAC0(task);
    gGameSession->hideHud = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (func_neo_ark_shrine_8017EC10(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if (prompt->buttons[0].state == 2) {
            id = hs->id;
            if (hs->id != -1) {
                do {
                    if (hs->hit != 0) {
                        if ((s16)id == 0x10 || st->field_F == 0) {
                            prompt->mode     = 0;
                            prompt->targetId = 0;
                            st->field_C      = hs->id;
                            st->field_E      = hs->promptKind;
                            task->state      = 3;
                            return;
                        }
                        st->field_C = id;
                        st->field_E = hs->promptKind;
                        task->state = 6;
                        return;
                    }
                    hs++;
                    id = hs->id;
                } while (hs->id != -1);
            }
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        do {
            task->state = 5;
        } while (0);
    }
}

/// Runs one step of the shrine's arrangement puzzle: for each of the five
/// entries of the group the current slot selects, it rotates the entry's index
/// to the front of `D_neo_ark_shrine_8018686C` when that index is still unused,
/// and plays a click for each move. The task then advances to state 2 and, if
/// anything moved, hands the step the group's helper reports to the cap
/// script - state 9 for a completed set, 7 / 0xE for the two sound-only steps,
/// and the flag-0xDB branch that starts the cap slot for the last one.
///
/// Three shapes here are load-bearing, not style:
///
/// - The walk is a `for` loop rather than the `do { } while` splat's `goto`
///   form compiles to. Only the front end's `NOTE_INSN_LOOP_BEG` marks make
///   loop.c run, and it is what hoists the two table addresses into the
///   preheader; the `goto` form leaves both `lui/addiu` pairs re-materialized
///   inside the loop.
/// - The tables are indexed as arrays (`D_...[i]`), which is what puts the
///   scaled index on the left of the address sum and adds the base last.
///   Reaching them through a pointer local instead flips both adds.
/// - `swapped` is a `u8`, and it is tested as an assignment inside the
///   condition. Narrow, its 0/1 stores are recorded by reload's CSE in QImode,
///   so they are not substituted for the `SImode` constant 0 of `i = 0` or the
///   shift amount of `state * 2`; and the `u8` store and the `zero_extend` the
///   test needs sit in one statement, which is close enough for combine to fold
///   the pair into a plain copy of the flag. Widen `swapped` or split the test
///   from the assignment and one of those three spots stops matching.
void func_neo_ark_shrine_8017DB10(Task* arg0)
{
    s16                 temp_v0;
    s16                 state;
    s16                 slot;
    s32                 i;
    u8                  swapped;
    u8                  moved;
    u16*                ord;
    u16                 prev;
    NeoArkShrineScript* st;

    st      = (NeoArkShrineScript*)arg0->work;
    swapped = 0;
    func_neo_ark_shrine_8017EAC0();
    for (i = 0; i < 5; i++) {
        state = D_neo_ark_shrine_801825EC[st->field_C][i];
        if (state == 0xFF) {
            break;
        }
        if (D_neo_ark_shrine_8018686C[state] == 0) {
            SndEvt_EnqueueType6(0x55150006, 0, 0);
            slot                                                                 = st->field_C;
            ord                                                                  = (u16*)&D_neo_ark_shrine_8018686C[slot];
            prev                                                                 = *ord;
            *ord                                                                 = D_neo_ark_shrine_8018686C[D_neo_ark_shrine_801825EC[slot][i]];
            swapped                                                              = 1;
            D_neo_ark_shrine_8018686C[D_neo_ark_shrine_801825EC[st->field_C][i]] = prev;
        }
    }
    arg0->state = 2;
    if ((moved = swapped != 0)) {
        temp_v0 = func_neo_ark_shrine_8017E254();
        switch (temp_v0) {
            case 1:
                if (GameFlag_GetNibble(0xDB) == 0) {
                    SndEvt_EnqueueType6(0x55150008, 0, 0);
                    GameFlag_SetNibble(0xDB, 1);
                    GameFlag_SetNibble(0x1B8, 0);
                    Gp_StartCapSlot(3, 0, 0);
                    return;
                }
                break;
            case 2:
                arg0->state = 9;
                break;
            case 3:
                SndEvt_EnqueueType6(0x55150007, 0, 0);
                arg0->state = 7;
                break;
            case 4:
                SndEvt_EnqueueType6(0x55150007, 0, 0);
                arg0->state = 0xE;
                break;
        }
    }
}

const TaskFuncTable16 D_neo_ark_shrine_8017D5D0 = {
    {
        func_neo_ark_shrine_8017ECC4,
        func_neo_ark_shrine_8017EDAC,
        func_neo_ark_shrine_8017D9A0,
        func_neo_ark_shrine_8017EDE0,
        func_neo_ark_shrine_8017EE44,
        func_neo_ark_shrine_8017EED4,
        func_neo_ark_shrine_8017DB10,
        func_neo_ark_shrine_8017EF68,
        func_neo_ark_shrine_8017EFE4,
        func_neo_ark_shrine_8017F094,
        func_neo_ark_shrine_8017F0F0,
        func_neo_ark_shrine_8017F178,
        func_neo_ark_shrine_8017F21C,
        func_neo_ark_shrine_8017F274,
        func_neo_ark_shrine_8017F320,
        func_neo_ark_shrine_8017F398,
    },
};

const TaskFuncTable4 D_neo_ark_shrine_8017D610 = {
    { func_neo_ark_shrine_8017F4C8, func_neo_ark_shrine_8017F578, func_neo_ark_shrine_8017F640, taskKill },
};

const TaskFuncTable3 D_neo_ark_shrine_8017D620 = {
    { func_neo_ark_shrine_8017F688, func_neo_ark_shrine_8017F738, taskKill },
};

/// Alignment pad closing this unit's `.rodata`: the next object starts with a
/// jump table, which the original build aligned to 8. Nothing reads it.
const u32 D_neo_ark_shrine_8017D62C = 0;

/// Outlines `rect` on screen in (`r`, `g`, `b`) with four unconnected flat
/// `LINE_F2`s -- top, right, bottom and left edge of the rectangle spanning
/// (`x`, `y`) to (`x + w`, `y + h`) -- each linked into `gGpuCurrentOt[1]`.
void func_neo_ark_shrine_8017DD38(RoomRect* rect, u8 r, u8 g, u8 b)
{
    LINE_F2* line;

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y;
    line->x1 = rect->x + rect->w;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x + rect->w;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y + rect->h;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);

    line           = (LINE_F2*)gGpuPrimCursor;
    gGpuPrimCursor = line + 1;
    setLineF2(line);
    line->x0 = rect->x;
    line->y0 = rect->y + rect->h;
    line->x1 = rect->x;
    line->y1 = rect->y;
    line->r0 = r;
    line->g0 = g;
    line->b0 = b;
    addPrim(gGpuCurrentOt + 1, line);
}

/// Animates and draws the shrine's sliding-tile puzzle. Each tile's target
/// position is taken from the board position it now occupies; its drawn
/// position eases halfway there every frame and snaps once both axes are
/// within four units. Every tile but tile 0, the gap, is then drawn as a 32x32
/// textured quad.
void func_neo_ark_shrine_8017DF7C(void)
{
    s32               i;
    s32               tile;
    NeoArkShrineSlot* cur;
    NeoArkShrineSlot* tgt;
    POLY_FT4*         prim;

    for (i = 0; i < 16; i++) {
        tile                              = D_neo_ark_shrine_8018686C[i];
        D_neo_ark_shrine_801868CC[tile].x = D_neo_ark_shrine_8018252C[i].x;
        D_neo_ark_shrine_801868CC[tile].y = D_neo_ark_shrine_8018252C[i].y;
    }

    for (i = 0; i < 16; i++) {
        tile    = D_neo_ark_shrine_8018686C[i];
        cur     = &D_neo_ark_shrine_8018688C[tile];
        tgt     = &D_neo_ark_shrine_801868CC[tile];
        cur->x += ((s16)tgt->x - (s16)cur->x) >> 1;
        cur->y += ((s16)tgt->y - (s16)cur->y) >> 1;
        if (ABS((s16)cur->x - (s16)tgt->x) < 4 &&
            ABS((s16)D_neo_ark_shrine_8018688C[tile].y - (s16)D_neo_ark_shrine_801868CC[tile].y) < 4) {
            D_neo_ark_shrine_8018688C[tile].x = D_neo_ark_shrine_801868CC[tile].x;
            D_neo_ark_shrine_8018688C[tile].y = D_neo_ark_shrine_801868CC[tile].y;
        }
        if (tile != 0) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setPolyFT4(prim);
            setUVWH(prim, D_neo_ark_shrine_801825AC[tile].x, D_neo_ark_shrine_801825AC[tile].y, 0x20, 0x20);
            prim->tpage = 0x8D;
            prim->clut  = 0x3FC0;
            setShadeTex(prim, 1);
            setXYWH(prim, D_neo_ark_shrine_8018688C[tile].x, D_neo_ark_shrine_8018688C[tile].y, 0x20, 0x20);
            addPrim(&gGpuCurrentOt[10], prim);
        }
    }
}

s16 func_neo_ark_shrine_8017E254(void)
{
    s32 flag;

    if (D_neo_ark_shrine_8018686C[0] == 9 && D_neo_ark_shrine_8018686C[1] == 10 &&
        D_neo_ark_shrine_8018686C[2] == 11 && D_neo_ark_shrine_8018686C[3] == 12 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_8018686C[0] == 12 && D_neo_ark_shrine_8018686C[1] == 11 &&
        D_neo_ark_shrine_8018686C[2] == 10 && D_neo_ark_shrine_8018686C[3] == 9 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_80186868 == 1) {
        return 4;
    }
    if (D_neo_ark_shrine_8018686C[3] == 1 && D_neo_ark_shrine_8018686C[6] == 2 &&
        D_neo_ark_shrine_8018686C[9] == 3 && D_neo_ark_shrine_8018686C[12] == 4 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    if (D_neo_ark_shrine_8018686C[3] == 4 && D_neo_ark_shrine_8018686C[6] == 3 &&
        D_neo_ark_shrine_8018686C[9] == 2 && D_neo_ark_shrine_8018686C[12] == 1 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    flag = D_neo_ark_shrine_8018686A;
    if (flag == 1) {
        D_neo_ark_shrine_8018686A = 0;
        if (GameFlag_GetNibble(0xE9) == 0) {
            D_8007216D                 = flag;
            gGameSession->at4.loc.room = flag;
        } else {
            D_8007216D                 = 4;
            gGameSession->at4.loc.room = 4;
        }
        gGameSession->roomObjsDirty = 1;
        SndEvt_EnqueueType6(0x5515000A, 0, 0);
        Gp_SpawnPadLerp(0x28, 0x30, 0x60);
    }
    if (D_neo_ark_shrine_8018686C[0] == 5 && D_neo_ark_shrine_8018686C[4] == 6 &&
        D_neo_ark_shrine_8018686C[8] == 7 && D_neo_ark_shrine_8018686C[12] == 8 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    if (D_neo_ark_shrine_8018686C[0] == 8 && D_neo_ark_shrine_8018686C[4] == 7 &&
        D_neo_ark_shrine_8018686C[8] == 6 && D_neo_ark_shrine_8018686C[12] == 5 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    return 0;
}
