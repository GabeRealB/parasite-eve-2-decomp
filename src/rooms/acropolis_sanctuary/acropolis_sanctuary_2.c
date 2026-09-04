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
extern u8  D_80073BA9;
extern u8  D_80071075;
extern s16 D_80071076;
extern s8  D_8007218A;
extern s8  D_80114C12;

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
extern s32 D_acropolis_sanctuary_801820E4;
extern s32 D_acropolis_sanctuary_801820F0;
extern s32 D_acropolis_sanctuary_801821C8;

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017D9E8);

/// The sanctuary cutscene task. State 0 allocates the task's `AcsCutsceneWork`
/// block, captures slot 3 in it, publishes the task itself in
/// `D_acropolis_sanctuary_80186C90` and cues the scene: slot 3 is sent the
/// 0x3E8 weapon record for the equipped weapon, the scene's sound event is
/// enqueued and its script pair is started.
///
/// State 1 drives the scene. `GameSession::field_1` reaching 0 instead stops
/// the sound, writes the room's exit into the save and hands off to task 0x11
/// before the task kills itself. Otherwise the scene fires exactly
/// once, when `func_acropolis_sanctuary_8017DCE0` has armed `phase` at 2 and
/// `step` is still 0: the player's effects are dropped, slot 3 is given the
/// 0x3F4 record and then warped to the scene's mark with a 0x3E9 placement, and
/// `step` is bumped so the next frame does nothing.
void func_acropolis_sanctuary_8017DA40(Task* arg0)
{
    AcsMsgArg        weapon;
    AcsMsgArg        rec;
    AcsMsgArg*       msg;
    AcsCutsceneWork* work;
    AcsCutsceneWork* cutscene;
    AcsCutsceneWork* slot;
    AcsCutsceneWork* target;
    s32              state;
    s32              idx;
    s32              weaponId;

    state = arg0->state;
    switch (state) {
        case 0:
            if (D_80114C12 != 1 && D_80071075 == 0) {
                work        = Mem_Calloc(0xC, 0);
                arg0->idMap = (TaskIdMap*)work;
                if (work == NULL) {
                    Task_Kill(arg0);
                } else {
                    Mem_Set(work, 0, 0xC);
                    work->target                   = Game_GetPtrSlot(3);
                    D_acropolis_sanctuary_80186C90 = arg0;
                }
                slot     = (AcsCutsceneWork*)arg0->idMap;
                weaponId = D_80073BA9;
                idx      = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;

                weapon.rec.field_0  = idx;
                weapon.rec.field_4  = 1;
                weapon.rec.field_8  = 1;
                weapon.rec.field_C  = 0xF;
                weapon.rec.field_10 = 0;
                Gp_DispatchMsg(slot->target, 0x3E8, (s32)&weapon, 0);
                SndEvt_EnqueueType6(0x510C0007, 0, 0);
                func_800E8634((s32)&D_acropolis_sanctuary_801820F0, 0, (s32)&D_acropolis_sanctuary_801821C8);
                arg0->state = arg0->state + 1;
            }
            break;

        case 1:
            if (Game_Session->field_1 == 0) {
                SndEvt_EnqueueType7(0x80000000, 0);
                Mc_SaveData.field_6 = 0xD;
                Mc_SaveData.field_7 = 1;
                Mc_SaveData.field_8 = 2;
                Mc_SaveData.field_5 = 1;
                D_80071076          = 1;
                Task_Spawn(0, 0x11, 0, 0);
                Task_Kill(arg0);
                break;
            }
            cutscene = (AcsCutsceneWork*)arg0->idMap;
            msg      = &rec;
            switch (cutscene->phase) {
                case 0:
                case 1:
                    break;
                case 2:
                    if (cutscene->step == 0) {
                        Gp_KillPlayerEffs();
                        target = (AcsCutsceneWork*)arg0->idMap;
                        if (target->target != NULL) {
                            rec.rec.field_0   = (s32)&D_acropolis_sanctuary_801820E4;
                            rec.rec.field_4   = 0;
                            rec.rec.field_8   = 0;
                            msg->rec.field_C  = 0xF;
                            msg->rec.field_10 = 1;
                            Gp_DispatchMsg(target->target, 0x3F4, (s32)msg, 0);
                        }
                        rec.place.pos.vx  = -0x1DB0;
                        rec.place.pos.vy  = 0;
                        msg->place.pos.vz = -0x1130;
                        rec.place.rot.vx  = 0;
                        rec.place.rot.vy  = 0;
                        rec.place.rot.vz  = 0;
                        Gp_DispatchMsg(cutscene->target, 0x3E9, (s32)msg, 0);
                        Mc_SaveData.field_4 = 0xE;
                        cutscene->step      = cutscene->step + 1;
                    }
                    break;
            }
            break;
    }
}

