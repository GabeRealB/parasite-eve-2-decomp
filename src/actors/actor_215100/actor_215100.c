#include "common.h"

#include "actors/actor_215100.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

/// Imports from the 0x80180000 overlay; no header names them yet.
void            func_80180390(s32 arg0);
void            func_801811C0(s16 arg0);
void            func_801848B4(void);
void            func_80184954(void);
extern TaskDesc D_80185384;
extern TaskDesc D_801856B8;
extern s8       D_8007216C;
extern u8       D_80071085;
extern u8       D_80071075;
extern Task*    D_8018E0C4;
extern TaskDesc D_actor_215100_8014CF6C;
extern TaskDesc D_actor_215100_8014E13C;
extern TaskDesc D_actor_215100_801544FC;
extern TaskDesc D_actor_215100_80154508;
extern Task*    D_actor_215100_8015E64C;
extern s32      D_actor_215100_8014D038;
extern s32      D_actor_215100_8014D03C;
extern s32      D_actor_215100_8014D044;
extern s32      D_actor_215100_80153ED4;
extern s32      D_actor_215100_80153FDC;
extern s32      D_actor_215100_801543E4;
/// Glyph metrics table this overlay's caption metrics are read out of, the
/// counterpart of gameplay's `Gp_CapGlyphs`. `func_actor_215100_8014B1B0`
/// stores it and `func_actor_215100_8014C360` indexes it with a text stream's
/// `code & 0x3FF`.
extern GlyphUvwh* D_actor_215100_8015E654;
/// Caption script table, and the script currently being played back with the
/// entry it is up to.
extern Actor215100Caption** D_actor_215100_8015E650;
extern Actor215100Caption*  D_actor_215100_8015E658;
extern s16                  D_actor_215100_8015E65C;
extern s16                  D_actor_215100_8015E65E;
extern s16                  D_actor_215100_8015E660;
extern s16                  D_actor_215100_8015E662;
extern s16                  D_actor_215100_8015E664;
extern s16                  D_actor_215100_8015E666;
extern s8                   D_actor_215100_8015E66C;
extern s32                  D_actor_215100_8015E670;
extern s16                  D_actor_215100_801544EC;
extern s16                  D_actor_215100_801544EE;
extern Actor215100CharRec   D_actor_215100_8015E678;
/// Caption schedule `func_actor_215100_8014AFAC` scans, terminated by a -1
/// `field_0`.
extern Actor215100CapWindow D_actor_215100_80154514[];
extern u8                   D_801153F4;
extern u8                   D_80115690;
void                        func_actor_215100_8014B0D4(void);
void                        func_actor_215100_8014B1B0(s32 arg0);
s32                         func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2);
s16                         func_actor_215100_8014BDFC(s32 arg0);
s16                         func_actor_215100_8014C06C(s32 arg0);
s16                         func_actor_215100_8014C298(s32 arg0);
s32                         func_actor_215100_8014C418(s32 arg0);
void                        func_actor_215100_8014B3C8(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
void                        func_actor_215100_8014BEE8(void);

INCLUDE_RODATA("actors/nonmatchings/actor_215100/actor_215100", D_actor_215100_80149E20);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_80149F2C);

