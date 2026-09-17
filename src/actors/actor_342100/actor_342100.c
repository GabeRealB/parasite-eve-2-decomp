#include "common.h"

#include "actors/actor_342100.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/session.h"
#include "main/sound.h"

/// Main-executable global with no module header yet: the remaining-enemy count.
extern s16 D_80073BA0;

/// Main-executable globals with no module header yet: `D_80073BA9` is the base
/// weapon id records are numbered from, and `D_8007218A` selects the alternate
/// set -- 1 means the second block, anything else the `+0x22` one.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Single-entry, type-0 spawn table: `func_actor_342100_80163408` starts entry
/// 0 and hands it the address of `Actor342100Work::field_20`. The task's
/// callback takes that block back through `Task::spawnArg2` and works the
/// surrounding fields -- it reads `field_22` and zeroes the words at +0x4 and
/// +0x6 -- so the seeded pair and the table belong together.
extern TaskDesc D_actor_342100_801648DC;

/// Null-terminated table of the overlay's per-state message tables, counted
/// and reported by `func_actor_342100_80162F54` when it arms the encounter:
/// three live entries and the null word that ends them.
extern s32 D_actor_342100_80164900[];

/// Animation step table `func_actor_342100_801629B8` walks: `s16` entries
/// holding the anim id one step on from `field_3C`, sent as the message's
/// second word with `0x2F` added; the first three entries are `-1`, which ends
/// the chain, and only the fourth is live. Sits directly after
/// `D_actor_342100_80164900`'s null word, and its first element is the address
/// `func_actor_342100_80162F54`'s encounter table of a different size would
/// have started at, so splat cut it out as a symbol of its own.
extern s16 D_actor_342100_80164910[];

/// Model/animation set `func_actor_342100_80162F54` installs with
/// `func_800E8614` on the same arm; a byte address is all the installer sees.
extern u8 D_actor_342100_801649C8[];

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80161E70);

INCLUDE_RODATA("actors/nonmatchings/actor_342100/actor_342100", D_actor_342100_80161E20);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162748);

/// Advance the encounter's animation one step: the work block's `field_2C` is
/// queried with 0x3ED and a non-zero answer stops the chain with 0; `field_3C`
/// is range-checked against 0x2F (the first anim id the table can name) and the
/// table's entry shifted up by 0x2F, a negative entry ending it with 1 as well.
/// The step that survives re-sends `GpAnimArg {setId, anim, 1, 0xA, 0}` as
/// message 0x3E8 -- `func_actor_342100_8016334C`'s tail with `field_C` = 0xA --
/// to the same target, and reports 1.
///
/// The `ret1` label and the `goto` are load-bearing, not leftovers. Every
/// `return 1;` compiles to `[v0=1][use v0][j return_label][barrier]`, and the
/// last `jump_optimize` pass (the one that runs after reload, `jump.c`'s
/// cross-jumping) merges those identical blocks into one: the branch that
/// dangled over the first of them is then rewritten to point at the survivor,
/// which costs the function an extra `j` and moves the block. Leaving the last
/// table check a bare `j ret1` instead keeps its branch a plain jump over an
/// unconditional jump, and `COMPILER_BARRIER()` after the label keeps an active
/// insn between the first branch and the jump it dangles over, so neither
/// branch is inverted. Both are needed for the target's shape; see
/// DECOMPILATION_LEARNINGS.md, "Several identical `return <const>;` blocks".
s32 func_actor_342100_801629B8(Task* arg0)
{
    Actor342100Work* work;
    Actor342100Work* w;
    GpAnimArg        msg;
    s16              anim;
    s32              weaponId;
    s32              setId;

    work = (Actor342100Work*)arg0->idMap;
    if (work->field_2C == NULL) {
    ret1:
        COMPILER_BARRIER();
        return 1;
    }
    if (Gp_DispatchMsg(work->field_2C, 0x3ED, 0, 0) != 0) {
        return 0;
    }
    if (work->field_3C < 0x2F) {
        return 1;
    }
    if (D_actor_342100_80164910[work->field_3C - 0x2F] < 0) {
        goto ret1;
    }
    anim         = (u16)D_actor_342100_80164910[work->field_3C - 0x2F] + 0x2F;
    w            = (Actor342100Work*)arg0->idMap;
    weaponId     = D_80073BA9;
    setId        = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0  = (void*)setId;
    w->field_3C  = anim;
    msg.field_4  = anim;
    msg.field_8  = 1;
    msg.field_C  = 0xA;
    msg.field_10 = 0;
    Gp_DispatchMsg(w->field_2C, 0x3E8, (s32)&msg, 0);
    return 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162AB0);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162C88);

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80162DDC);