/// Request entry point for the sanctuary cutscene task's work block: 0 and 1
/// arm the script at phase 1 or 2 respectively, rewinding `step` so the driver
/// runs the scene once, while 2 just plays the pair of sound events the scene
/// is cued with.
void func_acropolis_sanctuary_8017DCE0(s32 arg0)
{
    AcsCutsceneWork* work = (AcsCutsceneWork*)D_acropolis_sanctuary_80186C90->idMap;

    switch (arg0) {
        case 0:
            work->phase = 1;
            work->step  = 0;
            return;
        case 1:
            work->phase = 2;
            work->step  = 0;
            return;
        case 2:
            SndEvt_EnqueueType7(0x510C0007, 0);
            SndEvt_EnqueueType6(0x510C0008, 0, 0);
            return;
    }
}

/// Arms the sanctuary's blocker cage: copies the first four normals, eight
/// corners and four quads of the template at `D_acropolis_sanctuary_801822EC`
/// into the live set at `D_acropolis_sanctuary_80183568`, marking every copied
/// quad live, then slides all eight corners to where the cage belongs. Nibble 6
/// is the sanctuary cutscene flag: before the scene the cage sits across the
/// doorway, afterwards it is pushed 3000 units aside and out of the way.
void func_acropolis_sanctuary_8017DD78(void)
{
    AcsBlockerSet*  dst = &D_acropolis_sanctuary_80183568;
    AcsBlockerSet*  src = &D_acropolis_sanctuary_801822EC;
    AcsBlockerShift shift;
    s32             i;

    for (i = 0; i < 4; i++) {
        dst->normals[i].vx           = src->normals[i].vx;
        dst->normals[i].vy           = src->normals[i].vy;
        dst->normals[i].vz           = src->normals[i].vz;
        dst->corners[i * 2].vx       = src->corners[i * 2].vx;
        dst->corners[i * 2].vy       = src->corners[i * 2].vy;
        dst->corners[i * 2].vz       = src->corners[i * 2].vz;
        dst->corners[(i * 2) + 1].vx = src->corners[(i * 2) + 1].vx;
        dst->corners[(i * 2) + 1].vy = src->corners[(i * 2) + 1].vy;
        dst->corners[(i * 2) + 1].vz = src->corners[(i * 2) + 1].vz;
        dst->edges[i]                = src->edges[i];
        dst->edges[i].field_A        = 1;
    }

    if (GameFlag_GetNibble(6) == 0) {
        shift.vx = 200;
        shift.vy = 0;
        shift.vz = 380;
    } else {
        shift.vx = 3000;
        shift.vy = 0;
        shift.vz = 0;
    }

    for (i = 0; i < 8; i++) {
        dst->corners[i].vx += shift.vx;
        dst->corners[i].vy += shift.vy;
        dst->corners[i].vz += shift.vz;
    }
}