/// Arms the weapon pickup at this actor's spot while the event flag
/// `D_actor_215100_8014D038` is up and the story step has reached 3. A session
/// leave (`Game_Session->field_4 == 0x12`) drops the `func_80180390` hold and
/// `D_actor_215100_8014D03C` with it, sub-states 2 and 3 of
/// `Gp_StateC08.field_A` start the 0x3C-frame cooldown in
/// `D_actor_215100_8014D044`, and while that cooldown runs the function only
/// ticks it down.
///
/// Otherwise the player has to be standing in the zone — its model root's X
/// below -0x1806 and its Z inside [0x10CD, 0x1644) — not aiming
/// (`GameActor.field_954 != 2`), with the caption system idle, `D_80115768`
/// and `D_80071075` clear, its yaw inside one of the two 0x3FF-wide windows
/// opening at 0x201 and 0xA01, and one of the 0x1000 / 0x4000 pad masks held.
/// Either mask runs the handoff `func_actor_215100_8014AA54` uses: the weapon
/// message, caption command 0x14 and the scene task `D_actor_215100_8014CF6C`.
void func_actor_215100_8014A398(void)
{
    Task*          task;
    GameActor*     actor;
    GsCOORDINATE2* coord;
    s32            z;
    s32            facing;

    task  = (Task*)Game_GetPtrSlot(3);
    actor = (GameActor*)task->idMap;
    coord = ((TmdObject*)task->extra)->field_8;
    if (D_actor_215100_8014D038 != 0) {
        if (D_actor_215100_8015E670 >= 3) {
            if (Game_Session->field_4 == 0x12) {
                func_80180390(0);
                D_actor_215100_8014D03C = 0;
            }
            if ((u32)((u8)Gp_StateC08.field_A - 2) < 2U) {
                D_actor_215100_8014D044 = 0x3C;
            }
            if (D_actor_215100_8014D044 != 0) {
                D_actor_215100_8014D044 -= 1;
                return;
            }
            if ((actor->field_954 != 2) && (Gp_CapBusy() == 0) && (D_actor_215100_8014D03C == 0) &&
                (D_80115768 == 0) && (coord->coord.t[0] < -0x1806)) {
                z = coord->coord.t[2];
                if (z < 0x1644) {
                    if ((z >= 0x10CD) && (Gp_StateC08.field_A != 1) && (D_80071075 == 0)) {
                        facing = (u16)actor->field_52 & 0xFFF;
                        if (Pad_CheckButtons(0, 0, 0x1000) != 0) {
                            if ((u32)(facing - 0xA01) < 0x3FFU) {
                                Gp_MsgPlayerWeapon(0);
                                D_801153F4 = 1;
                                Gp_RunCapCmd(0x14, 0);
                                D_80115690 = 1;
                                Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 0, 0);
                            }
                        }
                        if ((Pad_CheckButtons(0, 0, 0x4000) != 0) && ((u32)(facing - 0x201) < 0x3FFU)) {
                            Gp_MsgPlayerWeapon(0);
                            D_801153F4 = 1;
                            Gp_RunCapCmd(0x14, 0);
                            D_80115690 = 1;
                            Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 0, 0);
                        }
                    }
                }
            }
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014A5C0);

/// Watches the caption system while the actor waits to be talked to: state 0
/// polls `Gp_CapBusy` / `Gp_GetCapEventKey`, and on key 2 hands the scene task
/// `D_8018E0C4` its exit and steps to state 1, while any other key kills the
/// task outright. State 1 starts the caption playback and steps to state 2,
/// which commits the ending: it flags the save-slot session, plays the sound,
/// clears the actor's own 0x97B, drops the story flag the sibling
/// `func_actor_215100_8014A908` sets, and releases the display reference.
void func_actor_215100_8014A7C4(Task* arg0)
{
    GameActor* actor;

    actor = (GameActor*)((Task*)Game_GetPtrSlot(3))->idMap;
    switch (arg0->state) {
        case 0:
            if (Gp_CapBusy() != 0) {
                break;
            }
            if (Gp_GetCapEventKey() == 2) {
                Task_CallExit(D_8018E0C4);
                arg0->state++;
            } else {
                Task_Kill(arg0);
            }
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            D_8007216C = 8;
            func_801811C0(0);
            arg0->state++;
            break;
        case 2:
            Game_Session->field_126 = 1;
            Game_Session->field_69 |= 0x80;
            SndEvt_EnqueueType2(0, 0x1E);
            actor->field_97B        = 0;
            D_actor_215100_8014D038 = 0;
            Gp_MsgPlayerWeapon(1);
            Gp_StateC08.field_6 &= 0xFD;
            if (D_80071085 != 0) {
                Display_ReleaseRef();
            }
            Task_Kill(arg0);
            break;
    }
}

