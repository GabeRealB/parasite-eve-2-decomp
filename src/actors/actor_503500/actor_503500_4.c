#include "common.h"

#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "actors/actor_503500.h"
#include "actors/actors_shared_801366fc.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];
void              func_actor_503500_801324EC(Task* arg0);
extern Task*      D_actor_503500_80176558;
/// The boss work block, cleared by `func_actor_503500_80132F64`.
extern Actor503500Work D_actor_503500_80176574;
extern TaskDesc        D_actor_503500_8014B964;
extern s8              D_actor_503500_80176D5A;
extern s16             D_actor_503500_80176D2E;
/// 18-entry table of per-slot s16 counters, indexed by slot in
/// `func_actor_503500_801360A4` / `_801360BC` / `_8013611C`.
extern s16 D_actor_503500_80176D64[];
extern u16 D_actor_503500_80176D24;
/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_503500_8014CD98[];
extern u8 D_actor_503500_8014D098[];
/// whatever room overlay is resident owns the body.
extern void              func_8017E27C(s32 arg0);
extern Actor503500MsgPos D_actor_503500_8017655C;
/// Player-facing flag byte in the main executable; no module header owns it yet.
extern u8 D_80073BA9;
/// Main-executable globals with no module header yet: `D_80071075` gates the
/// "everything is dead" message, `D_80073BA0` is the remaining-enemy count and
/// `D_80114C12` the cutscene/among-us mode flag.
extern u8  D_80071075;
extern s16 D_80073BA0;
extern s8  D_80114C12;
s32        func_actor_503500_80133684(Actor503500* arg0);
/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32  func_actor_503500_80135E04(Task* arg0, s32 arg1);
void func_actor_503500_801338E8(Actor503500* arg0);
void func_actor_503500_80134408(Actor503500* arg0);
void func_actor_503500_801345F4(Actor503500* arg0);
void func_actor_503500_80134A24(Actor503500* arg0);
void func_actor_503500_80134C68(Actor503500* arg0);
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 arg2);
s32  func_actor_503500_80136014(Actor503500* arg0, s32 arg1);
void func_actor_503500_8013611C(s32 arg0);
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void func_actor_503500_801372AC(s32 arg0);
void func_actor_503500_80136450(Actor503500* arg0);
void func_actor_503500_801369E4(Actor503500* arg0);
void func_actor_503500_80136A80(Actor503500* arg0);
void func_actor_503500_80136EFC(Actor503500* arg0, s32 arg1);
void func_actor_503500_801374BC(Actor503500* arg0);
void func_actor_503500_80137678(Actor503500* arg0);
void func_actor_503500_80138454(Actor503500* arg0);
void func_actor_503500_8013B460(Actor503500* arg0);
void func_actor_503500_8013B8D0(Actor503500* arg0);
void func_actor_503500_8013BE0C(Actor503500* arg0);
void func_actor_503500_8013E384(Actor503500* arg0);
void func_actor_503500_8013E740(Actor503500* arg0);
void func_actor_503500_8013EBE4(Actor503500* arg0);
/// Global "everything is frozen" mode byte in the main executable: 1 pauses the
/// actor, 2 hides it, anything else runs the normal per-frame chain.
extern u8 D_801153F4;
void      func_actor_503500_801398D0(Actor503500* arg0);
void      func_actor_503500_80139EFC(Actor503500* arg0);
void      func_actor_503500_8013A0D0(Actor503500* arg0);
void      func_actor_503500_8013A96C(Actor503500* arg0);
void      func_actor_503500_8013AA44(Actor503500* arg0);
void      func_actor_503500_8013AAC0(Actor503500* arg0);
void      func_actor_503500_8013AB38(Actor503500* arg0);
void      func_actor_503500_8013DBA8(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8013F328(Actor503500* arg0);
void      func_actor_503500_8013F4A4(Actor503500* arg0);
void      func_actor_503500_8013F948(Actor503500* arg0);
void      func_actor_503500_8013F984(Actor503500* arg0);
void      func_actor_503500_80140BE8(Actor503500* arg0);
void      func_actor_503500_80141248(Actor503500* arg0);
void      func_actor_503500_80141448(Actor503500* arg0);
void      func_actor_503500_80141B94(Actor503500* arg0);
void      func_actor_503500_80141D7C(Actor503500* arg0);
void      func_actor_503500_801420C4(Actor503500* arg0);
void      func_actor_503500_801421A8(Actor503500* arg0);
void      func_actor_503500_80142310(Actor503500* arg0, s32 arg1);
void      func_actor_503500_8014271C(Actor503500* arg0);
void      func_actor_503500_80142980(Actor503500* arg0);
void      func_actor_503500_8014418C(Actor503500* arg0);
void      func_actor_503500_801441E8(Actor503500* arg0);
void      func_actor_503500_80144238(Actor503500* arg0, s32 arg1);
/// Player-facing mode byte in the main executable, also written by the
/// acropolis helicopter landing pad room.
extern s8 D_801153F1;

void func_actor_503500_80132CC4(s8 arg0)
{
    Gp_ReleaseStateF0Add((GpObj20E*)Gp_LookupSlot4(0), 0x23);
    D_801153F1 = arg0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80132D00);

void func_actor_503500_80132D20(Task* arg0)
{
    func_800E8634((s32)D_actor_503500_8014CD98, 0, (s32)D_actor_503500_8014D098);
    Task_Kill(arg0);
}

void func_actor_503500_80132D60(void)
{
    Gp_StateC08.field_6 |= 1;
}

void func_actor_503500_80132D7C(void)
{
    Game_Session->field_52 = 1;
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

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80132DEC);

void func_actor_503500_80132E7C(void)
{
    Task* slot3;

    slot3 = Game_GetPtrSlot(3);
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
    func_80106350(Game_GetPtrSlot(3), D_80073BA9, 0);
}

void func_actor_503500_80132F28(void)
{
    Gp_HaltPadScripts();
    Game_Session->field_13B = 0;
}

void func_actor_503500_80132F58(void)
{
    D_actor_503500_80176558 = NULL;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80132F64);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80133270);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_801334CC);

