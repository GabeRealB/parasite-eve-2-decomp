#include "common.h"

#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/rooms_shared_8017ecb4.h"

/// Main-executable globals with no module header yet: `D_8007216C` is the
/// 1-based index of the area record the room is showing, the value
/// `Gp_FindViewIndex` returns and the view gate reads back next to
/// `GameSession.field_52`; `D_80073BA9` is the equipped-weapon index the slot-3
/// msg 0x3E8 record is keyed on; and `D_8007218A` picks which of the two
/// weapon-id bases that record uses. `D_80071075` gates the "everything is
/// dead" message and `D_80114C12` the cutscene/among-us mode flag: the second
/// arming state machine below waits for both to be clear.
extern s8 D_8007216C;
extern s8 D_8007218A;
extern u8 D_80073BA9;
extern u8 D_80071075;
extern s8 D_80114C12;

/// The one state machine that arms the breezeway, switched on the room task's
/// `DbwWork.field_C`:
///
/// * 0 does nothing but clear the state (the `case 0: break;` the switch needs
///   to build its dispatch tree -- reaching the tail is what clears it).
/// * 1 sends the scene's opening sequence: the session's two id bytes as the
///   0x7DA prompt payload with the 1 the receiver reads as "armed", the room's
///   reset placement (`D_dryfield_breezeway_80181E28`) with 0x7D4, and the two
///   `D_dryfield_breezeway_80181E40` placements -- the message-0x3E9 that moves
///   the player and the 0x3EE that takes the run's third record -- to the slot-3
///   game task before publishing view 4's area-record index.
/// * 2 republishes the player's weapon as slot-3 msg 0x3E8, the same record
///   `Gp_MsgPlayerWeapon` builds: this room's `field_4` 9, `field_8` 1, a
///   `field_C` of 0xA and everything else zeroed.
///
/// Anything else (`field_C` above 2) clears the state and returns, which is how
/// a finished arm retires. The pointer into the record is what makes the middle
/// three field stores go through `$a1` rather than the frame pointer: taking the
/// address as a value first lets CSE rewrite them as base+offset, the same
/// allocation the original compiler reached.
void func_dryfield_breezeway_8017DEC0(Task* arg0)
{
    DbwMsg7DA msg;
    DbwMsgBuf buf;
    GpRec14*  rec;
    DbwWork*  work;
    s32       state;
    s32       id;

    work  = (DbwWork*)arg0->idMap;
    state = work->field_C;

    switch (state) {
        default:
            work->field_C = 0;
            return;
        case 0:
            break;
        case 1:
            msg.field_0 = Game_Session->field_7;
            msg.field_1 = Game_Session->field_6;
            msg.field_2 = 1;
            Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
            Gp_DispatchMsg(work->field_4, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
            Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_dryfield_breezeway_80181E40[0], 0);
            Gp_DispatchMsg(work->field_0, 0x3EE, (s32)&D_dryfield_breezeway_80181E40[1], 0);
            D_8007216C = Gp_FindViewIndex(4);
            break;
        case 2:
            rec              = &buf.rec;
            id               = D_80073BA9;
            buf.rec.field_0  = (D_8007218A == 1) ? id + 1 : id + 0x22;
            rec->field_4     = 9;
            rec->field_8     = 1;
            rec->field_C     = 0xA;
            buf.rec.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&buf, 0);
            break;
    }
    work->field_C = 0;
}

void func_dryfield_breezeway_8017E010(Task* arg0)
{
    DbwWork* work;
    s32      id;

    switch (arg0->state) {
        case 0:
            work        = (DbwWork*)Mem_Malloc(0x14, 0);
            arg0->idMap = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(work, 0, 0x14);
                work->field_0                 = (void*)Game_GetPtrSlot(3);
                D_dryfield_breezeway_801843C0 = arg0;
                id                            = Game_Session->field_6 | (Game_Session->field_7 << 8);
                work->field_4                 = (void*)Gp_FindWorkById(id)->field_0;
                id                            = ((Game_Session->field_7 << 8) | 0x1000) | Game_Session->field_6;
                work->field_8                 = (void*)Gp_FindWorkById(id)->field_0;
            }
            arg0->state += 1;
            return;
        case 1:
            Task_Kill(arg0);
            return;
    }
}

/// The pair of cutscene blocks `func_800E8634` hands to `Task_Spawn` (bank 9,
/// type 7): the table the spawned task starts from and the event-command
/// stream it parks in `D_801156D0` for the task that follows it. Both live in
/// the room's trailing data blob.
extern s32 D_dryfield_breezeway_80181E70;
extern s32 D_dryfield_breezeway_80181F90;

