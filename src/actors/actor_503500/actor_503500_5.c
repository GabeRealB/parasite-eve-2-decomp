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
/// 18-entry table of per-slot u16 counters, indexed by slot in
/// `func_actor_503500_801360A4` / `_801360BC` / `_8013611C`.
extern u16 D_actor_503500_80176D64[];
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
/// Main-executable flag byte cleared when the boss enters state 2; also written
/// by `mist_r18`, which has no module header for it either.
extern s8 D_80071090;
s32       func_actor_503500_80133684(Actor503500* arg0);
void      func_actor_503500_80137074(Actor503500* arg0, s8 arg1, s16 arg2);
/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32  func_actor_503500_80135E04(Task* arg0, s32 arg1);
void func_actor_503500_801338E8(Actor503500* arg0);
void func_actor_503500_80134408(Actor503500* arg0);
void func_actor_503500_801345F4(Actor503500* arg0);
void func_actor_503500_80134A24(Actor503500* arg0);
void func_actor_503500_80134C68(Actor503500* arg0);
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 arg2);
s32  func_actor_503500_80136FA8(Actor503500Work* work, s32 slot);
s32  func_actor_503500_80136FDC(Actor503500Work* work, s32 slot);
void func_actor_503500_80136F40(Actor503500Work* work, s32 slot, s32 arg2, s32 arg3);
/// Animation-preset table indexed by preset id; `func_actor_503500_80135FB4`
/// and `func_actor_503500_80132F64` hand entry pointers to
/// `func_actor_503500_80135950`.
extern Actor503500AnimPreset D_actor_503500_8016EAC0[];
/// Applies preset `arg2` to the boss block's animation slots; `arg1` and `arg3`
/// are passed by every caller but the body ignores them.
void func_actor_503500_80135950(Actor503500* arg0, s32 arg1,
                                Actor503500AnimPreset* arg2, s32 arg3);
s32  func_actor_503500_80136014(Actor503500* arg0, s32 arg1);
void func_actor_503500_8013611C(s32 arg0);
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void func_actor_503500_801372AC(s32 arg0);
void func_actor_503500_80136450(Actor503500* arg0);
void func_actor_503500_801369E4(Actor503500* arg0);
void func_actor_503500_80136A80(Actor503500* arg0);
/// Republishes the boss's four cached matrices (`arg1`) and/or re-seeds its
/// display state (`arg2`); lives in `actor_503500_5`.
void func_actor_503500_80136B64(Actor503500* arg0, s32 arg1, s32 arg2);
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
void      func_actor_503500_801334CC(Actor503500* arg0);
void      func_actor_503500_80135178(Actor503500* arg0);
void      func_actor_503500_801353F0(Actor503500* arg0);
void      func_actor_503500_80135644(Actor503500* arg0);
void      func_actor_503500_80136280(Actor503500* arg0);
void      func_actor_503500_80136304(Actor503500* arg0);
void      func_actor_503500_80136A88(Actor503500* arg0);
void      func_actor_503500_80136AEC(Actor503500* arg0);
void      func_actor_503500_80136D30(Actor503500* arg0);
void      func_actor_503500_80136DDC(Actor503500* arg0);
/// Player-facing mode byte in the main executable, also written by the
/// acropolis helicopter landing pad room.
extern s8 D_801153F1;

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

void func_actor_503500_80132DEC(void)
{
    Task*          slot3;
    GsCOORDINATE2* coord;
    SVECTOR*       rot;

    slot3 = Game_GetPtrSlot(3);
    coord = ((TmdObject*)slot3->extra)->field_8;

    D_actor_503500_8017655C.x = coord->coord.t[0];
    D_actor_503500_8017655C.y = coord->coord.t[1];
    D_actor_503500_8017655C.z = coord->coord.t[2];

    /* Anchoring the rotation pointer *after* the three word stores is what
     * makes cse keep the plain symbol as the base address; taking it first
     * anchors the whole function on `D_actor_503500_8017655C + 0x10`. */
    rot = &D_actor_503500_8017655C.rot;

    rot->vx = ((GameActor*)slot3->idMap)->field_50;
    rot->vy = ((GameActor*)slot3->idMap)->field_52;
    rot->vz = ((GameActor*)slot3->idMap)->field_54;
}
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

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80132F64);