/// Toggles a pair of sprite commands for view `arg1` of the current room:
/// `arg0` zero draws the second command and skips the third, non-zero does the
/// reverse. `Gp_LinkViewSprts` reads `field_4` to decide whether to skip
/// OT-linking each command's prims.
void func_acropolis_sanctuary_8017DF88(s32 arg0, s32 arg1)
{
    GameSession*      g    = Game_Session;
    GameSessionFrom4* sess = (GameSessionFrom4*)&g->field_4;
    GpSprtCmd*        cmd;

    cmd = Gp_SprtTables[sess->field_3 - 1][g->field_74 - 1].field_0[sess->field_2 - 1][(arg1 & 0xFF) - 1].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
        cmd[2].field_4 = 1;
    } else {
        cmd[1].field_4 = 1;
        cmd[2].field_4 = 0;
    }
}

/// State 0 of the sanctuary's effect task: spawns the twelve 0x6008B effects
/// the room is decorated with, six with spawn arg `0x200 + i` and six with
/// `0xA00000 + i`, each anchored to the task's own coordinate and offset by its
/// entry in `D_acropolis_sanctuary_80182774`. It then publishes the task's
/// handler table under pointer slot 5 and advances the state so the spawn runs
/// once. Every frame after that it mirrors the session's stage byte into
/// `D_acropolis_sanctuary_80182770`: 1 while the byte is 0x10, held while it is
/// 0xC, 0 otherwise.
void func_acropolis_sanctuary_8017E00C(Task* task)
{
    GsCOORDINATE2*    coord;
    GameSessionFrom4* sess;
    s32               i;

    coord = ((TmdObject*)task->extra)->field_8;
    if (task->state == 0) {
        for (i = 0; i < 6; i++) {
            Gp_SpawnEff(0x6008B, coord, i + 0x200, &D_acropolis_sanctuary_80182774[i]);
        }
        for (i = 6; i < 12; i++) {
            Gp_SpawnEff(0x6008B, coord, i + 0xA00000, &D_acropolis_sanctuary_80182774[i]);
        }
        task->field_24 = D_acropolis_sanctuary_80182310;
        Game_SetPtrSlot(task, 5);
        D_acropolis_sanctuary_80182770 = 0;
        task->state                    = task->state + 1;
    }
    sess = (GameSessionFrom4*)&Game_Session->field_4;
    if (sess->field_0 == 0x10) {
        D_acropolis_sanctuary_80182770 = 1;
    } else if (sess->field_0 != 0xC) {
        D_acropolis_sanctuary_80182770 = 0;
    }
}

/// State 0 of the sanctuary's mosaic task: spawns one 0x60079 effect per tile,
/// first for all 72 entries of `D_acropolis_sanctuary_80182320` keyed by their
/// own index, then a second pass over the 16 tiles listed in
/// `D_acropolis_sanctuary_80182750` keyed by the tile index itself, so those
/// sixteen get a second effect on top. Each spawn reuses the task's own
/// `GpEffWork` offset triple: x is always 0, y and z come from the tile's grid
/// position scaled by 1145/128 and 2147/256 and shifted by the origin corner of
/// the size class in `quad`. Any state but 0 just releases the work block.
void func_acropolis_sanctuary_8017E134(Task* arg0)
{
    GpEffWork*     mem;
    GsCOORDINATE2* coord;
    AcsTile*       tile;
    s32            quad;
    s32            i;
    s32            idx;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (arg0->state != 0) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    for (i = 0; i < 0x48; i++) {
        tile          = &D_acropolis_sanctuary_80182320[i];
        quad          = tile->quad;
        mem->field_10 = 0;
        mem->field_12 = ((tile->col * 1145) >> 7) - D_acropolis_sanctuary_80182710[quad].corner[0].vy;
        mem->field_14 = -((tile->row * 2147) >> 8) - D_acropolis_sanctuary_80182710[quad].corner[0].vz;
        Gp_SpawnEff(0x60079, coord, i, (SVECTOR*)&mem->field_10);
    }
    for (i = 0; i < 0x10; i++) {
        idx           = D_acropolis_sanctuary_80182750[i];
        tile          = &D_acropolis_sanctuary_80182320[idx];
        quad          = tile->quad;
        mem->field_10 = 0;
        mem->field_12 = ((tile->col * 1145) >> 7) - D_acropolis_sanctuary_80182710[quad].corner[0].vy;
        mem->field_14 = -((tile->row * 2147) >> 8) - D_acropolis_sanctuary_80182710[quad].corner[0].vz;
        Gp_SpawnEff(0x60079, coord, idx, (SVECTOR*)&mem->field_10);
    }
    arg0->state = arg0->state + 1;
}

