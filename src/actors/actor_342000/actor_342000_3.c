#include "common.h"

#include "main/fs.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"

#include "actors/actor_342000.h"

void func_actor_342000_801640C0(Task* arg0, s32 arg1, Actor342000Move* arg2)
{
    Actor342000Work* work;
    GsCOORDINATE2*   coord;

    work              = (Actor342000Work*)arg0->idMap;
    coord             = &work->coord;
    coord->coord.t[0] = arg2->field_0;
    coord->coord.t[1] = arg2->field_4;
    coord->coord.t[2] = arg2->field_8;
    work->field_274   = arg2->field_10;
    work->field_278   = arg2->field_12;
    work->field_27C   = arg2->field_14;
    work->coord.flg   = 0;
}

void func_actor_342000_80164110(Task* arg0, s32 arg1, Actor342000Cmd* arg2, Actor342000Move* arg3)
{
    Actor342000Work* work;

    work = (Actor342000Work*)arg0->idMap;
    if (arg2->field_2 == 0xA) {
        work->field_264.vx = arg3->field_0;
        work->field_264.vy = arg3->field_4;
        work->field_264.vz = arg3->field_8;
    }
    work->field_2AA = arg2->field_2;
}

extern s8             D_8007216D;
extern GpAreaApplyRec D_8018FB6C[];

void func_actor_342000_80164154(void)
{
    Game_Session->field_5        = 7;
    D_8007216D                   = 7;
    Game_Session->unknown_133[1] = 6;
    Game_Session->unknown_133[0] = 1;
    Game_Session->field_76       = 1;
    Gp_ApplyAreaRecs(D_8018FB6C);
}

void func_actor_342000_801641B4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_50 != NULL) {
        Task_CallExit(work->field_50);
    }
    work->field_50 = NULL;
}

void func_actor_342000_801641FC(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_5C != NULL) {
        Task_Kill(work->field_5C);
    }
    if (work->field_60 != NULL) {
        Task_Kill(work->field_60);
    }
    work->field_5C = NULL;
    work->field_60 = NULL;
}

void func_actor_342000_80164260(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_7A == 0) {
        SndEvt_EnqueueType6(0x54280005, 0, 0);
        work->field_7A = 1;
    }
}

void func_actor_342000_801642B4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    work->field_68 = arg0;
    work->field_6A = 0;
}

void func_actor_342000_801642D4(s16 arg0)
{
    Actor342000EventWork* work;

    work           = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    work->field_70 = arg0;
    work->field_72 = 0;
}

extern s8 D_8007272D;

void func_actor_342000_801642F4(void)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    if (work->field_7C == 0) {
        Gp_StateF0.field_6      = 0;
        Gp_StateF0.field_1      = 0xF;
        Gp_StateF0.field_0      = 0;
        Gp_StateF0.field_2      = 0;
        Gp_StateF0.field_3      = 0;
        Game_Session->field_69 |= 0x80;
        D_8007272D              = 0xD;
        work->field_7C          = 1;
    }
}

void func_actor_342000_80164364(s32 arg0)
{
    Actor342000EventWork* work;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    Gp_DispatchMsg(work->field_48, 0x3F3, arg0, 0);
}

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// base weapon id records are numbered from, and `D_8007218A` selects the
/// alternate set -- 1 means the second block, anything else the `+0x22` one.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Fixed placement this function warps slot 3 to, sent as message 0x3E9 and
/// again as 0x3F2 by `func_actor_342000_80162BBC`.
extern Actor342000Move D_actor_342000_80164948;

extern void func_8018507C(void);

/// Warps the slot-3 task to the overlay's fixed placement (0x3E9), installs
/// the animation set the current weapon selects (`D_80073BA9 + 1` for the
/// alternate block, `+ 0x22` for the base one, sent as 0x3E8 to the slot
/// `Game_GetPtrSlot(3)` returns), raises 0x3F3, kills the child in
/// `field_64`, and cancels any pending CD command replacement.
void func_actor_342000_8016439C(void)
{
    Actor342000EventWork* work;
    GpAnimArg             msg;
    s32                   weaponId;
    s32                   anim;

    work = (Actor342000EventWork*)D_actor_342000_80165070->idMap;
    Gp_DispatchMsg(work->field_48, 0x3E9, (s32)&D_actor_342000_80164948, 0);
    func_8018507C();
    weaponId     = D_80073BA9;
    anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0  = (void*)anim;
    msg.field_4  = 1;
    msg.field_8  = 0;
    msg.field_C  = 0;
    msg.field_10 = 0;
    Gp_DispatchMsg(Game_GetPtrSlot(3), 0x3E8, (s32)&msg, 0);
    Gp_DispatchMsg(((Actor342000EventWork*)D_actor_342000_80165070->idMap)->field_48, 0x3F3, 1, 0);
    if (work->field_64 != NULL) {
        Task_Kill(work->field_64);
        work->field_64 = NULL;
    }
    CdCmd_CancelReplaceAndActivate();
}