/// Per-frame update. `D_801153F4` 1 pauses the boss (buffers kept, only
/// `func_actor_503500_80136AEC` runs), 2 hides it; anything else runs the
/// normal chain. `field_7D9` counts down to the frame the TMD buffers are freed.
void func_actor_503500_80133270(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       tmd;
    s32              mode;

    enemy = arg0->field_20;
    tmd   = arg0->extra;
    mode  = D_801153F4;
    work  = arg0->field_1C;

    switch (mode) {
        case 1:
            if (work->field_7E4 == 0) {
                SndEvt_EnqueueType8(0x40000000);
                Tmd_AllocBuffers(tmd);
                tmd->field_C   &= 0xFF7B;
                work->field_7E4 = mode;
                work->field_7E5 = 0;
            }
            func_actor_503500_80136AEC(arg0);
            return;
        case 2:
            if (work->field_7D9 >= 0) {
                if (work->field_7D9 == 0) {
                    Tmd_FreeBuffers(tmd);
                }
                work->field_7D9--;
            }
            if (work->field_7E5 == 0) {
                SndEvt_EnqueueType8(0x40000000);
                tmd->field_C   |= 0x84;
                work->field_7D9 = 1;
                work->field_7E4 = 0;
                work->field_7E5 = 1;
            }
            return;
        default:
            if (work->field_7E4 == 1 || work->field_7E5 == 1 || work->field_7E7 != 0) {
                SndEvt_EnqueueType9(0x40000000);
                work->field_7E4 = 0;
                work->field_7E5 = 0;
                work->field_7E7 = 0;
            }
            if ((D_80071075 & 0xF0) == 0x40) {
                SndEvt_EnqueueType8(0x40000000);
                work->field_7E7 = 1;
            }
            if (Game_Session->field_1 == 0) {
                tmd->field_C &= 0xFF7F;
            }
            if (work->field_7D9 >= 0) {
                if (work->field_7D9 == 0) {
                    Tmd_FreeBuffers(tmd);
                }
                work->field_7D9--;
            }
            if (enemy->field_4C != 0) {
                func_actor_503500_80136280(arg0);
            }
            func_actor_503500_80136A88(arg0);
            func_actor_503500_80136AEC(arg0);
            func_actor_503500_80136B64(arg0, 0, 0);
            func_actor_503500_801334CC(arg0);
            func_actor_503500_80136304(arg0);
            func_actor_503500_80135178(arg0);
            func_actor_503500_801353F0(arg0);
            func_actor_503500_80136D30(arg0);
            func_actor_503500_80135644(arg0);
            func_actor_503500_80136DDC(arg0);
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_801334CC);

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

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_801338E8);

/// Script step that dismisses one of the boss's slot-10/11 helpers. State 0
/// picks the slot: with `field_7DE` clear it tries the slot chosen by the low
/// bit of `field_7C8` and then the other one, and always advances; otherwise
/// the sign of `field_7BA` picks slot 10 or 11 and it only advances once that
/// slot is ready. State 1 waits for the chosen slot (`field_7C2`) to finish
/// dying. Returns 1 while no slot is ready, 0 the frame the request is issued
/// or while waiting, and 1 once the slot has gone quiet. `arg0` is passed
/// through the step table and ignored here.
s32 func_actor_503500_80133BF4(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;
    s32 odd;
    s32 slot;

    ret = 1;
    switch ((s8)work->field_7DB) {
        case 0:
            if (work->field_7DE == 0) {
                odd  = work->field_7C8 & 1;
                slot = odd + 0xA;
                if (func_actor_503500_80136FDC(work, slot) != 0 ||
                    (slot = 0xB - odd, func_actor_503500_80136FDC(work, slot) != 0)) {
                    func_actor_503500_80136F40(work, slot, 2, 0x78);
                    work->field_7C2 = slot;
                }
                ret             = 0;
                work->field_7D2 = 0;
                work->field_7DB = 1;
            } else if (work->field_7BA > 0) {
                if (func_actor_503500_80136FDC(work, 0xA) != 0) {
                    func_actor_503500_80136F40(work, 0xA, 2, 0x78);
                    ret             = 0;
                    work->field_7C2 = 0xA;
                    work->field_7D2 = 0;
                    work->field_7DB = 1;
                }
            } else if (func_actor_503500_80136FDC(work, 0xB) != 0) {
                func_actor_503500_80136F40(work, 0xB, 2, 0x78);
                ret             = 0;
                work->field_7C2 = 0xB;
                work->field_7D2 = 0;
                work->field_7DB = 1;
            }
            break;
        case 1:
            if (func_actor_503500_80136FA8(work, work->field_7C2) == 0) {
                ret = 0;
            }
            break;
    }
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80133D40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80133FD8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80134284);

