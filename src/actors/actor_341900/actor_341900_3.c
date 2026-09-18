#include "common.h"

#include "main/fs.h"
#include "main/task.h"

#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"

#include "actors/actor_341900.h"

extern TaskDesc D_actor_341900_80164190;

/// Main-executable globals with no module header yet: `D_80073BA9` is the
/// base weapon id records are numbered from, and `D_8007218A` selects the
/// alternate set -- 1 means the second block, anything else the `+0x22` one.
extern u8 D_80073BA9;
extern s8 D_8007218A;

/// Fixed placement `func_actor_341900_801635A4` warps slot 3 to, sent as
/// message 0x3E9 right after the animation message.
extern Actor341900MsgPos D_actor_341900_80163B28;

void func_actor_341900_80163388(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    Gp_DispatchMsg(work->field_8, 0x7D5, arg0, 0);
}

void func_actor_341900_801633C0(s32 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    Gp_DispatchMsg(work->field_0, 0x3F3, arg0, 0);
}

void func_actor_341900_801633F8(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C == 0) {
        work->field_6C = 1;
        Gp_KillPlayerEffs();
    }
}

void func_actor_341900_80163438(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_6C != 0) {
        Gp_SpawnWeaponEff();
        work->field_6C = 0;
        Gp_MsgPlayerWeapon(0);
    }
}

void func_actor_341900_80163488(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_8 != NULL) {
        Task_Kill(work->field_8);
        work->field_8 = NULL;
    }
}

void func_actor_341900_801634D0(void)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    if (work->field_C != NULL) {
        Task_Kill(work->field_C);
        work->field_C = NULL;
    }
    if (work->field_10 != NULL) {
        Task_Kill(work->field_10);
        work->field_10 = NULL;
    }
}

void func_actor_341900_80163534(void)
{
    Task_SpawnFromTable(&D_actor_341900_80164190, 1, 9, 0);
}

void func_actor_341900_80163564(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_5C = arg0;
    work->field_5E = 0;
}

void func_actor_341900_80163584(s16 arg0)
{
    Actor341900Work* work = (Actor341900Work*)D_actor_341900_80164208->work;

    work->field_64 = arg0;
    work->field_66 = 0;
}

/// Installs one animation set on slot 3 (message 0x3E8) and then warps it to
/// the overlay's fixed placement (message 0x3E9), cancelling any pending CD
/// command replacement on the way out. The set is `D_80073BA9 + 1` for the
/// alternate weapon block and `D_80073BA9 + 0x22` for the base one; its
/// `field_4` is 9, the rest of the frame is zero.
void func_actor_341900_801635A4(void)
{
    Actor341900Work* work;
    GpAnimArg        msg;
    s32              weaponId;
    s32              anim;

    work         = (Actor341900Work*)D_actor_341900_80164208->work;
    weaponId     = D_80073BA9;
    anim         = (D_8007218A == 1) ? weaponId + 1 : weaponId + 0x22;
    msg.field_0  = (void*)anim;
    msg.field_4  = 9;
    msg.field_8  = 0;
    msg.field_C  = 0;
    msg.field_10 = 0;
    Gp_DispatchMsg(work->field_0, 0x3E8, (s32)&msg, 0);
    Gp_DispatchMsg(work->field_0, 0x3E9, (s32)&D_actor_341900_80163B28, 0);
    CdCmd_CancelReplaceAndActivate();
}