s32 func_actor_503500_80133684(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy**        slots;
    GpEnemy*         slot1;
    s32              ret;

    ret   = 0;
    work  = arg0->field_1C;
    slots = work->enemies;

    if (work->field_7B0 != 2) {
        if (!(work->field_774 & 1) && (slots[4] == NULL)) {
            slots[10]->task->killCountdown = 8;
            ret                            = 1;
            work->field_774               |= 1;
        }
        if (!((work->field_774 >> 1) & 1) && (slots[5] == NULL)) {
            slots[11]->task->killCountdown = 8;
            ret                            = 1;
            work->field_774               |= 2;
        }
        if (!((work->field_774 >> 2) & 1) && (slots[1] == NULL)) {
            if (slots[12] != NULL) {
                slots[12]->task->killCountdown = 8;
                ret                            = 1;
                work->field_774               |= 4;
            }
        }
    }

    if (!((work->field_774 >> 3) & 1) &&
        ((((slot1 = slots[1], slot1 == NULL)) && (slots[12] == NULL)) ||
         (slots[7] == NULL) || (slots[8] == NULL) || (slots[10] == NULL) ||
         (slots[11] == NULL) || ((slots[9] == NULL) && (slot1 == NULL)) ||
         ((slots[4]->field_40 == 0) && (slots[5]->field_40 == 0)))) {
        if ((((GameActor*)((Task*)Game_GetPtrSlot(3))->idMap)->field_954 != 2) &&
            (D_80073BA0 > 0) && (D_80114C12 != 1)) {
            ret = 1;
            if (D_80071075 == 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 0, 0);
                work->field_774 |= 8;
                /* `ret` has to be dead across the call for GCC to keep it in
                 * $a1: it is re-set on the way out of both arms. */
                goto done;
            }
        } else {
        done:
            ret = 1;
        }
    }
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_801338E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80133BF4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80133D40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80133FD8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80134408);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_801345F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80134A24);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80134C68);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80134EAC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_801353F0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135644);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135828);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135950);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135B74);

/// Clears slot `arg1` of the boss work block's `enemies` array. `arg0` is
/// loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135CE8(Task* arg0, s32 arg1)
{
    D_actor_503500_80176574.enemies[arg1] = NULL;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135D00);

/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32 func_actor_503500_80135E04(Task* arg0, s32 arg1)
{
    return D_actor_503500_80176574.enemies[arg1] == NULL;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135E20);

/// Records the per-slot halfword for slot `arg1` of the boss work block.
/// `arg0` is loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2)
{
    D_actor_503500_80176574.field_730[arg1] = arg2;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80135FB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80136014);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80136048);

s32 func_actor_503500_8013608C(void)
{
    return (u32)(D_actor_503500_80176D24 - 2) < 3U;
}

void func_actor_503500_801360A4(s32 arg0, s16 arg1)
{
    D_actor_503500_80176D64[arg0] = arg1;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_801360BC);

void func_actor_503500_8013611C(s32 arg0)
{
    D_actor_503500_80176D64[arg0] = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80136134);

s8 func_actor_503500_80136208(void)
{
    return D_actor_503500_80176D5A;
}

s16 func_actor_503500_80136218(void)
{
    return D_actor_503500_80176D2E;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80136228);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_4", func_actor_503500_80136280);