/// Two-step state of the boss block. Step 0 hides the second body part,
/// unlinks the enemy node, stores the summed `field_40` of occupied slots
/// 1..16 in `Game_Session->field_12A` and plays sound 0x40230010 at the
/// part's position. Step 1 counts 0x1F frames, then posts message 0x13F4
/// under the same gates as `func_actor_503500_80133684`.
void func_actor_503500_80134408(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    s32              i;
    s16              sum;
    s32              pan;

    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch ((s8)work->field_7DA) {
        case 0:
            work->field_5D4.flags &= 0x7FFF;
            func_actor_503500_8013611C(arg0->spawnArg1);
            Gp_UnlinkNode(&enemy->node);
            enemy->field_54 = 0;
            work->field_7B4 = 0;
            Gp_PulseState1C();
            sum = 0;
            for (i = 1; i < 0x11; i++) {
                if (work->enemies[i] != NULL) {
                    sum += work->enemies[i]->field_40;
                }
            }
            Game_Session->field_12A = sum;
            work->field_7E0         = 0;
            func_actor_503500_80135FB4(arg0, 0xE, 0x20);
            pan = (s8)Gp_GetObjPan((GpObj38*)&arg0->extra->field_8[3]);
            SndEvt_EnqueueType6(0x40230010, pan,
                                (s8)(Gp_GetObjDepth((GpObj38*)&arg0->extra->field_8[3]) / 2));
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (++work->field_7BC >= 0x1F &&
                ((GameActor*)((Task*)Game_GetPtrSlot(3))->idMap)->field_954 != 2 &&
                D_80073BA0 > 0 && D_80114C12 != 1 && D_80071075 == 0) {
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F4, 0, 0);
                SndEvt_EnqueueType7(0x40230010, 0x2D);
                work->field_7DA = work->field_7DA + 1;
            }
            break;
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_801345F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80134A24);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80134C68);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80134EAC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_801353F0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135644);

/// Copies bits 0x80, 2 and 4 of the parent task's model `field_C` onto
/// `arg0`'s model, unless that model is attached to `Gfx_ViewCoord`.
/// Clearing bit 4 reallocates the model's buffers; setting it writes 2 to
/// `*arg1`.
void func_actor_503500_80135828(Actor503500* arg0, s8* arg1)
{
    TmdObject* obj;
    TmdObject* pobj;
    u16        flags;
    u16        flags2;

    obj = arg0->extra;
    if (obj->field_8->sub != &Gfx_ViewCoord) {
        flags = obj->field_C;
        pobj  = arg0->parent->extra;
        if (flags & 0x80) {
            if (!(pobj->field_C & 0x80)) {
                obj->field_C = flags & ~0x80;
            }
        } else if (pobj->field_C & 0x80) {
            obj->field_C = flags | 0x80;
        }
        flags2 = obj->field_C;
        if (flags2 & 2) {
            if (!(pobj->field_C & 2)) {
                obj->field_C = flags2 & ~2;
            }
        } else if (pobj->field_C & 2) {
            obj->field_C = flags2 | 2;
        }
        flags2 = obj->field_C;
        if (flags2 & 4) {
            if (!(pobj->field_C & 4)) {
                obj->field_C = flags2 & ~4;
                Tmd_AllocBuffers(obj);
            }
        } else if (pobj->field_C & 4) {
            obj->field_C = flags2 | 4;
            *arg1        = 2;
        }
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135950);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135B74);

