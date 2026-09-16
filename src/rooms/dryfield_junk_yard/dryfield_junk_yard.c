#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/4CC.h"
#include "gameplay/D4.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "main/tmd.h"

extern s32 D_dryfield_junk_yard_8017DD20;
extern s32 D_dryfield_junk_yard_8017DD88;
extern s32 D_dryfield_junk_yard_8017DE00;
extern s32 D_dryfield_junk_yard_8017DE30;
extern s32 D_dryfield_junk_yard_8017E490;
extern s32 D_dryfield_junk_yard_8017E658;

void func_dryfield_junk_yard_8017D658(Task* task);

/// State 0 of the yard's pickup-model task: hides the mesh with flag 4
/// (`field_C` 0x84) once the item's 2-bit flag reads 2, otherwise shows it
/// (`field_C` 0) and clears the animation byte, then advances the task.
void func_dryfield_junk_yard_8017D5F4(Task* task)
{
    GpItemObj8* obj;
    TmdObject*  tmd;
    s32         flag;

    obj          = (GpItemObj8*)task->spawnArg2;
    tmd          = (TmdObject*)task->extra;
    flag         = Gp_GetCurBit2Flag(obj->field_8);
    tmd->field_C = 0;
    if (flag == 2) {
        tmd->field_C = 0x84;
    } else {
        tmd->field_E = 0;
    }
    func_dryfield_junk_yard_8017D658(task);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", func_dryfield_junk_yard_8017D658);

void func_dryfield_junk_yard_8017D708(Task* arg0)
{
    arg0->field_24 = &D_dryfield_junk_yard_8017DD20;
    Game_SetPtrSlot(arg0, 7);
    if (Game_GetPtrSlot(0xA) != 0) {
        if (GameFlag_GetNibble(0x38) == 0) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE00, 0);
            Gp_AllyAnimId(&D_dryfield_junk_yard_8017DD88);
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E8, (s32)&D_dryfield_junk_yard_8017DD88, 0);
        }
        if ((GameFlag_GetNibble(0x39) == 0) && (GameFlag_GetNibble(0x28) >= 2)) {
            GameFlag_SetNibble(0x39, 1);
            func_800E8634((s32)&D_dryfield_junk_yard_8017E490, 0, (s32)&D_dryfield_junk_yard_8017E658);
        } else if (Game_Session->field_8 == 2) {
            Gp_DispatchMsg(Game_GetPtrSlot(0xA), 0x3E9, (s32)&D_dryfield_junk_yard_8017DE30, 0);
        }
    }
    arg0->state = (s32)(arg0->state + 1);
}

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", D_dryfield_junk_yard_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", RoomsShared8017d878Table);

INCLUDE_RODATA("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", D_dryfield_junk_yard_8017D5D0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_junk_yard/dryfield_junk_yard", func_dryfield_junk_yard_8017D848);
