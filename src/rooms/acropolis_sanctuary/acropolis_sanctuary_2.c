#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_sanctuary.h"
#include "rooms/room_common.h"

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

extern SVECTOR       D_acropolis_sanctuary_8017D5D0;
extern GpMsgEntry    D_acropolis_sanctuary_8018081C[];
extern RoomPlacement D_acropolis_sanctuary_801808BC;
extern GpRec14       D_acropolis_sanctuary_801809F8;
extern GpRec14       D_acropolis_sanctuary_80180A0C;
extern s32           D_acropolis_sanctuary_80180AE8;
extern u8            D_acropolis_sanctuary_80181814[];
extern TaskDesc      D_acropolis_sanctuary_80182240;
extern AcsBlockerSet D_acropolis_sanctuary_801822EC;
extern GpMsgEntry    D_acropolis_sanctuary_80182310[];
extern AcsTile       D_acropolis_sanctuary_80182320[];
extern AcsQuad       D_acropolis_sanctuary_80182710[];
extern s16           D_acropolis_sanctuary_80182750[];
extern s32           D_acropolis_sanctuary_80182770;
extern SVECTOR       D_acropolis_sanctuary_80182774[];
extern AcsBlockerSet D_acropolis_sanctuary_80183568;
extern Task*         D_acropolis_sanctuary_80186C90;

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

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017E338);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017EC90);

INCLUDE_ASM("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary_2", func_acropolis_sanctuary_8017F4E8);

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