/// First tick of the overlay's event/controller task, the one that arms the
/// encounter as state 0 and then waits for the player's arrival as state 1.
///
/// State 0 counts the live entries of the overlay's message-table list and
/// hands slot 3 that list with message 0x3F7, lets the player's weapon into
/// the message stream (`Gp_MsgPlayerWeapon`), raises the `Gp_StateC08` flag
/// `func_800A7DB8` gates on, installs the model set and hands slot 6 the
/// 0xFA4 that starts the encounter, then starts spawn entry 2 with the task
/// itself and steps to state 1. State 1 ticks the child and reports 1 to keep
/// the task alive until `Game_Session->field_1` is set.
s32 func_actor_342100_80162F54(Task* arg0)
{
    Actor342100Work*  work = (Actor342100Work*)arg0->idMap;
    Actor342100Work*  msgWork;
    Actor342100Msg3F7 msg;
    s32               n;

    switch (work->field_3E) {
        case 0:
            msgWork = (Actor342100Work*)arg0->idMap;
            n       = 0;
            while (D_actor_342100_80164900[n & 0xFFFF] != 0) {
                n += 1;
            }
            msg.table = &D_actor_342100_80164900[0];
            msg.count = n & 0xFFFF;
            Gp_DispatchMsg(msgWork->field_2C, 0x3F7, (s32)&msg, 0);
            Gp_MsgPlayerWeapon(0);
            Gp_StateC08.field_6 |= 1;
            func_800E8614((s32)&D_actor_342100_801649C8, 0);
            Gp_DispatchMsg(Game_GetPtrSlot(6), 0xFA4, 0, 0);
            work->field_34 = Task_SpawnFromTable(&D_actor_342100_80164B78, 2, 0, (s32)arg0);
            work->field_3E = work->field_3E + 1;
            break;
        case 1:
            if (Game_Session->field_1 != 0) {
                break;
            }
            return 1;
    }
    func_actor_342100_801629B8(arg0);
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_801630A4);

void func_actor_342100_80163344(Actor342100* arg0, s32 arg1, s32 arg2)
{
    arg0->field_30 = arg2;
}

/// Point the overlay's slot-3 task at the animation set `arg0 + 0x2F` and hand
/// the work block's `field_3C` the same value, then install the set with
/// message 0x3E8. The set's block is `D_80073BA9 + 1` under the alternate
/// weapon configuration and `D_80073BA9 + 0x22` otherwise; its `field_4` is the
/// same halfword the block keeps, `field_8` is 1 and `field_C` 0xF.
void func_actor_342100_8016334C(s32 arg0)
{
    Actor342100Work* work;
    GpAnimArg        msg;
    s16              anim;
    s32              weaponId;
    s32              setId;

    work           = (Actor342100Work*)D_actor_342100_80164BB8->idMap;
    anim           = arg0 + 0x2F;
    weaponId       = D_80073BA9;
    setId          = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0    = (void*)setId;
    work->field_3C = anim;
    msg.field_4    = anim;
    msg.field_8    = 1;
    msg.field_C    = 0xF;
    msg.field_10   = 0;
    Gp_DispatchMsg(work->field_2C, 0x3E8, (s32)&msg, 0);
}

void func_actor_342100_801633D0(s32 arg0)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->idMap;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_actor_342100_80163408(void)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->idMap;

    work->field_20 = 0x258;
    work->field_22 = 0x100;
    Task_SpawnFromTable(&D_actor_342100_801648DC, 0, 0, (s32)&work->field_20);
}

/// Entry/exit of the overlay's spawned child. A zero arm plays the cue, asks
/// slot 4 to forward message 0x7DB with the `{ 0, 0x2C, 4 }` record, passes the
/// same record on to `field_30` if that target exists, and starts the child at
/// entry 3; a non-zero arm tells the already-spawned child so through its
/// `Task::spawnArg1`.
void func_actor_342100_80163454(s32 arg0)
{
    Actor342100Work*  work = (Actor342100Work*)D_actor_342100_80164BB8->idMap;
    Actor342100Msg7DA msg;

    if (arg0 == 0) {
        SndEvt_EnqueueType6(0x54270005, 0, 0);
        Gp_PulseState1C();
        msg.field_1 = 0x2C;
        msg.field_0 = 0;
        msg.field_2 = 4;
        Gp_DispatchMsg(Game_GetPtrSlot(4), 0x7DA, (s32)&msg, 0x7DB);
        if (work->field_30 != NULL) {
            Gp_DispatchMsg(work->field_30, 0x7DB, (s32)&msg, 0);
        }
        work->field_38 = Task_SpawnFromTable(&D_actor_342100_80164B78, 3, 0, 0);
        return;
    }
    work->field_38->spawnArg1 = 1;
}

void func_actor_342100_80163518(void)
{
    D_80073BA0              = 0;
    Game_Session->field_128 = 3;
}