void func_actor_215100_8014A908(void)
{
    D_actor_215100_8014D038 = 0;
    if (D_actor_215100_8015E670 < 3) {
        D_8007216C = 8;
        func_801811C0(0);
    } else {
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
    }
    if (D_actor_215100_8015E670 < 4) {
        Gp_StateC08.field_6 &= 0xFD;
    }
    SndEvt_EnqueueType2(0, 0x1E);
}

void func_actor_215100_8014A9A0(void)
{
    if (D_actor_215100_8015E670 == 5) {
        D_actor_215100_8014D038 = 0;
        func_80180390(1);
        D_actor_215100_8014D03C = 1;
        Game_Session->field_126 = 1;
        SndEvt_EnqueueType2(0, 0x1E);
        Game_Session->field_69 |= 0x80;
    }
    if (D_actor_215100_8015E670 < 3) {
        Gp_RunCapCmd(0x1D, 3);
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 1, 0, 0);
    }
}

/// Hands the actor off to its caption script, or starts one, depending on
/// whether the script for the current story flag has already run.
///
/// The `else` arm is a `do { } while (0)` whose `break` is the "already
/// committed" exit. It is not vestigial: the loop notes it emits make `reorg`
/// mark that branch's label as leaving a loop, so the delay-slot pass predicts
/// it not-taken and fills its slot from the fall-through rather than from the
/// shared `return 2` tail. Without the loop the branch reaches the same label
/// by a copied `li v0,2`, one instruction longer.
s32 func_actor_215100_8014AA54(Actor215100CharRec* arg0)
{
    if (D_actor_215100_8014D038 != 0) {
        if (arg0->field_5 != 0) {
            return 2;
        }
        D_actor_215100_8015E678 = *arg0;
        Gp_MsgPlayerWeapon(0);
        D_801153F4 = 1;
        Gp_RunCapCmd(0x14, 0);
        D_80115690 = 1;
        Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 1, 0);
    } else {
        do {
            if (GameFlag_GetNibble(0xED) == 0) {
                return 1;
            }
            if (arg0->field_5 != 0) {
                break;
            }
            D_actor_215100_8015E678 = *arg0;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(0x17);
            Task_SpawnFromTable(&D_actor_215100_8014CF6C, 0, 2, 0);
        } while (0);
    }
    return 2;
}

void func_actor_215100_8014AB6C(void)
{
    if (D_actor_215100_8014D038 != 0) {
        func_80184954();
        return;
    }
    Gp_SpawnIfCapIdle(0x11, 1);
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014ABAC);

void func_actor_215100_8014AD50(Task* arg0)
{
    if (arg0->killCountdown % 48 == 0) {
        Task_SpawnFromTable(&D_801856B8, 1, 0, 0);
    }
    arg0->killCountdown = arg0->killCountdown + 1;
}

void func_actor_215100_8014ADD8(void)
{
    Task_SpawnFromTable(&D_80185384, 0, 0, 0);
}

void func_actor_215100_8014AE08(s32 arg0)
{
    if (arg0 != 0) {
        func_801848B4();
    }
}

void func_actor_215100_8014AE2C(s32 arg0)
{
    if (arg0 != 0) {
        D_actor_215100_8015E64C = Task_SpawnFromTable(&D_actor_215100_8014E13C, 2, 0, 0);
        return;
    }
    if (D_actor_215100_8015E64C != NULL) {
        Task_Kill(D_actor_215100_8015E64C);
        D_actor_215100_8015E64C = NULL;
    }
}

void func_actor_215100_8014AE90(s16 arg0)
{
    func_801811C0(arg0);
}

void func_actor_215100_8014AEB4(s16 arg0)
{
    Game_Session->field_52 = arg0;
}

void func_actor_215100_8014AEC4(s32 arg0)
{
    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(1);
        func_800E6D4C(0x300, 0);
        return;
    }
    Gp_ResetCap();
}

void func_actor_215100_8014AF0C(void)
{
    switch (GameFlag_GetNibble(0xF5)) {
        case 0:
            GameFlag_SetNibble(0xF5, 1);
            func_800E8614((s32)&D_actor_215100_80153ED4, 0);
            break;
        case 1:
            func_800E8614((s32)&D_actor_215100_80153FDC, 0);
            GameFlag_SetNibble(0xF5, 2);
            break;
        case 2:
            func_800E8614((s32)&D_actor_215100_801543E4, 0);
            break;
    }
}

