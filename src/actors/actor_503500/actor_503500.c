#include "common.h"
#include "main/task.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"
#include "gameplay/3FB8.h"
#include "gameplay/D4.h"
#include "main/session.h"
#include "actors/actor_503500.h"
extern s8 D_actor_503500_80176D5A;

extern u16 D_actor_503500_80176D24;

extern Task* D_actor_503500_80176558;

extern TaskDesc D_actor_503500_8014B964;

/// `Gp_DispatchMsg` handler table installed at `Task::field_24` by
/// `func_actor_503500_80132430`; terminator id 0x7FFFFFFF.
extern GpMsgEntry D_actor_503500_80146888[];

void func_actor_503500_801324C4(Task* arg0);
void func_actor_503500_801324EC(Task* arg0);

/// Opaque script/table blobs in the overlay's `.data`, handed to
/// `func_800E8634` (which forwards them to `Task_Spawn`) as raw addresses.
extern u8 D_actor_503500_8014CD98[];
extern u8 D_actor_503500_8014D098[];

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_8013223C);

void func_actor_503500_80132430(Task* arg0)
{
    GameActorExt*        ext;
    Actor503500ColorMtx* work;

    ext  = arg0->extra;
    work = Mem_Calloc(sizeof(Actor503500ColorMtx), false);
    if (work == NULL) {
        Gp_EnemyTaskExit(arg0);
        return;
    }

    arg0->idMap    = (TaskIdMap*)work;
    ext->field_C  |= 0x84;
    work->field_44 = 0;
    func_actor_503500_801324EC(arg0);
    arg0->field_24     = D_actor_503500_80146888;
    arg0->exitCallback = func_actor_503500_801324C4;
    arg0->state       += 1;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801324C4);

void func_actor_503500_801324EC(Task* arg0)
{
    GameActorExt*        ext;
    Actor503500ColorMtx* work;

    ext           = arg0->extra;
    work          = (Actor503500ColorMtx*)arg0->idMap;
    ext->field_1C = &work->light;
    ext->field_20 = &work->color;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132508);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132584);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132664);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500", D_actor_503500_80131E20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_8013270C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132778);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132990);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132B78);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132B98);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132BB8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132BD8);

void func_actor_503500_80132BF8(void)
{
    Mc_SaveData.field_6 = 0x16;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_actor_503500_80132C40(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_503500_8014B964, 0, arg0, 0);
}

void func_actor_503500_80132C70(s32 arg0)
{
    D_actor_503500_80176558 = Task_SpawnFromTable(&D_actor_503500_8014B964, 1, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132CA4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132CC4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132D00);

void func_actor_503500_80132D20(Task* arg0)
{
    func_800E8634((s32)D_actor_503500_8014CD98, 0, (s32)D_actor_503500_8014D098);
    Task_Kill(arg0);
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132D60);

void func_actor_503500_80132D7C(void)
{
    Game_Session->field_52 = 1;
}

void func_actor_503500_80132D90(s32 arg0)
{
    GameFlag_SetNibble(0x100, arg0);
}

/// Overlay import at a fixed address (`configs/USA/sym/actors.imports.txt`);
/// whatever room overlay is resident owns the body.
extern void func_8017E27C(s32 arg0);

void func_actor_503500_80132DB4(s32 arg0)
{
    func_8017E27C(arg0 & 0xFF);
}

extern Actor503500MsgPos D_actor_503500_8017655C;

void func_actor_503500_80132DD4(void)
{
    D_actor_503500_8017655C.x = 0;
    D_actor_503500_8017655C.y = 0;
    D_actor_503500_8017655C.z = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132DEC);

void func_actor_503500_80132E7C(void)
{
    Task* slot3;

    slot3 = Game_GetPtrSlot(3);
    if ((D_actor_503500_8017655C.x != 0) || (D_actor_503500_8017655C.y != 0) ||
        (D_actor_503500_8017655C.z != 0)) {
        Gp_DispatchMsg(slot3, 0x3E9, (s32)&D_actor_503500_8017655C, 0);
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132EE8);

/// Player-facing flag byte in the main executable; no module header owns it yet.
extern u8 D_80073BA9;

void func_actor_503500_80132EF4(void)
{
    func_80106350(Game_GetPtrSlot(3), D_80073BA9, 0);
}

/// `src/gameplay/3CD8.c`; no gameplay header declares it yet.
extern void Gp_HaltPadScripts(void);

void func_actor_503500_80132F28(void)
{
    Gp_HaltPadScripts();
    Game_Session->field_13B = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132F58);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80132F64);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80133270);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801334CC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80133684);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801338E8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80133BF4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80133D40);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80133FD8);

INCLUDE_RODATA("actors/nonmatchings/actor_503500/actor_503500", D_actor_503500_80131E44);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80134284);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80134408);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801345F4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80134A24);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80134C68);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80134EAC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135178);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801353F0);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135644);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135828);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135950);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135B74);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135CE8);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135D00);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135E04);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135E20);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135F9C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80135FB4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136014);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136048);

s32 func_actor_503500_8013608C(void)
{
    return (u32)(D_actor_503500_80176D24 - 2) < 3U;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801360A4);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_801360BC);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_8013611C);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136134);

s8 func_actor_503500_80136208(void)
{
    return D_actor_503500_80176D5A;
}

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136218);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136228);

INCLUDE_ASM("actors/nonmatchings/actor_503500/actor_503500", func_actor_503500_80136280);