/// Clears slot `arg1` of the boss work block's `enemies` array. `arg0` is
/// loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135CE8(Task* arg0, s32 arg1)
{
    D_actor_503500_80176574.enemies[arg1] = NULL;
}

/// `TaskDesc` table `func_actor_503500_80135D00` spawns slot enemies from.
extern TaskDesc D_actor_503500_8016E924;

/// Spawns table entry `arg1` as a child of `arg0`'s enemy, tints its model
/// from the current area's record and parks it in slot `arg1` of the boss
/// work block's `enemies` array. Returns the new enemy, or NULL.
GpEnemy* func_actor_503500_80135D00(Actor503500* arg0, s32 arg1)
{
    GpAreaKey  key;
    GpAreaKey* sessionKey;
    u8         areaByte0;
    GpAreaRec* rec;
    GpCdRec10* entry;
    GpEnemy*   enemy;
    TmdObject* model;
    s32        idx;
    u32        raw;
    /* Taken before the spawn call: the ROM keeps the address in s4 across
       every call rather than rebuilding it at the store. */
    Actor503500Work* work = &D_actor_503500_80176574;

    enemy = Gp_SpawnEnemyFromTable(&D_actor_503500_8016E924, arg1, arg1, arg0->field_20);
    if (enemy != NULL) {
        sessionKey  = (GpAreaKey*)&Game_Session->field_4;
        raw         = arg0->field_20->field_8;
        model       = (TmdObject*)enemy->task->extra;
        key.field_3 = sessionKey->field_3;
        key.field_2 = sessionKey->field_2;
        key.field_1 = sessionKey->field_1;
        areaByte0   = sessionKey->field_0;
        idx         = raw >> 12;
        key.field_0 = areaByte0;
        Gp_SyncAreaKeyIndex(&key);
        rec             = Gp_GetNestedAreaRec(&key);
        entry           = (GpCdRec10*)((idx << 4) + (s32)rec->field_0);
        model->field_24 = entry->field_D;
        model->field_25 = entry->field_E;
        if (model->field_18 != NULL) {
            Tmd_ProcessStream(model);
            Tmd_ProcessStream(model);
        }
        work->enemies[arg1] = enemy;
    }
    return enemy;
}

/// Reports whether slot `arg1` of the boss work block's `enemies` array is
/// empty. `arg0` is loaded by every caller but the body ignores it.
s32 func_actor_503500_80135E04(Task* arg0, s32 arg1)
{
    return D_actor_503500_80176574.enemies[arg1] == NULL;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135E20);

/// Records the per-slot halfword for slot `arg1` of the boss work block.
/// `arg0` is loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2)
{
    D_actor_503500_80176574.field_730[arg1] = arg2;
}

/// Sets the per-slot rate `GpAnimSlot::field_9` on animation slots 1..16 of the
/// boss block -- `rate` of 0 meaning `Gp_AnimResetSlot`'s own 0x10 default,
/// exactly as `func_actor_503500_80137048` does -- then applies preset `arg1`.
void func_actor_503500_80135FB4(Actor503500* arg0, s32 arg1, s32 rate)
{
    Actor503500WorkBoss* work;
    GpAnimSlot*          slot;
    s32                  i;

    work = (Actor503500WorkBoss*)arg0->field_1C;
    slot = &work->slots[1];
    if (rate == 0) {
        rate = 0x10;
    }
    for (i = 0xF; i >= 0; i--) {
        slot->field_9 = rate;
        slot++;
    }
    func_actor_503500_80135950(arg0, 0x7D3, &D_actor_503500_8016EAC0[arg1], 0);
}
/// Reports whether the boss is in sub-state `arg1` and, if so, whether either
/// of the 0x102 bits of its state flag halfword is set. Returns -1 for any
/// other sub-state. `arg0` is loaded by every caller but the body ignores it,
/// the same way `func_actor_503500_80135E04` does.
s32 func_actor_503500_80136014(Actor503500* arg0, s32 arg1)
{
    if (arg1 != D_actor_503500_80176574.field_7D5) {
        return -1;
    }
    return (D_actor_503500_80176574.slot40.boss.flags_4C & 0x102) != 0;
}
/// Puts the boss into state 2: clears the state's step counters and the two
/// per-state halfwords, asks `func_actor_503500_80137074` for sub-state 3 and
/// drops the main-executable flag.
void func_actor_503500_80136048(Actor503500* arg0)
{
    Actor503500Work* work;

    work            = arg0->field_1C;
    work->field_7B0 = 2;
    work->field_7DA = 0;
    work->field_7DB = 0;
    work->field_7BC = 0;
    work->field_7BE = 0;
    func_actor_503500_80137074(arg0, 0, 3);
    D_80071090 = 0;
}
/// Reports whether the boss-wide gate is open; the body ignores its
/// argument, and callers pass unrelated pointers they already hold.
s32 func_actor_503500_8013608C(void* arg0)
{
    return (u32)(D_actor_503500_80176D24 - 2) < 3U;
}

