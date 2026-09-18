#include "common.h"

#include "actors/actor_342100.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"
#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/wipsys.h"

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

/// Placement tables the overlay's spawn task picks between by
/// `Game_Session->field_4`: 0x1D, 0x1E, 0x1F, 0x23 and 0x24 select the 0x80164930
/// / 0x80164918 / 0x80164948 / 0x80164960 / 0x80164980 table respectively, and
/// the values in between select none. Each is a zero-`vx`-terminated `SVECTOR`
/// list of two to three placements -- the terminator is an all-zero entry -- and
/// `func_actor_342100_80162C88` drops one effect task on every live entry.
extern SVECTOR D_actor_342100_80164918[];
extern SVECTOR D_actor_342100_80164930[];
extern SVECTOR D_actor_342100_80164948[];
extern SVECTOR D_actor_342100_80164960[];
extern SVECTOR D_actor_342100_80164980[];

/// Model/animation set `func_actor_342100_80162F54` installs with
/// `func_800E8614` on the same arm; a byte address is all the installer sees.
extern u8 D_actor_342100_801649C8[];

/// Effect record `func_actor_342100_80162DDC` hands `func_800FDB18` together
/// with one part of the player's model: `field_0` is that part's coordinate
/// and `field_4` the scale that goes with it (0x100 for the wide pick, 0x10
/// for the narrow one). Ships as `{ NULL, 0, 1 }` in the data blob, directly
/// before the part table below.
extern GpEffArg D_actor_342100_801649A0;

/// The player-model parts the effect record above is aimed at, as indices into
/// the player's coordinate array (`TmdObject::field_8`): sixteen `u16`s
/// running 1..0x12, of which `func_actor_342100_80162DDC` takes the first four
/// (2, 4, 6, 0xA) when it masks the LCG draw with 3 and all sixteen when it
/// masks with 0xF.
extern u16 D_actor_342100_801649A8[];

/// Frame counter the narrow arm of `func_actor_342100_80162DDC`'s state 1 is
/// gated on: it aims the effect only on the frames where the low nibble (or,
/// for the other arm, the low three bits) of this global is clear.
extern s32 D_80070F70;

/// Random-number state the overlay's spawn task rolls once per tick: the
/// product's high halfword picks the model part.
extern u32 Gp_LcgState;

INCLUDE_ASM("actors/nonmatchings/actor_342100/actor_342100", func_actor_342100_80161E70);

/// Fade-to-white driver of the encounter, six states over the eight-byte
/// channel block it allocates into its own `Task::work` and hands the parent
/// work block through `Task::spawnArg2`.
///
/// State 0 allocates the ramp, zeroes the three channels and parks the
/// message record `D_actor_342100_801648F8` in `Task::field_24`. States 2 and
/// 3 step `field_2` -- the first by 0xA up to 0x50, the second by 1 up to
/// 0xFF -- and each hands the state machine back to 1 when it clamps, so the
/// two ramps run back to back. State 4 steps `field_4` / `field_6` by 8; once
/// `field_4` passes 0xFF the display mode is switched, `Fs_ImgBuffers` is
/// filled white, the parent work block's `field_24` is raised, and state 5
/// draws the full-screen white `TILE` + `DR_TPAGE` packed into
/// `Gpu_PrimCursor` before returning without the fade call. Every other state
/// -- 1, 6 and up -- only draws the fade.
void func_actor_342100_80162748(Task* arg0)
{
    Actor342100FadeWork* work;
    Actor342100FadeWork* alloc;
    Actor342100Work*     parent;
    TILE*                tile;
    DR_TPAGE*            dr;

    work = (Actor342100FadeWork*)arg0->work;
    switch (arg0->state) {
        case 0:
            alloc      = (Actor342100FadeWork*)Mem_Malloc(8, 0);
            arg0->work = (TaskIdMap*)alloc;
            if (alloc == NULL) {
                Task_Kill(arg0);
                return;
            }
            work           = alloc;
            work->field_6  = 0;
            work->field_4  = 0;
            work->field_2  = 0;
            arg0->field_24 = &D_actor_342100_801648F8;
            arg0->state   += 1;
            break;
        case 2:
            work->field_2 += 0xA;
            if ((s16)work->field_2 >= 0x51) {
                work->field_2 = 0x50;
                arg0->state   = 1;
            }
            break;
        case 3:
            work->field_2 += 1;
            if ((s16)work->field_2 >= 0x100) {
                work->field_2 = 0xFF;
                arg0->state   = 1;
            }
            break;
        case 4:
            work->field_4 += 8;
            work->field_6 += 8;
            if ((s16)work->field_4 >= 0x100) {
                parent           = (Actor342100Work*)((Task*)arg0->spawnArg2)->work;
                parent->field_24 = 2;
                Display_SetMode(0xD010);
                Mem_Set(Fs_ImgBuffers, 0xFF, 0x25800);
                work->field_6 = 0xFF;
                work->field_4 = 0xFF;
                arg0->state   = 5;
            }
            break;
        case 5:
            tile           = (TILE*)Gpu_PrimCursor;
            Gpu_PrimCursor = (DR_TPAGE*)(tile + 1);
            setlen(tile, 3);
            setcode(tile, 0x60);
            tile->r0 = 0xFF;
            tile->g0 = 0xFF;
            tile->b0 = 0xFF;
            tile->x0 = -0xA0;
            tile->y0 = -0x78;
            tile->w  = 0x140;
            tile->h  = 0xF0;
            addPrim(Gpu_CurrentOt - 16, tile);

            dr             = Gpu_PrimCursor;
            Gpu_PrimCursor = dr + 1;
            setlen(dr, 1);
            dr->code[0] = 0xE1000200;
            addPrim(Gpu_CurrentOt - 16, dr);
            return;
    }
    Fade_DrawOverlay((u8)work->field_2, (u8)work->field_4, (u8)work->field_6, 1);
}

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

    work = (Actor342100Work*)arg0->work;
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
    w            = (Actor342100Work*)arg0->work;
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