/// Drives the caption schedule while the actor waits to be talked to: state 0
/// arms it, and state 1 scans `D_actor_215100_80154514` for the window
/// containing `Game_Session.field_120` - the first entry whose `field_0 * 30`
/// has not dropped below the clock and whose `field_4 * 30` has - and, when it
/// finds one, starts that entry's script at its own line key with the task's
/// `spawnArg1` as the line delay. It then ticks the clock down one, unless the
/// caption system is busy or `D_801153F4` is up.
void func_actor_215100_8014AFAC(Actor215100* task, s32 arg1)
{
    s32 i;
    s32 script;
    s32 key;
    s32 time;

    switch (task->state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            script = 0;
            key    = arg1;
            for (i = 0; D_actor_215100_80154514[i].field_0 != -1; i++) {
                time = Game_Session->field_120;
                if ((D_actor_215100_80154514[i].field_0 * 30 >= time) &&
                    (D_actor_215100_80154514[i].field_4 * 30 < time)) {
                    script = D_actor_215100_80154514[i].field_8;
                    key    = D_actor_215100_80154514[i].field_C;
                    break;
                }
            }
            if (script != 0) {
                func_actor_215100_8014B2B8(script, key, task->spawnArg1Lo);
                func_actor_215100_8014B0D4();
            }
            if ((Gp_CapBusy() == 0) && (D_801153F4 == 0)) {
                Game_Session->field_120 = (u16)Game_Session->field_120 - 1;
            }
            break;
    }
}

void func_actor_215100_8014B0D4(void)
{
    if ((D_actor_215100_8015E658 != NULL) &&
        (D_actor_215100_8015E658[D_actor_215100_8015E662].field_8 != -1) &&
        (Gp_CapBusy() == 0)) {
        func_actor_215100_8014B3C8(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8, 0x80, 1,
                                   D_actor_215100_8015E658[D_actor_215100_8015E662].field_0 |
                                       ((D_actor_215100_8015E658[D_actor_215100_8015E662].field_1 & 0x10) * 0x10));
        if (!(D_actor_215100_8015E658[D_actor_215100_8015E662].field_4 & 1)) {
            func_actor_215100_8014BEE8();
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B1B0);

/// Starts playing the caption script `arg0` picks out of
/// `D_actor_215100_8015E650`, keyed on `arg1`, and parks its per-line metrics in
/// the globals `func_actor_215100_8014B0D4` reads. Returns 1 when there is no
/// such script, 0 once it is playing; `arg2` is the line delay.
s32 func_actor_215100_8014B2B8(s16 arg0, s16 arg1, s32 arg2)
{
    Actor215100Caption* caption;
    s16                 entry;

    caption                 = D_actor_215100_8015E650[arg0];
    D_actor_215100_8015E658 = caption;
    if (caption == NULL) {
        return 1;
    }
    D_actor_215100_8015E666 = arg1;
    entry                   = func_actor_215100_8014C418(1);
    D_actor_215100_8015E662 = entry;
    D_actor_215100_8015E660 = arg2;
    D_actor_215100_8015E65C = func_actor_215100_8014C06C(D_actor_215100_8015E658[entry].field_8);
    D_actor_215100_8015E65E = func_actor_215100_8014BDFC(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E664 = func_actor_215100_8014C298(D_actor_215100_8015E658[D_actor_215100_8015E662].field_8);
    D_actor_215100_8015E66C = 0x1E;
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014B3C8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BDFC);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014BEE8);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C06C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C17C);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C298);