void func_actor_503500_801360A4(s32 arg0, s16 arg1)
{
    D_actor_503500_80176D64[arg0] = arg1;
}

/// Tries to claim `arg1` counts for slot `arg0`: sums every *other* slot's
/// counter plus the requested amount and, if the total stays under 9, writes
/// the request into the slot. Returns whether it was granted.
s32 func_actor_503500_801360BC(s32 arg0, s32 arg1)
{
    s32  accepted;
    s32  total;
    s32  i;
    u16* slot;

    accepted = 0;
    total    = arg1;
    i        = 0;
    slot     = D_actor_503500_80176D64;
    do {
        if (i != arg0) {
            total += (s16)*slot;
        }
        i++;
        slot++;
    } while (i < 0x12);

    if (total < 9) {
        D_actor_503500_80176D64[arg0] = (u16)arg1;
        accepted                      = 1;
    }
    return accepted;
}
void func_actor_503500_8013611C(s32 arg0)
{
    D_actor_503500_80176D64[arg0] = 0;
}

/// Main-executable matrix the scene tracks (`acropolis_forked_road.h` calls it
/// the camera target); no module header owns it yet.
extern MATRIX* D_80073B8C;

/// Yaw from the actor's first part to `D_80073B8C`'s translation, relative to
/// the part's own heading, wrapped into [-0x800, 0x800).
s16 func_actor_503500_80136134(Actor503500* arg0)
{
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s16            angle;

    coord  = arg0->extra->field_8;
    vec.vx = D_80073B8C->t[0] - coord->coord.t[0];
    vec.vy = 0;
    vec.vz = D_80073B8C->t[2] - coord->coord.t[2];
    angle  = ratan2(vec.vx, vec.vz) - ratan2(coord->coord.m[0][2], coord->coord.m[2][2]);
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    return angle;
}

s8 func_actor_503500_80136208(void)
{
    return D_actor_503500_80176D5A;
}

s16 func_actor_503500_80136218(void)
{
    return D_actor_503500_80176D2E;
}

/// Exit callback of the boss task: tears down the second body part's display
/// node, clears the enemy's `field_54` back-pointer slot and destroys it.
void func_actor_503500_80136228(Actor503500* arg0)
{
    GpEnemy* enemy;

    enemy = arg0->field_20;
    func_actor_503500_80136B64(arg0, 0, 1);
    Gp_UnlinkObj(&arg0->field_1C->field_5D4);
    enemy->field_54 = 0;
    arg0->field_1C  = NULL;
    Gp_DestroyEnemy(enemy, (Task*)arg0);
}

void func_actor_503500_80136280(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    u8               flags;

    enemy = arg0->field_20;
    flags = enemy->field_4C;
    work  = arg0->field_1C;
    if (flags & 1) {
        enemy->field_4C = flags & 0xFE;
    }
    if (enemy->field_4C & 2) {
        enemy->field_4C &= 0xFD;
        func_actor_503500_80136EFC(arg0, 3);
        work->field_7B2 = 3;
    }
    flags = enemy->field_4C;
    if (flags & 0xC) {
        enemy->field_4C = flags & 0xF3;
    }
}