/// State 0 allocates the overlay's effect record -- eight bytes, scale 0x100,
/// count 1, aimed at the model's root coordinate -- through `arg0->work`,
/// which is also where the null check reads it back: that is what leaves the
/// copy into `eff` after the branch instead of before it. State 1 waits out
/// `spawnArg1` and steps to 2. State 2 runs on every fourth frame, and builds
/// the effect's offset vector out of five LCG rolls: two per signed component
/// (the value from one roll, its sign from the next) plus a third that is
/// always negative. Only the three rolls whose value goes into `Gp_LcgState`
/// are stored, so the two temporary rolls are separate variables -- one `rng`
/// would be a single long-lived pseudo and take a register the constant needs.
///
/// Where `vec.vx = vx` sits is load-bearing. Placed with the last roll it is
/// scheduled past the argument setup, which lengthens `vx`'s live range enough
/// that global-alloc prefers the `0x71357911` constant and hands the component
/// $a2 (99.49%); between the third roll and the `vec.vy` store it stays short
/// and takes $a1, the constant falling to $a2 (100.00%).
void func_actor_342100_80162AB0(Task* arg0)
{
    GpEffArg*      eff;
    GsCOORDINATE2* coord;
    SVECTOR        vec;
    s32            rng;
    s32            rng2;
    s32            vx;
    s32            vz;

    eff   = (GpEffArg*)arg0->work;
    coord = ((TmdObject*)arg0->extra)->field_8;
    switch (arg0->state) {
        case 0:
            arg0->work = (TaskIdMap*)Mem_Malloc(8, 0);
            if (arg0->work == NULL) {
                Task_Kill(arg0);
                return;
            }
            eff = (GpEffArg*)arg0->work;
            Mem_Set(eff, 0, 8);
            eff->field_4 = 0x100;
            eff->field_0 = ((TmdObject*)arg0->extra)->field_8;
            eff->field_6 = 1;
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1 <= 0) {
                arg0->state = 2;
                return;
            }
            arg0->spawnArg1--;
            return;
        case 2:
            if (D_80070F70 & 0xF) {
                return;
            }
            coord->flg = 0;
            Gp_UpdateCoord(coord);
            rng         = Gp_LcgState * 5 + 0x71357911;
            vx          = ((u32)rng >> 16) & 0x3F;
            Gp_LcgState = rng * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                vx = -vx;
            }
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            vec.vx      = vx;
            vec.vy      = -(((u32)Gp_LcgState >> 16) & 0x3F);
            rng2        = Gp_LcgState * 5 + 0x71357911;
            vz          = ((u32)rng2 >> 16) & 0x3F;
            Gp_LcgState = rng2 * 5 + 0x71357911;
            if (((u32)Gp_LcgState >> 16) & 1) {
                vz = -vz;
            }
            vec.vz = vz;
            func_800FDB18(3, coord, &vec, eff);
            return;
    }
}

