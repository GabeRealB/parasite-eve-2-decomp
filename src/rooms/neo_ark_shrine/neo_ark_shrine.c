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

/// Scratch state of the shrine's cap script, stored at `Task::idMap`
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

/// Message table installed at `Task::field_24` by the room task's state 0.
extern GpMsgEntry D_neo_ark_shrine_80181E34[];

extern TaskDesc D_neo_ark_shrine_80181E5C;

extern TaskDesc D_neo_ark_shrine_80182508;

/// Task spawned in state 0, polled by `Task_PollKill` and cleared in state 1.
extern Task* D_neo_ark_shrine_80186864;

/// Cap event key (`Gp_StartCap`'s third argument) handed to the slot-7 event
/// this room starts, so the event's exit can tell which one it was.
extern s32 D_neo_ark_shrine_80181E74;

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
                Task_Kill(task);
            }
            return;
    }
}

void func_neo_ark_shrine_8017D8F4(Task* task)
{
    task->field_24 = D_neo_ark_shrine_80181E34;
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
    NeoArkShrineScript* st     = (NeoArkShrineScript*)task->idMap;
    u16                 id;

    func_neo_ark_shrine_8017EAC0(task);
    Game_Session->field_68 = 1;
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

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", func_neo_ark_shrine_8017DB10);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80180294States);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", RoomsShared80181e70Table);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine", D_neo_ark_shrine_8017D620);
