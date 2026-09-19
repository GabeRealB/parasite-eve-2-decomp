#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/rooms_shared_8017ecb4.h"

/// Scratch state of the shrine's cap script, stored at `Task::work`
/// (`Mem_Calloc(0x10)` in `func_neo_ark_shrine_8017ECC4`).
typedef struct {
    /* 0x00 */ u8  pad_0[8];
    /* 0x08 */ u16 timer; ///< frames the current script step has run
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  field_F;
} NeoArkShrineScript;

void func_neo_ark_shrine_8017F448(void);

extern void func_80179B14(RoomEventMsg* in, RoomEventMsg* out);

/// Message table installed at `Task::msgTable` by the room task's state 0.
extern GpMsgEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C;

extern TaskDesc D_neo_ark_shrine_80182508;

/// Task spawned in state 0, polled by `Task_PollKill` and cleared in state 1.
extern Task* D_neo_ark_shrine_80186864;

/// Cap event key (`Gp_StartCap`'s third argument) handed to the slot-7 event
/// this room starts, so the event's exit can tell which one it was.
extern s32 D_neo_ark_shrine_80181E74;

/// Per-slot group tables for the shrine's arrangement puzzle: five `s16` order
/// indices per slot, `0xFF` terminated, into `D_neo_ark_shrine_8018686C`.
extern s16 D_neo_ark_shrine_801825EC[][5];

/// Current order index of each of the 16 slots. Read back through a `u16`
/// pointer where the puzzle swaps two of them, which is why those accesses are
/// unsigned while the rest are `s16`.
extern s16 D_neo_ark_shrine_8018686C[16];

/// Steps the currently selected group and returns which kind of step it was.
s16 func_neo_ark_shrine_8017E254();

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

void func_neo_ark_shrine_8017D940(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017D948);

/// Argument-less helper, called by this room's cap script every frame. Declared
/// without a parameter list because this state passes `task` to it: the extra
/// `$a0` set makes the hotspot scan's argument setup *not* a "birthing insn" in
/// sched1's `adjust_priority`, so it is not launched at `LAUNCH_PRIORITY` and
/// the scan block keeps `hs` in the branch delay slot. At entry `$a0` still
/// holds the caller's `task`, so the copy is redundant and the allocator drops
/// it - the emitted code is the same either way.
void func_neo_ark_shrine_8017EAC0();

extern RoomHotspot D_neo_ark_shrine_80182430[];

/// Idle state of the shrine's cap script: the hotspot the cursor sits on is
/// confirm-tested (`buttons[0].state == 2`) and its `id` / `promptKind` are
/// latched into the script state, with the 3-vs-6 split decided by hotspot id
/// 0x10 and the script's own `field_F`. The scan walks the hotspot table the
/// shared `RoomsShared8017ecb4` just marked, and `buttons[1].state == 2` leaves
/// the scan by advancing the task to state 5.
///
/// Both oddities below are allocator levers, not logic. The `do { } while (0)`
/// around the last state store folds away, but flow counts the reference at
/// loop depth 2, which is what lifts the parameter above the hotspot pointer in
/// global-alloc's rank; without it the two swap `$s2`/`$s4`. Passing `task` to
/// the helper above is the sched1 counterpart.
void func_neo_ark_shrine_8017D9A0(Task* task)
{
    RoomHotspot*        hs     = D_neo_ark_shrine_80182430;
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
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
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

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80180294States);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80181e70Table);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D620);