/// Spawn the encounter's effect tasks: `Game_Session->field_4` selects one of
/// the overlay's placement tables, and every entry in it rolls the LCG once,
/// starts spawn entry 4 (`func_actor_342100_80162AB0`) with the roll's masked
/// high half as its `spawnArg1` -- the lifetime that task's state 1 counts down
/// -- and lays the entry onto the model the new task displays: identity rotation
/// at scale 0x1000 through the `GpMtxWords` view of `coord`, the entry's `vx` /
/// `vy` / `vz` written to `coord.t[0..2]`. The walk is `while (pos->vx != 0)`,
/// so a table is as many entries as it has non-zero `vx`s and a table whose
/// first entry is zero spawns nothing.
///
/// The table pointer is deliberately uninitialised: `Game_Session->field_4`
/// values 0x20..0x22 -- and anything outside the jump table -- leave it holding
/// whatever the caller left in `$s1`, which is the target's shape.
///
/// Referenced from the `0x0D` entry of the command table in
/// `D_actor_342100_801649C8` (+0x90), next to the same-shaped entries naming
/// `func_actor_342100_8016334C` / `func_actor_342100_801633D0` /
/// `func_actor_342100_80163408` / `func_actor_342100_80163454`. That entry
/// passes it no arguments, which is why the declaration is `(void)`.
void func_actor_342100_80162C88(void)
{
    GsCOORDINATE2* coord;
    GpMtxWords*    rot;
    SVECTOR*       pos;
    Task*          task;
    u32            rng;

    switch (Game_Session->field_4) {
        case 29:
            pos = D_actor_342100_80164930;
            break;
        case 30:
            pos = D_actor_342100_80164918;
            break;
        case 31:
            pos = D_actor_342100_80164948;
            break;
        case 35:
            pos = D_actor_342100_80164960;
            break;
        case 36:
            pos = D_actor_342100_80164980;
            break;
    }
    while (pos->vx != 0) {
        rng               = Gp_LcgState * 5 + 0x71357911;
        Gp_LcgState       = rng;
        task              = Task_SpawnFromTable(&D_actor_342100_80164B78, 4, (rng >> 16) & 0x1F, 0);
        coord             = ((TmdObject*)task->extra)->field_8;
        rot               = (GpMtxWords*)&coord->coord;
        rot->w0           = 0x1000;
        rot->w1           = 0;
        rot->w2           = 0x1000;
        rot->w3           = 0;
        rot->h4           = 0x1000;
        coord->coord.t[0] = pos->vx;
        coord->coord.t[1] = pos->vy;
        coord->coord.t[2] = pos->vz;
        pos++;
    }
}