/// Draws one frame of a whole mosaic tile: a semi-transparent textured quad
/// whose four corners are the size class's own corner offsets, rotated by the
/// task's own `workm` and then projected through `GsWSMATRIX` into an
/// `AcsTileScratch` block taken from `G_SCRATCH_HEAD`. The first corner goes
/// through `rtps` and the other three through `rtpt`; tiles inside `otz` 0x11
/// are dropped. The texture window is the tile's own `row` / `col` origin
/// stretched by its `field_0` / `field_2` extent, so the quad shows its own
/// piece of the mosaic sheet at full size -- this is the intact tile,
/// `func_acropolis_sanctuary_8017EC90` draws the shards it breaks into.
///
/// The drift (`field_10`..`field_14`) and spin (`field_18`..`field_1C`) are
/// seeded from the LCG on the first drawn frame, in one of two strengths
/// chosen by the tile's `field_8`: a fast, wide-tumbling one and a slow one
/// whose life (`field_26`) also gets a random 0..7 bonus. Life is the tile's
/// `field_A`, and `field_22` is the frame counter measured against it -- the
/// tile drifts while it is still young, and the work block is released 0x3C
/// frames past that.
///
/// While drifting, a tile of the fast kind (`field_24` zero) has a 1-in-60
/// chance per frame -- or a certainty once past y = -0xBFF -- of shedding one
/// to four 0x6007A shards, tagged 0x1000 so they spawn as the airborne
/// variant. Crossing x = -0x2740 above y = -0xED7 either shatters a
/// size-class-1 tile into two to four untagged shards or, for size class 0,
/// bounces it by halving and inverting the vertical step. Either split costs
/// 0x64 of life. Once the room flag is set and the session is not in mode
/// 0x10, tiles past x = -0x28C0 also age by 0x3C, so they clear away.
void func_acropolis_sanctuary_8017E338(Task* arg0)
{
    GpEffWork*      mem;
    GsCOORDINATE2*  coord;
    void**          scratch;
    u8*             head;
    AcsTileScratch* blk;
    POLY_FT4*       prim;
    SVECTOR*        sv;
    s32             quad;
    s32             i;
    s32             n;

    mem   = arg0->spawnArg2;
    quad  = D_acropolis_sanctuary_80182320[arg0->spawnArg1].quad;
    coord = ((TmdObject*)arg0->extra)->field_8;
    Gp_UpdateCoord(coord);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x28;
    blk      = (AcsTileScratch*)(head - 0x28);
    gte_SetTransMatrix(&GsWSMATRIX);
    for (i = 0; i < 4; i++) {
        blk->v[i].vx = D_acropolis_sanctuary_80182710[quad].corner[i].vx;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(AcsTileScratch, v));
        sv->vy = D_acropolis_sanctuary_80182710[quad].corner[i].vy;
        sv->vz = D_acropolis_sanctuary_80182710[quad].corner[i].vz;
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv0(&blk->v[0]);
    gte_rtps_real();
    prim           = (POLY_FT4*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
    setlen(prim, 9);
    setcode(prim, 0x2C);
    gte_stsxy(&prim->x0);
    gte_ldv3(&blk->v[1], &blk->v[2], &blk->v[3]);
    gte_rtpt_real();
    gte_stsxy3(&prim->x1, &prim->x2, &prim->x3);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        if (mem->field_22 == 0) {
            mem->field_24 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_8;
            if (mem->field_24 != 0) {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -(((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 0x40 - (((u32)Gp_LcgState >> 16) & 0x7F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_18 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1A = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1C = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
                mem->field_26 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_A;
            } else {
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_10 = -(((u32)Gp_LcgState >> 16) & 0x1F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_12 = ((u32)Gp_LcgState >> 16) & 7;
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_14 = 4 - (((u32)Gp_LcgState >> 16) & 7);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_18 = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1A = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_1C = 0x20 - (((u32)Gp_LcgState >> 16) & 0x3F);
                Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
                mem->field_26 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_A +
                                (((u32)Gp_LcgState >> 16) & 7);
            }
        }
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_0;
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_2;
        prim->u3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_0;
        prim->v3 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col +
                   D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_2;
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x28;
    if (mem->field_26 + 0x3C < mem->field_22) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    if (mem->field_26 < mem->field_22) {
        coord->coord.t[0] += mem->field_10;
        coord->coord.t[1] += mem->field_12;
        coord->coord.t[2] += mem->field_14;
        Gfx_RotMatrixYXZ(&coord->coord, (SVECTOR*)&mem->field_18, 0);
        coord->flg    = 0;
        mem->field_12 = mem->field_12 + 3;
        if (mem->field_24 == 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            if ((u16)(((u32)Gp_LcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
                Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
                n           = (((u32)Gp_LcgState >> 16) & 3) + 1;
                for (i = 0; i < n; i++) {
                    Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1 | 0x1000, NULL);
                }
                mem->field_22 = mem->field_22 + 0x64;
            }
        }
    }
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        if (quad != 0) {
            Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
            // The size class is dead once it has been tested, so the shard
            // count reuses its local -- keeping the two apart costs `$s5`.
            quad = ((u32)Gp_LcgState >> 16) & 3;
            if (quad != 0) {
                quad = quad + 1;
                for (i = 0; i < quad; i++) {
                    Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1, NULL);
                }
                mem->field_22 = mem->field_22 + 0x64;
            } else {
                coord->coord.t[1] -= mem->field_12 * 2;
                mem->field_12      = -(mem->field_12 >> 1);
            }
        } else {
            coord->coord.t[1] -= mem->field_12 * 2;
            mem->field_12      = -(mem->field_12 >> 1);
        }
    }
    if ((u8)Game_Session->field_4 != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->field_22 = mem->field_22 + 0x3C;
    }
    mem->field_22 = mem->field_22 + 1;
}

/// Draws one frame of a mosaic shard: a semi-transparent textured triangle
/// whose three corners come from the first three corners of
/// `D_acropolis_sanctuary_80182710`, scaled about the origin by the shard's
/// size (`field_26`) with the GTE's `gpf` interpolator and rotated by the
/// task's own `workm`, then projected through `GsWSMATRIX` with `rtpt` into an
/// `AcsMosaicScratch` block taken from `G_SCRATCH_HEAD`; shards inside `otz`
/// 0x11 are dropped. The texture window is the tile's `row` / `col` corner
/// stretched by the same size factor, so the shard shows its own piece of the
/// mosaic sheet.
///
/// `Task::spawnArg1` is unpacked on the first frame: bits 12..15 select the
/// drift pattern, the high halfword is the size (defaulting to 0x1000) and only
/// the low 12 bits are kept, as the index into the tile table. The same frame
/// seeds the per-frame drift (`field_10`..`field_14`) and spin
/// (`field_18`..`field_1C`) from the LCG -- pattern 0 falls faster, since its
/// vertical step is seeded negative.
///
/// Each frame the shard drifts by that step, gains 3 of downward speed, and is
/// respun. Crossing x = -0x2740 above y = -0xED7 either bounces it (halving and
/// inverting the vertical step, twice at most) or, for shards at least 0x401
/// big, shatters it into one or two 0x6007A children; a big shard also has a
/// 1-in-60 chance per frame -- or a certainty once past y = -0xBFF -- of
/// splitting into two. Either way the split costs 0x3C of life. Once the room
/// flag is set and the session is not in mode 0x10, shards past x = -0x28C0
/// also age by 0x3C, so they clear away.
void func_acropolis_sanctuary_8017EC90(Task* arg0)
{
    GpEffWork*        mem;
    GsCOORDINATE2*    coord;
    void**            scratch;
    u8*               head;
    AcsMosaicScratch* blk;
    POLY_FT3*         prim;
    AcsQuadCorner*    corner;
    s32               size;
    s32               hi;
    s32               i;
    s32               n;
    s32               flags;
    SVECTOR*          sv;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if (mem->field_22 >= 0x3D || mem->field_20 >= 2) {
        Gp_ReleaseState1CMem(mem, arg0);
        return;
    }
    Gp_UpdateCoord(coord);
    scratch  = (void**)G_SCRATCH_HEAD;
    head     = *scratch;
    *scratch = head - 0x20;
    blk      = (AcsMosaicScratch*)(head - 0x20);
    if (mem->field_22 == 0) {
        mem->field_24 = (arg0->spawnArg1 >> 12) & 0xF;
        hi            = (s16)(arg0->spawnArg1 >> 16);
        size          = 0x1000;
        if ((u16)hi != 0) {
            size = hi;
        }
        mem->field_26   = size;
        arg0->spawnArg1 = arg0->spawnArg1 & 0xFFF;
        if (mem->field_24 != 0) {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = ((u32)Gp_LcgState >> 16) & 0xF;
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
        } else {
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_10 = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_12 = -(((u32)Gp_LcgState >> 16) & 0x1F);
            Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
            mem->field_14 = 8 - (((u32)Gp_LcgState >> 16) & 0xF);
        }
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_18 = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_1A = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
        Gp_LcgState   = Gp_LcgState * 5 + 0x71357911;
        mem->field_1C = 0x80 - (((u32)Gp_LcgState >> 16) & 0xFF);
    }
    gte_SetTransMatrix(&GsWSMATRIX);
    corner = D_acropolis_sanctuary_80182710->corner;
    for (i = 0; i < 3; i++) {
        blk->v[i].vx = corner[i].vx;
        // Spelled as an offset rather than `&blk->v[i]` so it stays a separate
        // pointer from the one the GTE macros below take; writing both the same
        // way lets CSE fold them into one register and the loop stops matching.
        sv     = (SVECTOR*)((u8*)blk + i * sizeof(SVECTOR) + OFFSET_OF(AcsMosaicScratch, v));
        sv->vy = corner[i].vy;
        sv->vz = corner[i].vz;
        gte_lddp(mem->field_26);
        gte_ldsv(&blk->v[i]);
        gte_gpf12_real();
        gte_stsv(&blk->v[i]);
        gte_SetRotMatrix(&coord->workm);
        gte_ldv0(&blk->v[i]);
        gte_rtv0_real();
        gte_stsv(&blk->v[i]);
        blk->v[i].vx += coord->workm.t[0];
        sv->vy       += coord->workm.t[1];
        sv->vz       += coord->workm.t[2];
    }
    gte_SetRotMatrix(&GsWSMATRIX);
    gte_ldv3(&blk->v[0], &blk->v[1], &blk->v[2]);
    gte_rtpt_real();
    prim           = (POLY_FT3*)Gpu_PrimCursor;
    Gpu_PrimCursor = (DR_TPAGE*)((LINE_G3*)prim + 1);
    setlen(prim, 7);
    setcode(prim, 0x24);
    gte_stsxy3(&prim->x0, &prim->x1, &prim->x2);
    gte_stszotz(&blk->otz);
    if (blk->otz >= 0x11) {
        prim->tpage = 0x8C;
        prim->clut  = 0x4200;
        prim->code |= 3;
        prim->u0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row;
        prim->v0    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col;
        prim->u1    = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_0 * mem->field_26) >> 12);
        prim->v1 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col;
        prim->u2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].row;
        prim->v2 = D_acropolis_sanctuary_80182320[arg0->spawnArg1].col +
                   ((D_acropolis_sanctuary_80182320[arg0->spawnArg1].field_2 * mem->field_26) >> 12);
        addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                prim);
    }
    coord->coord.t[0]      += mem->field_10;
    coord->coord.t[1]      += mem->field_12;
    *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x20;
    coord->coord.t[2]      += mem->field_14;
    Gfx_RotMatrixYXZ(&coord->coord, (SVECTOR*)&mem->field_18, 0);
    coord->flg    = 0;
    mem->field_12 = mem->field_12 + 3;
    if (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        n           = ((u32)Gp_LcgState >> 16) & 1;
        if (mem->field_26 >= 0x401 && n != 0) {
            n = n + 1;
            for (i = 0; i < n; i++) {
                Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1 | (mem->field_26 << 15), NULL);
            }
            mem->field_22 = mem->field_22 + 0x3C;
        } else {
            coord->coord.t[1] -= mem->field_12 * 2;
            mem->field_12      = -(mem->field_12 >> 1);
            mem->field_20      = mem->field_20 + 1;
        }
    } else if (mem->field_26 >= 0x401) {
        Gp_LcgState = Gp_LcgState * 5 + 0x71357911;
        if ((u16)(((u32)Gp_LcgState >> 16) % 60U) == 0 || coord->coord.t[1] >= -0xBFF) {
            for (i = 0; i < 2; i++) {
                flags = (mem->field_26 << 15) | 0x1000;
                Gp_SpawnEff(0x6007A, coord, arg0->spawnArg1 | flags, NULL);
            }
            mem->field_22 = mem->field_22 + 0x3C;
        }
    }
    if ((u8)Game_Session->field_4 != 0x10 && D_acropolis_sanctuary_80182770 != 0 &&
        (coord->coord.t[0] < -0x28C0 ||
         (coord->coord.t[0] < -0x2740 && coord->coord.t[1] >= -0xED7))) {
        mem->field_22 = mem->field_22 + 0x3C;
    }
    mem->field_22 = mem->field_22 + 1;
}

