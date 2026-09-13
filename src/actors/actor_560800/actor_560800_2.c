#include "common.h"
#include "actors/actor_560800.h"
#include "gameplay/D4.h"
#include "main/sound.h"
#include "main/task.h"

#include "main/fs.h"
#include "psyq/libgpu.h"

extern TaskDesc ActorsShared80136280Desc;
extern s32      D_actor_560800_8016F57C[];

void func_actor_560800_801362B0(s32 arg0)
{
    Task_SpawnFromTable(&ActorsShared80136280Desc, 3, arg0, 0);
}

void func_actor_560800_801362E0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;
    Actor560800Msg   msg;

    msg.field_2 = arg0;
    Gp_DispatchMsg(work->field_20, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_8013631C(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;
    Actor560800Msg   msg;

    msg.field_2 = arg0;
    Gp_DispatchMsg(work->field_24, 0x7DB, (s32)&msg, 0);
}

void func_actor_560800_80136358(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;

    work->field_28 = arg0;
    work->field_2A = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136378);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801363F8);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801364A0);

/// Copies a 64x256 strip of VRAM to (0x280, 0x100), then re-loads the chunk at
/// `D_8006C454` with `D5B498_8006C234` set to 5 for the duration (that byte is
/// the image mode `Fs_LoadImageChunk` reads for chunks whose second halfword is
/// in 0xF5..0xFF), restoring it to 0 afterwards.
void func_actor_560800_80136548(void)
{
    RECT rect;

    rect.x = 0x3C0;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x100;
    MoveImage(&rect, 0x280, 0x100);
    D5B498_8006C234 = 5;
    Fs_LoadImageChunk(D_8006C454, 1);
    D5B498_8006C234 = 0;
}

void func_actor_560800_801365B0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;

    work->field_40 = arg0;
    work->field_42 = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801365D0);

void func_actor_560800_80136678(s32 arg0)
{
    SndEvt_EnqueueType6(D_actor_560800_8016F57C[arg0], 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801366B0);

void func_actor_560800_801367C0(s16 arg0)
{
    Actor560800Work* work = (Actor560800Work*)D_actor_560800_8017578C->idMap;

    work->field_58 = arg0;
    work->field_5A = 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_801367E0);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136818);

INCLUDE_ASM("actors/nonmatchings/actor_560800/actor_560800_2", func_actor_560800_80136878);
