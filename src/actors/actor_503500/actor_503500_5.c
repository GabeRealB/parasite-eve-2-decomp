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
void func_actor_503500_80135F9C(Task* arg0, s32 arg1, s16 arg2);
s32  func_actor_503500_80136FA8(Actor503500Work* work, s32 slot);
s32  func_actor_503500_80133D40(Actor503500* arg0, Actor503500Work* work);
s32  func_actor_503500_80133FD8(Actor503500* arg0, Actor503500Work* work);
s32  func_actor_503500_80136FDC(Actor503500Work* work, s32 slot);
void func_actor_503500_80136F40(Actor503500Work* work, s32 slot, s32 arg2, s32 arg3);
/// Animation-preset table indexed by preset id; `func_actor_503500_80135FB4`
/// and `func_actor_503500_80132F64` hand entry pointers to
/// `func_actor_503500_80135950`.
extern Actor503500AnimPreset D_actor_503500_8016EAC0[];
/// Applies preset `arg2` to the boss block's animation slots; `arg1` and `arg3`
/// are passed by every caller but the body ignores them. Always returns 0.
s32 func_actor_503500_80135950(Actor503500* arg0, s32 arg1,
                               Actor503500AnimPreset* arg2, s32 arg3);
/// `func_800B4114` is deliberately declared locally with a signed `arg2`; see
/// the note in `include/gameplay/1BC.h`.
void func_800B4114(GpAnimCtx* arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
/// Animation-bank table indexed by `Actor503500AnimPreset::field_0`.
extern void* D_actor_503500_8016EAB8[];
s32          func_actor_503500_80136014(Actor503500* arg0, s32 arg1);
void         func_actor_503500_8013611C(s32 arg0);
void         func_actor_503500_80135828(Actor503500* arg0, s8* arg1);
void         func_actor_503500_801372AC(s32 arg0);
void         func_actor_503500_80136450(Actor503500* arg0);
void         func_actor_503500_801369E4(Actor503500* arg0);
void         func_actor_503500_80136A80(Actor503500* arg0);
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

/// `TaskDesc` table `func_actor_503500_80132F64` and
/// `func_actor_503500_80135D00` spawn slot enemies from.
extern TaskDesc D_actor_503500_8016E924;
/// Initial position of the boss's collision node, copied into both the
/// enemy's `field_1C` and `field_5D4`'s `field_10/12/14`.
extern SVECTOR  D_actor_503500_8016EC50;
extern TaskDesc D_actor_503500_8016E9F0;
/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132F64`.
extern GpMsgEntry D_actor_503500_8016EA2C[];
extern GpPairSrcE D_actor_503500_8016E7EC[];
void              func_actor_503500_80136228(Actor503500* arg0);

/// State-0 init of the boss: clears and seeds its work block, links the
/// second body part's display node, spawns slot enemies 1..11 from
/// `D_actor_503500_8016E924` (tinting each from the current area record, as
/// `func_actor_503500_80135D00` does) and applies preset 0x7D3.
void func_actor_503500_80132F64(Actor503500* arg0)
{
    GpAreaKey      key;
    GpAreaKey*     sessionKey;
    u8             areaByte0;
    GpAreaRec*     rec;
    GpCdRec10*     entry;
    GpEnemy*       child;
    TmdObject*     model;
    u32            raw;
    s32            idx;
    s32            i;
    TmdObject*     tmd;
    GpEnemy*       enemy;
    GsCOORDINATE2* coord;
    GsCOORDINATE2* part;
    GpRec18*       recs;
    /* Kept in a register across the spawn loop: the ROM stores enemies[0]
       through the same base rather than rebuilding the address. */
    Actor503500Work* work = &D_actor_503500_80176574;

    tmd   = arg0->extra;
    enemy = arg0->field_20;
    coord = tmd->field_8;
    Mem_Set(work, 0, 0x7E8);
    arg0->field_1C     = work;
    work->field_7D5    = -1;
    work->field_7D6    = -1;
    work->field_7D9    = -1;
    work->field_7A4    = 0x80000;
    work->field_6C4.vx = coord->coord.t[0] << 16;
    work->field_6C4.vy = coord->coord.t[1] << 16;
    work->field_6C4.vz = coord->coord.t[2] << 16;
    work->field_7D8    = 1;
    work->field_7CA    = 0x5A;
    tmd->field_1C      = &work->lightMtx;
    tmd->field_20      = &work->colorMtx;
    tmd->field_E       = 0x14;
    coord->flg         = 0;

    enemy->field_4       = &coord->coord;
    part                 = &coord[3];
    enemy->field_48      = 0;
    enemy->field_18      = part;
    enemy->node.field_4 |= 9;
    enemy->field_1C.vx   = D_actor_503500_8016EC50.vx;
    enemy->field_1C.vy   = D_actor_503500_8016EC50.vy;
    enemy->field_1C.vz   = D_actor_503500_8016EC50.vz;
    recs                 = work->rec5F4;
    enemy->field_50      = &D_actor_503500_8016E7EC[arg0->spawnArg1];
    enemy->field_54      = (s32)recs;
    enemy->field_40      = enemy->field_50->field_4;

    work->field_5D4.field_8  = part;
    work->field_5D4.field_C  = recs;
    work->field_5D4.field_18 = 0x30023;
    work->field_5D4.field_1C = 0x258;
    work->field_5D4.flags    = 1;
    work->field_5D4.field_10 = D_actor_503500_8016EC50.vx;
    work->field_5D4.field_12 = D_actor_503500_8016EC50.vy;
    work->field_5D4.field_14 = D_actor_503500_8016EC50.vz;
    Gp_LinkObj(2, &work->field_5D4);
    Gp_InitRec18Table(recs, 8, 0);
    work->field_6E4.field_4 = 0x600;
    work->field_6E4.field_0 = part;
    work->field_6E4.field_6 = 3;
    work->field_5D4.flags  &= 0x7FFF;

    for (i = 1; i < 12; i++) {
        child = Gp_SpawnEnemyFromTable(&D_actor_503500_8016E924, i, i, enemy);
        if (child != NULL) {
            sessionKey  = (GpAreaKey*)&Game_Session->field_4;
            raw         = arg0->field_20->field_8;
            model       = (TmdObject*)child->task->extra;
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
            work->enemies[i] = child;
        }
    }
    work->enemies[0] = enemy;
    ((void (*)(s32))Gp_IncStateF0Ref)(0x23);
    func_actor_503500_80136B64(arg0, 1, 0);
    for (i = 17; i >= 0; i--) {
        D_actor_503500_80176D64[i] = 0;
    }
    func_actor_503500_80135950(arg0, 0x7D3, D_actor_503500_8016EAC0, 0);
    arg0->exitCallback = (TaskFunc)func_actor_503500_80136228;
    arg0->field_24     = D_actor_503500_8016EA2C;
    arg0->state       += 1;
}

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

/// Main-executable matrix the scene tracks (`acropolis_forked_road.h` calls it
/// the camera target); no module header owns it yet.
extern MATRIX* D_80073B8C;

extern u32 Gp_LcgState;

/// Per-frame upkeep: ticks the `field_752` slot counters down to 0 while the
/// boss is in state 0, rolls `field_7C8` from `Gp_LcgState`, stores the yaw to
/// `D_80073B8C` (offset by `field_7D2`, wrapped into [-0x800, 0x800)) in
/// `field_7B8`, and when `field_7CC` runs out links or unlinks `field_20`'s
/// node per `field_7E2`.
void func_actor_503500_801334CC(Actor503500* arg0)
{
    Actor503500Work* work;
    GsCOORDINATE2*   coord;
    GpEnemy*         enemy;
    SVECTOR          vec;
    s16              angle;
    s16              count;
    s16*             p;
    s32              i;

    work = arg0->field_1C;
    p    = work->field_752;
    if (work->field_7B0 == 0) {
        for (i = 0; i < 0x11; i++, p++) {
            if (--*p < 0) {
                *p = 0;
            }
        }
    }
    Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
    work->field_7C8 = Gp_LcgState >> 16;
    coord           = arg0->extra->field_8;
    vec.vx          = D_80073B8C->t[0] - coord->coord.t[0];
    vec.vy          = 0;
    vec.vz          = D_80073B8C->t[2] - coord->coord.t[2];
    angle           = work->field_7D2 + ratan2(vec.vx, vec.vz);
    while (angle >= 0x800) {
        angle -= 0x1000;
    }
    while (angle < -0x800) {
        angle += 0x1000;
    }
    work->field_7B8 = angle;
    enemy           = arg0->field_20;
    count           = --work->field_7CC;
    if (count < 0) {
        work->field_7CC = 0;
    } else if (count == 0) {
        if (work->field_7E2 != 0) {
            work->field_5D4.flags |= 0x8000;
            Gp_LinkNode(&enemy->node);
        } else {
            work->field_5D4.flags &= 0x7FFF;
            Gp_UnlinkNode(&enemy->node);
        }
    }
}

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

/// Script step pairing `func_actor_503500_80133D40` and `_80133FD8`. State 0
/// runs the one the sign of `field_7BA` picks; the fallback to the other one
/// needs `field_7BA` beyond +/-0x76C on the opposite side, which that sign
/// rules out, so it never fires. `field_7D2` becomes 0x7D0 when the result
/// is 0, else 0. State 1 bumps `field_7BE`
/// unless `field_7C2` is slot 7 or 8, and once that slot is done or 0x5B
/// frames have passed returns a per-slot delay (0xF, 0x3C, 0x5A or 0x1E);
/// until then it returns 0. Any other state returns 1.
s32 func_actor_503500_80134284(Actor503500* arg0, Actor503500Work* work)
{
    s32 ret;
    s32 dir;

    dir = work->field_7BA;
    switch ((s8)work->field_7DB) {
        case 0:
            if (dir > 0) {
                ret = func_actor_503500_80133D40(arg0, work);
                if (ret == 1 && dir < -0x76C) {
                    ret = func_actor_503500_80133FD8(arg0, work);
                }
            } else {
                ret = func_actor_503500_80133FD8(arg0, work);
                if (ret == 1 && dir > 0x76C) {
                    ret = func_actor_503500_80133D40(arg0, work);
                }
            }
            if (ret == 0) {
                work->field_7D2 = 0x7D0;
            } else {
                work->field_7D2 = 0;
            }
            break;
        case 1:
            ret = 0;
            if ((u16)work->field_7C2 - 7 >= 2U) {
                work->field_7BE++;
            }
            if (func_actor_503500_80136FA8(work, work->field_7C2) != 0 || work->field_7BE > 0x5A) {
                switch (work->field_7C2) {
                    case 2:
                    case 3:
                        ret = 0xF;
                        break;
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                        ret = 0x3C;
                        break;
                    case 7:
                    case 8:
                        ret = 0x5A;
                        break;
                    default:
                        ret = 0x1E;
                        break;
                }
            }
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

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

/// Three-step state of the boss block. Step 0 saves the coordinate's rotation
/// and seeds the Y scale to 0x1000; step 1 counts 0x1F frames, then applies
/// animation preset 0xE; step 2 restores the rotation every frame, squashes it
/// in Y down to 0x200 and fires the light / effect cues at frames 0x3C, 0x46
/// and 0x64. From step 2 on, preset 0xE is reapplied whenever
/// `func_actor_503500_80136014` reports it.
void func_actor_503500_80134A24(Actor503500* arg0)
{
    Actor503500Work* work;
    GpEnemy*         enemy;
    TmdObject*       obj;
    GsCOORDINATE2*   coord;
    s32*             src;
    s32*             dst;
    s32              i;
    VECTOR           scale;

    coord = arg0->extra->field_8;
    obj   = arg0->extra;
    work  = arg0->field_1C;
    enemy = arg0->field_20;
    switch ((s8)work->field_7DA) {
        case 0:
            work->field_7BC = 0;
            work->field_7CE = 0x1000;
            dst             = work->field_77C;
            src             = (s32*)coord->coord.m;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            work->field_78C = coord->coord.m[2][2];
            work->field_7DA = work->field_7DA + 1;
            break;
        case 1:
            if (++work->field_7BC >= 0x1F) {
                func_actor_503500_80135FB4(arg0, 0xE, 0x20);
                work->field_7BC = 0;
                work->field_7DA = work->field_7DA + 1;
            }
            break;
        case 2:
            if (work->field_7CE > 0x200) {
                work->field_7CE -= 0x10;
            }
            dst = (s32*)coord->coord.m;
            src = work->field_77C;
            for (i = 0; i < 4; i++) {
                *dst++ = *src++;
            }
            coord->coord.m[2][2] = work->field_78C;
            scale.vx             = 0x1000;
            scale.vy             = work->field_7CE;
            scale.vz             = 0x1000;
            ScaleMatrixL(&coord->coord, &scale);
            coord->flg = 0;
            switch (++work->field_7BC) {
                case 0x3C:
                    obj->field_C |= 2;
                    Gp_SetLightMode((GpObj4C*)enemy, 1);
                    break;
                case 0x46:
                    Gp_SpawnEff(0x600A5, coord, 1, NULL);
                    break;
                case 0x64:
                    Gp_SetLightMode((GpObj4C*)enemy, 2);
                    break;
            }
            break;
    }
    if ((s8)work->field_7DA >= 2 && func_actor_503500_80136014(arg0, 0xE) != 0) {
        func_actor_503500_80135FB4(arg0, 0xE, 0x20);
    }
}

void func_actor_503500_80134C68(Actor503500* arg0)
{
    Actor503500Work* work;
    Task*            task;
    GsCOORDINATE2*   coord;

    work = arg0->field_1C;
    if ((u16)(work->field_7B0 - 2) < 3U) {
        func_actor_503500_80136F40(work, 0, 0, 0);
        return;
    }
    switch (work->field_7E1) {
        case 0:
            func_actor_503500_80135FB4(arg0, 4, 0x10);
            func_actor_503500_80137074(arg0, 1, 0x4B);
            work->field_7E1++;
            break;
        case 1:
            if (func_actor_503500_80136014(arg0, 4) != 0) {
                func_actor_503500_80135FB4(arg0, 5, 0x10);
                task = Task_SpawnFromTable(&D_actor_503500_8016E9F0, 4, 0x5A, (s32)arg0);
                if (task != NULL) {
                    coord             = ((TmdObject*)task->extra)->field_8;
                    coord->sub        = &arg0->extra->field_8[3];
                    coord->coord.t[0] = D_actor_503500_8016EC50.vx;
                    coord->coord.t[1] = D_actor_503500_8016EC50.vy;
                    coord->coord.t[2] = D_actor_503500_8016EC50.vz;
                }
                D_80071090      = 1;
                work->field_7C0 = 0;
                work->field_7E1++;
            }
            break;
        case 2:
            if (++work->field_7C0 >= 0x5B) {
                func_actor_503500_80135FB4(arg0, 6, 0x10);
                work->field_7C0 = 0;
                work->field_7E1++;
            }
            break;
        case 3:
            if (++work->field_7C0 >= 0x33) {
                func_actor_503500_80135FB4(arg0, 7, 0);
                work->field_7E1++;
            }
            break;
        case 4:
            if (func_actor_503500_80136014(arg0, 7) != 0) {
                func_actor_503500_80135FB4(arg0, 8, 0);
                func_actor_503500_80137074(arg0, 0, 0xE);
                work->field_7E1++;
            }
            break;
        case 5:
            if (func_actor_503500_80136014(arg0, 8) != 0) {
                work->field_7E0 = 0;
                D_80071090      = 0;
                func_actor_503500_80135F9C((Task*)arg0, 0, 0);
                func_actor_503500_80135FB4(arg0, 0, 0);
            }
            break;
    }
}

/// Applies this frame's hits from the collision records `arg2[0..arg3)` to
/// the boss. Each attack id is taken once, and only type-2 ids land while the
/// `field_7B4` countdown is clear: the damage scales with the attacker's
/// distance, `Gp_RollEnemyChance` can quadruple it, and a hit that empties
/// `field_40` starts the death state instead of the id's status effect. `arg1`
/// is passed by the caller but unused.
void func_actor_503500_80134EAC(Actor503500* arg0, GpObj* arg1, GpRec18* arg2, s32 arg3)
{
    VECTOR           d;
    SVECTOR          pos;
    MATRIX           mtx;
    Actor503500Work* work;
    GpEnemy*         enemy;
    GsCOORDINATE2*   coord;
    GsCOORDINATE2*   src;
    s16              stun;
    u32              id;
    s32              dmg;
    s32              i;
    s32              j;

    enemy = arg0->field_20;
    work  = arg0->field_1C;
    coord = arg0->extra->field_8;
    for (i = 0; i < arg3; i++) {
        id = arg2[i].field_4;
        for (j = 0; j < i; j++) {
            if (arg2[j].field_4 == id) {
                goto next;
            }
        }
        if ((id & 0xFFFF0000) == 0x10000) {
            continue;
        }
        if ((id & 0xFFFF0000) != 0x20000) {
            continue;
        }
        if (work->field_7B4 != 0) {
            continue;
        }
        src = Gp_ActorSlots[(id >> 7) & 1]->extra->field_8;
        Gp_ComposeParentWorld(coord, &mtx, &pos);
        d.vx = src->coord.t[0] - pos.vx;
        d.vy = src->coord.t[1] - pos.vy;
        d.vz = src->coord.t[2] - pos.vz;
        dmg  = Gp_ComputeDamage(id, SquareRoot0(d.vx * d.vx + d.vy * d.vy + d.vz * d.vz), 0, 0);
        if (Gp_RollEnemyChance(enemy, id, 0) != 0) {
            dmg *= 4;
            Gp_SpawnEff(0x6009C, coord, 0, NULL);
        }
        func_800E2C78((GpObj40*)enemy, id, dmg, 0);
        enemy->field_40 -= dmg;
        func_800DA6E8(&enemy->node, dmg, 0);
        if (enemy->field_40 <= 0) {
            func_actor_503500_80136EFC(arg0, 4);
            work->field_7E6 = 1;
        } else {
            switch (Gp_GetIdParam0(id) & 0xFFFF) {
                case 0:
                case 4:
                case 5:
                case 6:
                case 7:
                case 8:
                case 9:
                    break;
                case 1:
                    Gp_SetObjFlag1((GpObj4C*)enemy);
                    break;
                case 2:
                    Gp_SetObjFlag2((GpObj5D*)enemy, id, 0);
                    break;
                case 3:
                    Gp_SetObjFlag4((GpObj5C*)enemy, id, 0);
                    break;
            }
        }
        func_800FDB18(Gp_GetIdParam1(id) & 0xFFFF, coord, NULL, &work->field_6E4);
        stun = Gp_GetIdParam2(id);
        if (work->field_7B4 < stun) {
            work->field_7B4 = stun;
        }
    next:;
    }
}

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

/// Applies preset `arg2`: re-seeds the slot array from bank `field_0` when it
/// changes, then sets every slot 1..0x13 to clip `field_4` (blended over
/// `field_C` frames by `func_800B4114` when `field_8` is set and the array was
/// already seeded) and ticks it once, before re-applying the part scales.
s32 func_actor_503500_80135950(Actor503500* arg0, s32 arg1, Actor503500AnimPreset* arg2, s32 arg3)
{
    Actor503500Work* work;
    Actor503500Work* work2;
    TmdObject*       ext;
    s32              i;

    work = arg0->field_1C;
    ext  = arg0->extra;
    if (arg2->field_0 != work->field_7D6) {
        work->field_7D6 = arg2->field_0;
        func_800B3F84((GpAnimCtx*)work, D_actor_503500_8016EAB8[work->field_7D6], (GpAnimObj*)ext,
                      work->field_334, (GpAnimSlot*)&work->obj.field_14);
        work->field_7D4 = 0;
    }
    work->field_7D5 = arg2->field_4;
    if (arg2->field_8 != 0 && work->field_7D4 != 0) {
        for (i = 1; i < 0x14; i++) {
            func_800B4114((GpAnimCtx*)work, i, work->field_7D5, 0, arg2->field_C);
        }
    } else {
        for (i = 1; i < 0x14; i++) {
            Gp_AnimResetSlot((GpAnimCtx*)work, i, work->field_7D5);
        }
    }
    for (i = 1; i < 0x14; i++) {
        Gp_AnimTickIndex((GpAnimCtx*)work, i);
    }
    work->field_7D4 = 1;
    work2           = arg0->field_1C;
    if (work2->field_7AC & 0x20) {
        work2->coord504 = arg0->extra->field_8[4];
        ScaleMatrix(&work2->coord504.coord, &work2->field_5A4);
    }
    if (work2->field_7AC & 0x800) {
        work2->coord554 = arg0->extra->field_8[10];
        ScaleMatrix(&work2->coord554.coord, &work2->field_5B4);
    }
    if (work2->field_7AC & 0x10000) {
        ScaleMatrix(&arg0->extra->field_8[16].coord, &work2->field_5C4);
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500_5", func_actor_503500_80135B74);

/// Clears slot `arg1` of the boss work block's `enemies` array. `arg0` is
/// loaded by every caller but the body ignores it, the same way
/// `func_actor_503500_80135E04` does.
void func_actor_503500_80135CE8(Task* arg0, s32 arg1)
{
    D_actor_503500_80176574.enemies[arg1] = NULL;
}

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

/// Sets the scale of boss part `arg1` (5, 11 or 16) from `arg2` and marks it
/// in `field_7AC` for `func_actor_503500_80136DDC`. Parts 5 and 11 also seed
/// the private copy of model part 4 / 10 and link it from the next part's
/// `sub`; any other `arg1` only sets its bit.
void func_actor_503500_80135E20(Actor503500* arg0, s32 arg1, SVECTOR* arg2)
{
    Actor503500Work* work = &D_actor_503500_80176574;

    switch (arg1) {
        case 5:
            work->field_5A4.vx          = arg2->vx;
            work->field_5A4.vy          = arg2->vy;
            work->field_5A4.vz          = arg2->vz;
            work->coord504              = arg0->extra->field_8[4];
            arg0->extra->field_8[5].sub = &work->coord504;
            break;
        case 11:
            work->field_5B4.vx           = arg2->vx;
            work->field_5B4.vy           = arg2->vy;
            work->field_5B4.vz           = arg2->vz;
            work->coord554               = arg0->extra->field_8[10];
            arg0->extra->field_8[11].sub = &work->coord554;
            break;
        case 16:
            work->field_5C4.vx = arg2->vx;
            work->field_5C4.vy = arg2->vy;
            work->field_5C4.vz = arg2->vz;
            break;
    }
    work->field_7AC |= 1 << arg1;
}

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