/// Draws one frame of the sanctuary's flame sprite. The task's coordinate is
/// refreshed and projected through `GsWSMATRIX` into an `AcsSprayScratch` block
/// taken from `G_SCRATCH_HEAD`; the projected point becomes the centre of a
/// semi-transparent `POLY_FT4` on tpage 0x2B whose half-extent is
/// `field_24 * 0x27 / otz`, so the flame shrinks with distance and is dropped
/// entirely inside `otz` 0x11. `Task::spawnArg1` is unpacked once, on the first
/// frame: bits 16..27 are the sprite's size (defaulting to 0x280 when zero),
/// bits 8..9 pick one of four 0x28x0x27 cells across the sheet -- and, through
/// `getClut`, the matching 16-colour palette -- and only the low nibble is kept,
/// as the index into `D_acropolis_sanctuary_801827D4`, the per-variant mask of
/// camera views the flame is visible from. The grey level is the variant's base
/// level plus its flicker amplitude on odd frames.
void func_acropolis_sanctuary_8017F4E8(Task* arg0)
{
    GpEffWork*       mem;
    GsCOORDINATE2*   coord;
    void**           scratch;
    u8*              head;
    AcsSprayScratch* blk;
    POLY_FT4*        prim;
    AcsSpriteLevels  base;
    AcsSpriteLevels  step;
    s32              param;
    s32              lvl;
    s16              x;
    s16              y;

    mem   = arg0->spawnArg2;
    coord = ((TmdObject*)arg0->extra)->field_8;
    if ((D_acropolis_sanctuary_801827D4[arg0->spawnArg1 & 0xF] >> ((u8)Game_Session->field_4 - 1)) & 1) {
        Gp_UpdateCoord(coord);
        scratch  = (void**)G_SCRATCH_HEAD;
        head     = *scratch;
        *scratch = head - 0x14;
        blk      = (AcsSprayScratch*)(head - 0x14);
        if (arg0->state == 0) {
            base            = D_acropolis_sanctuary_8017D5D8;
            step            = D_acropolis_sanctuary_8017D5DC;
            param           = arg0->spawnArg1;
            mem->field_24   = (param & 0x0FFF0000) ? ((param >> 16) & 0xFFF) : 0x280;
            mem->field_26   = (arg0->spawnArg1 >> 8) & 3;
            arg0->spawnArg1 = arg0->spawnArg1 & 0xF;
            mem->field_28   = base.v[mem->field_26];
            mem->field_2A   = step.v[mem->field_26];
            arg0->state++;
        }
        blk->pos.vx = *(u16*)&coord->workm.t[0];
        blk->pos.vy = *(u16*)&coord->workm.t[1];
        blk->pos.vz = *(u16*)&coord->workm.t[2];
        gte_SetTransMatrix(&GsWSMATRIX);
        gte_SetRotMatrix(&GsWSMATRIX);
        gte_ldv0(&blk->pos);
        gte_rtps_real();
        prim           = (POLY_FT4*)Gpu_PrimCursor;
        Gpu_PrimCursor = (DR_TPAGE*)(prim + 1);
        setlen(prim, 9);
        setcode(prim, 0x2C);
        gte_stsxy(&blk->sx);
        gte_stszotz(&blk->otz);
        if (blk->otz >= 0x11) {
            lvl         = (u8)mem->field_28 + (Display_State.field_8 & 1) * mem->field_2A;
            prim->tpage = 0x2B;
            prim->code |= 2;
            setRGB0(prim, lvl, lvl, lvl);
            prim->clut = getClut(mem->field_26 * 0x10, 0x10E);
            prim->u0   = mem->field_26 * 0x28;
            prim->v0   = 0;
            prim->u1   = mem->field_26 * 0x28 + 0x27;
            prim->v1   = 0;
            prim->u2   = mem->field_26 * 0x28;
            prim->v2   = 0x27;
            prim->u3   = mem->field_26 * 0x28 + 0x27;
            prim->v3   = 0x27;
            blk->half  = (mem->field_24 * 0x27) / blk->otz;
            x          = blk->sx - (u16)blk->half;
            prim->x2   = x;
            prim->x0   = x;
            x          = blk->sx + (u16)blk->half;
            prim->x3   = x;
            prim->x1   = x;
            y          = blk->sy - (u16)blk->half;
            prim->y1   = y;
            prim->y0   = y;
            y          = blk->sy + (u16)blk->half;
            prim->y3   = y;
            prim->y2   = y;
            addPrim((u_long*)(((((u32)blk->otz << Display_State.field_128) >> 2) & 0xFFC) + (s32)Gpu_CurrentOt),
                    prim);
        }
        *(void**)G_SCRATCH_HEAD = (u8*)*(void**)G_SCRATCH_HEAD + 0x14;
    }
}

/// Spawns effect 0x60078 on the room task's model coordinate, seeded with the
/// fixed offset vector held in this unit's rodata. Always consumes the event
/// (returns 0).
s32 func_acropolis_sanctuary_8017F918(Task* task)
{
    GsCOORDINATE2* coord = ((TmdObject*)task->extra)->field_8;
    SVECTOR        vec   = D_acropolis_sanctuary_8017D5D0;

    Gp_SpawnEff(0x60078, coord, 0, &vec);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F974);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017FB18);

/// Per-frame visibility gate for the sanctuary's item object: hides the model
/// (`field_C` bit 0x80) while the camera sits on view 0xB or 0xD, or once the
/// item's 2-bit pickup flag has reached 2; otherwise shows it again with the
/// default flags.
void func_acropolis_sanctuary_80180264(Task* task)
{
    GpItemObj8* obj = task->spawnArg2;
    TmdObject*  tmd = task->extra;
    s32         flag;
    s32         view;

    flag = Gp_GetCurBit2Flag(obj->field_8);
    view = Gp_GetViewIndex();
    if (view == 0xB || view == 0xD || flag == 2) {
        tmd->field_C = 0x80;
    } else {
        tmd->field_C = 8;
        tmd->field_E = 0;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_801802E0);
