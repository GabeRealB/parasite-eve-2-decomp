#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/display.h"
#include "main/gameflag.h"
#include "main/gfx.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_sanctuary.h"
#include "rooms/room_common.h"

#include <psyq/inline_c.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// `rtps` / `rtpt` / `mvmva` / `gpf`. The `inline_c.h` macros of those names
/// assemble to different words, so spell the instructions out.
#define gte_rtps_real()  __asm__ volatile("nop; nop; .word 0x4A180001")
#define gte_rtpt_real()  __asm__ volatile("nop; nop; .word 0x4A280030")
#define gte_rtv0_real()  __asm__ volatile("nop; nop; .word 0x4A486012")
#define gte_gpf12_real() __asm__ volatile("nop; nop; .word 0x4B98003D")
/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// equipped-weapon index the slot-3 msg 0x3E8 record is keyed on,
/// `D_80071075` and `D_80114C12` gate the cutscene task's setup (the latter is
/// the cutscene/among-us mode flag) and `D_8007218A` picks which of the two
/// weapon-id bases that record uses. `D_80071076` is set to 1 alongside the
/// save writes when the task hands off to task 0x11, the same way the fountain
/// and helicopter-pad rooms set it.
extern u8              D_80073BA9;
extern u8              D_80071075;
extern s16             D_80071076;
extern s8              D_8007218A;
extern s8              D_80114C12;
extern SVECTOR         D_acropolis_sanctuary_8017D5D0;
extern AcsSpriteLevels D_acropolis_sanctuary_8017D5D8;
extern AcsSpriteLevels D_acropolis_sanctuary_8017D5DC;
extern GpMsgEntry      D_acropolis_sanctuary_8018081C[];
extern RoomPlacement   D_acropolis_sanctuary_801808BC;
extern GpRec14         D_acropolis_sanctuary_801809F8;
extern GpRec14         D_acropolis_sanctuary_80180A0C;
extern s32             D_acropolis_sanctuary_80180AE8;
extern u8              D_acropolis_sanctuary_80181814[];
extern TaskDesc        D_acropolis_sanctuary_80182240;
extern AcsBlockerSet   D_acropolis_sanctuary_801822EC;
extern GpMsgEntry      D_acropolis_sanctuary_80182310[];
extern AcsTile         D_acropolis_sanctuary_80182320[];
extern AcsQuad         D_acropolis_sanctuary_80182710[];
extern s16             D_acropolis_sanctuary_80182750[];
extern s32             D_acropolis_sanctuary_80182770;
extern SVECTOR         D_acropolis_sanctuary_80182774[];
extern u16             D_acropolis_sanctuary_801827D4[];
extern AcsBlockerSet   D_acropolis_sanctuary_80183568;
extern Task*           D_acropolis_sanctuary_80186C90;
/// Gameplay's LCG seed; it has no module header yet.
extern s32 Gp_LcgState;
/// Payloads the sanctuary cutscene task sends: `..._801820E4` is the record
/// slot-3 msg 0x3F4 takes and `..._801820F0` / `..._801821C8` the script pair
/// `func_800E8634` is started on.
extern s32  D_acropolis_sanctuary_801820E4;
extern s32  D_acropolis_sanctuary_801820F0;
extern s32  D_acropolis_sanctuary_801821C8;
extern void func_acropolis_sanctuary_8017DD78(void);
extern void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1);

s32 func_acropolis_sanctuary_8017D810(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 0 && GameFlag_GetNibble(6) == 0) {
        func_800E8614((s32)&D_acropolis_sanctuary_80181814, 0);
    }
    return 0;
}

/// Message gate for the sanctuary hotspot registered under id 0x13EF: sub-id 1
/// arms the room's own task the first time it is seen, latching nibble 7 so a
/// second visit does nothing. The record is not copied to the outgoing one -
/// this handler only ever consumes the message (returns 0).
s32 func_acropolis_sanctuary_8017D848(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    if (in->field_2 == 1 && GameFlag_GetNibble(7) == 0) {
        GameFlag_SetNibble(7, 1);
        Task_SpawnFromTable(&D_acropolis_sanctuary_80182240, 0, 0, 0);
    }
    return 0;
}

void func_acropolis_sanctuary_8017D8A0(u32 arg0)
{
    func_acropolis_sanctuary_8017DF88((arg0 >> 8) & 0xFF, arg0 & 0xFF);
}

/// Republishes the player's weapon to slot 3: picks the room's 0x3E8 record by
/// the equipped-weapon index in `D_80073BA9`, has `Gp_PlayerWeaponId` stamp the
/// current weapon model id into its `field_0`, then sends it.
void func_acropolis_sanctuary_8017D8CC(void)
{
    if (D_80073BA9 == 2) {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_801809F8.field_0);
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&D_acropolis_sanctuary_801809F8, 0);
    } else {
        Gp_PlayerWeaponId(&D_acropolis_sanctuary_80180A0C.field_0);
        Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&D_acropolis_sanctuary_80180A0C, 0);
    }
}

/// State 0 of the sanctuary room task: publishes the room's message-handler
/// table under pointer slot 7 and advances to the next state. Unless nibble 6
/// has already reached 1 it also chains slot-4 message list 1 onto itself and,
/// when nibble 2 is set and that slot holds a task, places the actor by sending
/// it the 0x7D3 animation record followed by the 0x7D4 placement.
void func_acropolis_sanctuary_8017D930(Task* arg0)
{
    Task* slot;

    arg0->field_24 = D_acropolis_sanctuary_8018081C;
    Game_SetPtrSlot(arg0, 7);
    arg0->state = arg0->state + 1;
    if (GameFlag_GetNibble(6) != 1) {
        slot = (Task*)Gp_LookupSlot4(1);
        Gp_MsgSlot4Chain(1, 1);
        if (GameFlag_GetNibble(2) != 0 && slot != NULL) {
            Gp_DispatchMsg(slot, 0x7D3, (s32)&D_acropolis_sanctuary_80180AE8, 0);
            Gp_DispatchMsg(slot, 0x7D4, (s32)&D_acropolis_sanctuary_801808BC, 0);
        }
    }
    func_acropolis_sanctuary_8017DD78();
}