/// The long-lived half of the arming pair: `func_dryfield_breezeway_8017E010`
/// is the same state 0 with no sequencer and no cutscene behind it, and is the
/// one `dryfield_night_water_tank` spawns. This one arms the room and then
/// stays resident to run `func_dryfield_breezeway_8017DEC0` every frame.
///
/// State 0 arms the room, but only while no cutscene is running
/// (`D_80114C12 != 1`) and the area is not cleared (`D_80071075 == 0`) --
/// otherwise it returns having done nothing, which retires the task on the
/// next frame. It allocates the 0x14 `DbwWork` block, publishes the room task
/// in `D_dryfield_breezeway_801843C0`, republishes the player's weapon as
/// slot-3 msg 0x3E8 (`GpRec14`, the record `Gp_MsgPlayerWeapon` also builds:
/// `field_0` off the equipped-weapon index in `D_80073BA9`, `field_4` and
/// `field_8` both 1, `field_C` 0xA and `field_10` zero) and starts the room's
/// opening cutscene through `func_800E8634`, which is what raises
/// `Game_Session::field_1`. It then advances to state 1.
///
/// State 1 runs the sequencer every frame until the cutscene clears
/// `Game_Session::field_1`, at which point the task kills itself. Any other
/// state goes straight to the sequencer.
void func_dryfield_breezeway_8017E114(Task* arg0)
{
    GpRec14  buf;
    DbwWork* work;
    s32      id;

    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                return;
            }
            work        = (DbwWork*)Mem_Malloc(0x14, 0);
            arg0->idMap = (TaskIdMap*)work;
            if (work == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(work, 0, 0x14);
                work->field_0                 = (void*)Game_GetPtrSlot(3);
                D_dryfield_breezeway_801843C0 = arg0;
                id                            = Game_Session->field_6 | (Game_Session->field_7 << 8);
                work->field_4                 = (void*)Gp_FindWorkById(id)->field_0;
                id                            = ((Game_Session->field_7 << 8) | 0x1000) | Game_Session->field_6;
                work->field_8                 = (void*)Gp_FindWorkById(id)->field_0;
            }
            id           = D_80073BA9;
            buf.field_0  = (D_8007218A == 1) ? id + 1 : id + 0x22;
            buf.field_4  = 1;
            buf.field_8  = 1;
            buf.field_C  = 0xA;
            buf.field_10 = 0;
            Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&buf, 0);
            func_800E8634((s32)&D_dryfield_breezeway_80181E70, 0, (s32)&D_dryfield_breezeway_80181F90);
            arg0->state += 1;
            break;
        case 1:
            if (Game_Session->field_1 == 0) {
                Task_Kill(arg0);
                return;
            }
            break;
    }
    func_dryfield_breezeway_8017DEC0(arg0);
}

void func_dryfield_breezeway_8017E2D4(void)
{
    DbwWork*  work;
    DbwMsg7DA msg;

    work        = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    msg.field_0 = Game_Session->field_7;
    msg.field_1 = Game_Session->field_6;
    msg.field_2 = 2;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
}

void func_dryfield_breezeway_8017E350(void)
{
    Gp_ArmStateF0(1);
}

void func_dryfield_breezeway_8017E370(s16 arg0)
{
    DbwWork* work;

    work          = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    work->field_C = arg0;
    work->field_E = 0;
}