/// Height of the caption line the text stream `arg0` starts with, walking it
/// the way gameplay's `func_800E6BB8` does — this overlay's caption system is
/// a copy of that one, and the two functions compile to the same 0xB8 bytes
/// with only the glyph table symbol differing.
///
/// The running maximum starts at 0 and each glyph code (non-negative, `& 0x3FF`
/// indexing `D_actor_215100_8015E654`) raises it to that glyph's `h + 2`. Either
/// terminator ends the scan: `-2` leaves the maximum as it stands, `-1` forces
/// 0xD, and any other negative code is stepped over like a glyph without
/// touching the maximum. A maximum still at 0 — the stream opened with `-2` —
/// comes back as 2.
s32 func_actor_215100_8014C360(u16* arg0)
{
    s32                 height;
    s32                 i;
    s32                 cont;
    u16                 code;
    s32                 shifted;
    volatile GlyphUvwh* glyph;
    GlyphUvwh*          table;
    s32                 next;
    s32                 htmp;
    s32                 v0tmp;

    height  = 0;
    i       = height;
    cont    = 1;
    code    = arg0[0];
    table   = D_actor_215100_8015E654;
    shifted = code << 16;
    for (;;) {
        shifted = shifted >> 16;
        if (shifted == -2) {
            cont = 0;
        } else if (shifted == -1) {
            cont   = 0;
            height = 0xD;
        } else {
            if (shifted >= 0) {
                glyph = (GlyphUvwh*)((code & 0x3FF) * sizeof(GlyphUvwh) + (s32)table);
                if (height < glyph->h + 2) {
                    htmp   = glyph->h;
                    height = htmp + 2;
                    goto do_inc;
                }
                next = i + 1;
            } else {
            do_inc:
                next = i + 1;
            }
            i = next;
            TOUCH_REG(next);
            code = arg0[(s16)next];
        }
        v0tmp = cont;
        TOUCH_REG(v0tmp);
        if (v0tmp == 0) {
            break;
        }
        shifted = code << 16;
    }
    if (height == 0) {
        height = 2;
    }
    return height;
}

s32 func_actor_215100_8014C418(s32 arg0)
{
    s32                 flag;
    s32                 id;
    s32                 base;
    Actor215100Caption* p;

    flag = -1;
    id   = D_actor_215100_8015E666;
    base = (s32)D_actor_215100_8015E658;
    p    = (Actor215100Caption*)(arg0 * sizeof(Actor215100Caption) + base);
loop:
    if (p->field_8 == flag) {
        goto done;
    }
    if (p->field_5 == id) {
        goto done;
    }
    p++;
    arg0++;
    goto loop;
done:
    return arg0;
}

void func_actor_215100_8014C46C(Task* task)
{
    s32 remaining;

    remaining       = task->spawnArg1 - 1;
    task->spawnArg1 = remaining;
    if (remaining <= 0) {
        Task_Kill(task);
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C4A8(Task* task)
{
    s32 remaining;
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            task->state = 1;
            break;
        case 1:
            remaining       = task->spawnArg1 - 1;
            task->spawnArg1 = remaining;
            if ((remaining <= 0) || (Pad_CheckButtons(0, 1, Pad_MaskCancel) != 0)) {
                Task_Kill(task);
                Stage_SetEndingFlag();
            }
            break;
    }
    func_actor_215100_8014B0D4();
}

void func_actor_215100_8014C538(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_actor_215100_801544FC, 0, arg2, 0);
}

void func_actor_215100_8014C58C(s16 arg0, s16 arg1, s16 arg2)
{
    func_actor_215100_8014B2B8(arg0, arg1, 0xD0);
    Display_InitModeObj(&D_actor_215100_80154508, arg2, 0, 0);
}

void func_actor_215100_8014C5E0(s16 arg0, s16 arg1, s16 arg2)
{
    s32 count;
    s32 i;

    count                   = 0;
    D_actor_215100_801544EC = arg0;
    D_actor_215100_801544EE = arg1;
    for (i = 0; i < 0x32; i++) {
        if (D_8006C338[i].field_0 == 3) {
            if (count == arg2) {
                func_actor_215100_8014B1B0(D_8006C338[i].field_4);
                break;
            }
            count++;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", ActorsShared80131e24Sub0);

INCLUDE_ASM("actors/nonmatchings/actor_215100/actor_215100", func_actor_215100_8014C874);