/// Spawn task of the overlay's spawn table (`func_actor_342100_80162748`'s
/// neighbour entry, started with the encounter): each tick rolls the LCG and
/// aims the overlay's effect record at one part of the player's model, taken
/// from the coordinate array `Game_GetPtrSlot(3)`'s display object owns.
///
/// State 0 fires unconditionally -- the wide pick, scale 0x100 -- and steps to
/// state 1. State 1 fires only on a frame the `D_80070F70` gate lets through,
/// and which pick that is depends on the task's `spawnArg1`: the zero arm
/// takes the same four parts as state 0 at scale 0x10, the non-zero arm the
/// whole table at scale 0x100.
///
/// The three arms each spell the aim-and-fire sequence out. That is what the
/// target's shape is: the two state-1 arms are byte-for-byte equal from the
/// table-base `lui` on, so `jump.c`'s cross-jumping (the `jump_optimize` that
/// runs after reload) merges that suffix into one block and leaves each arm
/// its own copy of the address and scale in front of the jump -- the address
/// and scale cannot merge because the scale differs. Folding the arms into one
/// `goto`-shared block instead compiles them into a single copy with a live
/// scale value, which is a different object (95.02%).
void func_actor_342100_80162DDC(Task* arg0)
{
    Task* slot;
    s32   idx;

    slot        = Game_GetPtrSlot(3);
    Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
    idx         = Gp_LcgState >> 16;

    switch (arg0->state) {
        case 0:
            idx                            &= 3;
            D_actor_342100_801649A0.field_4 = 0x100;
            D_actor_342100_801649A0.field_0 = &((TmdObject*)slot->extra)->field_8[D_actor_342100_801649A8[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->field_8, NULL, &D_actor_342100_801649A0);
            arg0->state++;
            return;
        case 1:
            if (arg0->spawnArg1 == 0) {
                if (D_80070F70 & 0xF) {
                    return;
                }
                idx                            &= 3;
                D_actor_342100_801649A0.field_4 = 0x10;
                D_actor_342100_801649A0.field_0 = &((TmdObject*)slot->extra)->field_8[D_actor_342100_801649A8[idx]];
                func_800FDB18(3, ((TmdObject*)slot->extra)->field_8, NULL, &D_actor_342100_801649A0);
                return;
            }
            if (D_80070F70 & 7) {
                return;
            }
            idx                            &= 0xF;
            D_actor_342100_801649A0.field_4 = 0x100;
            D_actor_342100_801649A0.field_0 = &((TmdObject*)slot->extra)->field_8[D_actor_342100_801649A8[idx]];
            func_800FDB18(3, ((TmdObject*)slot->extra)->field_8, NULL, &D_actor_342100_801649A0);
            return;
    }
}

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
    Actor342100Work*  work = (Actor342100Work*)arg0->work;
    Actor342100Work*  msgWork;
    Actor342100Msg3F7 msg;
    s32               n;

    switch (work->field_3E) {
        case 0:
            msgWork = (Actor342100Work*)arg0->work;
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

extern u8       D_80071075;
extern s8       D_80114C11;
extern s8       D_80114C12;
extern u8       D_80114CF8;
extern u8       D_801153F4;
extern TaskDesc D_8018B57C;
extern TaskDesc D_8018B83C;

/// The overlay's event/controller task. Idles while the session or any of
/// the global pause flags hold it. State 0 allocates the work block and
/// publishes the task, then picks state 1 or 2 from
/// `Game_Session->unknown_130[0]`; state 1 waits on flag 0x11E and pending
/// object 5, and states 1 and 2 both move to 3 once `field_120` has dropped
/// to zero while the player still has HP. State 3 ticks
/// `func_actor_342100_80162F54` until it reports done.
///
/// `work` is read from `work` before state 0 replaces it, so the two
/// `field_30` stores go through the block the task held on entry.
void func_actor_342100_801630A4(Task* arg0)
{
    u16              id;
    s8               kind;
    u8               extra;
    Actor342100Work* work;
    Actor342100Work* newWork;
    s32              ready;
    PlayerStatus*    cfg;

    work = (Actor342100Work*)arg0->work;
    if (Game_Session->field_65 != 0 || D_80114C11 != 0 || D_801153F4 != 0 || D_80114CF8 != 0) {
        return;
    }
    switch (arg0->state) {
        case 0:
            if (D_80114C12 == 1 || D_80071075 != 0) {
                break;
            }
            newWork    = Mem_Malloc(0x44, 0);
            arg0->work = (TaskIdMap*)newWork;
            if (newWork == NULL) {
                Task_Kill(arg0);
            } else {
                Mem_Set(newWork, 0, 0x44);
                newWork->field_2C       = Game_GetPtrSlot(3);
                D_actor_342100_80164BB8 = arg0;
            }
            Task_SpawnFromTable(&D_8018B57C, 0, 0xD0, 0);
            SndEvt_EnqueueType6(0x54270007, 0, 0);
            switch ((u8)Game_Session->unknown_130[0]) {
                case 0:
                    arg0->state++;
                    break;
                case 1:
                    work->field_30 = Task_SpawnFromTable(&D_8018B83C, 0, 1, 0);
                default:
                    arg0->state = 2;
                    break;
            }
            break;
        case 1:
            if (GameFlag_GetNibble(0x11E) != 0) {
                if (Gp_TakePendingObj4C(&id, (u8*)&kind, &extra) != 0 && (id & 0x7FFF) == 5 && kind == 1) {
                    work->field_30 = Task_SpawnFromTable(&D_8018B83C, 0, 0, 0);
                    arg0->state++;
                }
            }
            cfg = &Player_Status;
            if (Game_Session->field_120 > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 2:
            cfg = &Player_Status;
            if (Game_Session->field_120 > 0 || cfg->hp <= 0) {
                ready = 0;
            } else {
                ready = 1;
            }
            if (ready) {
                arg0->state = 3;
            }
            break;
        case 3:
            if ((s16)func_actor_342100_80162F54(arg0) != 0) {
                arg0->state++;
            }
            break;
        case 4:
            break;
    }
}

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

    work           = (Actor342100Work*)D_actor_342100_80164BB8->work;
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
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->work;

    Gp_DispatchMsg(work->field_34, 0x7DB, arg0, 0);
}

/// Seed the spawn entry's two parameters and start the task that consumes
/// them, passing the block itself as `Task::spawnArg2`.
void func_actor_342100_80163408(void)
{
    Actor342100Work* work = (Actor342100Work*)D_actor_342100_80164BB8->work;

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
    Actor342100Work*  work = (Actor342100Work*)D_actor_342100_80164BB8->work;
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