void func_dryfield_breezeway_8017E390(void)
{
    DbwMsgBuf buf;
    DbwWork*  work;
    s32       id;

    id               = D_80073BA9;
    buf.rec.field_0  = (D_8007218A == 1) ? id + 1 : id + 0x22;
    buf.rec.field_4  = 9;
    buf.rec.field_8  = 0;
    buf.rec.field_C  = 0;
    buf.rec.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&buf, 0);

    work            = (DbwWork*)D_dryfield_breezeway_801843C0->idMap;
    buf.msg.field_0 = Game_Session->field_7;
    buf.msg.field_1 = Game_Session->field_6;
    buf.msg.field_2 = 2;
    Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&buf, 0x7DB);
    Gp_DispatchMsg(work->field_8, 0x7D4, (s32)&D_dryfield_breezeway_80181E28, 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_breezeway/dryfield_breezeway_2", func_dryfield_breezeway_8017E464);

/// Main-executable symbols with no module header yet: `D_80070F70` is the
/// frame counter the prop's swing angle is derived from, and `func_8004BFF8`
/// is the Y rotation builder `ActorsShared80139948` also reaches.
///
/// Its `angle` parameter is declared `s32` rather than the `s16` the actor
/// headers use because the calls below feed it `rsin`'s `int` result, which
/// the target passes through untruncated.
extern s32 D_80070F70;
void       func_8004BFF8(s32 angle, MATRIX* matrix);

/// The two image records the key-item prompt's scan uploads the first time it
/// runs, taken from the room's trailing data blob: the confirm and cancel
/// artwork `Gp_LoadImages` stages into VRAM.
extern GpImgRec D_dryfield_breezeway_80182F24;
extern GpImgRec D_dryfield_breezeway_80183144;

/// Runs the key-item prompt's scan state: uploads this room's two prompt
/// `GpImgRec`s the first time it runs (`Task::killCountdown` is zero, and the
/// increment latches it so a later frame never reloads them), rebuilds the
/// event task's display object matrix as the same pure Y rotation of
/// `rsin(D_80070F70 * 16)` the prop's swing builds -- one full turn every 256
/// frames -- and re-seeds `func_dryfield_breezeway_8017EB8C` at the reset
/// position (0, 0x20) rather than at the cursor the prop's scan passes.
///
/// Highlighting the cursor (`mode` 1) is the state the scan runs in; landing on
/// `D_dryfield_breezeway_80182E00` -- the key-item prompt's own one-entry table,
/// where `func_dryfield_breezeway_8017E81C` reaches the two-entry prop table --
/// confirms it (`mode` 2) and walks that table for the entry whose `hit` is
/// raised. The entry's `id` and `promptKind` go to the event work block
/// (`DbwEventWork.field_4C` / `promptKind`), which
/// `func_dryfield_breezeway_8017FD9C` re-spawns the prompt from, and the task
/// advances to state 3. A cancel press (`buttons[1].state` 2) ends the script
/// in state 5, and a busy cap abandons the scan with the prompt cleared.
void func_dryfield_breezeway_8017E65C(Task* task)
{
    DbwEventWork*     work;
    RoomHotspot*      hs;
    RoomActionPrompt* prompt;
    GsCOORDINATE2*    coord;
    MATRIX*           m;

    coord  = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    work   = (DbwEventWork*)task->idMap;
    hs     = D_dryfield_breezeway_80182E00;
    prompt = &D_80114D28;

    if (task->killCountdown == 0) {
        Gp_LoadImages(&D_dryfield_breezeway_80182F24);
        Gp_LoadImages(&D_dryfield_breezeway_80183144);
        task->killCountdown = (u16)task->killCountdown + 1;
    }

    m                  = &coord->coord;
    *(s32*)&m->m[0][0] = 0x1000;
    *(s32*)&m->m[1][1] = 0x1000;
    *(s16*)&m->m[2][2] = 0x1000;
    *(s32*)&m->m[0][2] = 0;
    *(s32*)&m->m[2][0] = 0;

    func_8004BFF8(rsin(D_80070F70 * 0x10), m);
    coord->flg = 0;
    func_dryfield_breezeway_8017EB8C(task, 0, 0x20);
    Game_Session->field_68 = 1;
    Game_Session->field_1  = 1;
    if (Gp_CapBusy() != 0) {
        prompt->mode     = 0;
        prompt->targetId = 0;
        return;
    }
    prompt->targetId = 0x80;
    if (RoomsShared8017ecb4(hs, prompt->screen.xy.x, prompt->screen.xy.y) != 0) {
        prompt->mode = 2;
        if ((prompt->buttons[0].state == 2) && (hs->id != -1)) {
            do {
                if (hs->hit != 0) {
                    prompt->mode     = 0;
                    prompt->targetId = 0;
                    work->field_4C   = hs->id;
                    work->promptKind = hs->promptKind;
                    task->state      = 3;
                    return;
                }
                hs++;
            } while (hs->id != -1);
        }
    } else {
        prompt->mode = 1;
    }
    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}

/// Breathes the room's hanging prop: rebuilds the display object's coordinate
/// matrix as a pure Y rotation of `rsin(D_80070F70 * 16)` -- one full turn
/// every 256 frames -- off an identity built the same word-at-a-time way
/// `func_dryfield_breezeway_8017E464` builds the event work's two matrices, then
/// re-seeds the hotspot scan `func_dryfield_breezeway_8017EB8C` at the
/// prompt's own screen position and hit-tests it against the room's table.
///
/// Highlighting the cursor (`mode` 1) is the state the scan runs in; landing on
/// an entry confirms it (`mode` 2) and walks `D_dryfield_breezeway_80182DDC`
/// for the entry that was hit, which is the prop the player is looking at --
/// pressing confirm against it runs cap slot 3 and ends the script in state 5.
/// A cancel press (`buttons[1].state` 2) ends it in state 5 as well.
void func_dryfield_breezeway_8017E81C(Task* task)
{
    RoomActionPrompt* prompt = &D_80114D28;
    GsCOORDINATE2*    coord  = (GsCOORDINATE2*)((TmdObject*)task->extra)->field_8;
    DbwEventWork*     work   = (DbwEventWork*)task->idMap;
    RoomHotspot*      hs     = D_dryfield_breezeway_80182DDC;
    MATRIX*           m;

    prompt->mode     = 1;
    prompt->targetId = 0x80;

    m                            = &coord->coord;
    *(s32*)&coord->coord.m[0][0] = 0x1000;
    *(s32*)&m->m[1][1]           = 0x1000;
    *(s16*)&m->m[2][2]           = 0x1000;
    *(s32*)&m->m[0][2]           = 0;
    *(s32*)&m->m[2][0]           = 0;

    func_8004BFF8(rsin(D_80070F70 * 0x10), m);
    func_dryfield_breezeway_8017EB8C(task, prompt->screen.xy.x, prompt->screen.xy.y);

    if (RoomsShared8017ecb4(hs, work->cursorX, work->cursorY) != 0) {
        prompt->mode = 2;
        while (hs->id != -1) {
            if (hs->hit != 0) {
                Gp_RunCapCmd1(3);
                task->state = 5;
                return;
            }
            hs++;
        }
    }

    if (prompt->buttons[1].state == 2) {
        task->state = 5;
    }
}
