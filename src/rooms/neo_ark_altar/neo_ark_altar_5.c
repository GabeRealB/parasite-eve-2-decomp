#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflow.h"
#include "main/mem.h"
#include "main/session.h"
#include "main/task.h"

extern TaskDesc RoomsShared8018397cDesc;

extern u8 D_8007216D;

extern void func_neo_ark_altar_8017E148(void);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DC40);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017DF0C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E148);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E260);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E658);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017E92C);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_altar/neo_ark_altar_5", func_neo_ark_altar_8017EC34);

void func_neo_ark_altar_8017ED60(Task* task);
void func_neo_ark_altar_8017EDBC(Task* task);
void func_neo_ark_altar_8017DF0C(Task* task);
void func_neo_ark_altar_8017EDF8(Task* task);
void func_neo_ark_altar_8017EE30(Task* task);
void func_neo_ark_altar_8017EE90(Task* task);
void func_neo_ark_altar_8017EF00(Task* task);
void func_neo_ark_altar_8017EF34(Task* task);

/// State handlers of the altar task, dispatched by
/// `func_neo_ark_altar_8017ECE0` off `Task::state`. The table is declared here
/// rather than left to the splitter: only a file-scope definition in this unit
/// puts the bytes in this unit's `.rodata`, after the jump tables
/// `func_neo_ark_altar_8017DC40` contributes to the same block.
const TaskFuncTable8 D_neo_ark_altar_8017D648 = {
    func_neo_ark_altar_8017ED60,
    func_neo_ark_altar_8017EDBC,
    func_neo_ark_altar_8017DF0C,
    func_neo_ark_altar_8017EDF8,
    func_neo_ark_altar_8017EE30,
    func_neo_ark_altar_8017EE90,
    func_neo_ark_altar_8017EF00,
    func_neo_ark_altar_8017EF34,
};

void func_neo_ark_altar_8017ECE0(Task* arg0)
{
    TaskFuncTable8 sp = D_neo_ark_altar_8017D648;

    sp.funcs[arg0->state](arg0);
}

void func_neo_ark_altar_8017ED60(Task* arg0)
{
    TaskIdMap* work;

    work        = Mem_Calloc(0x10, 0);
    arg0->idMap = work;
    if (work == NULL) {
        Task_Kill(arg0);
        return;
    }
    func_neo_ark_altar_8017E148();
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EDBC(Task* arg0)
{
    arg0->killCountdown = arg0->killCountdown + 1;
    if (arg0->killCountdown >= 3) {
        arg0->state = (s32)(arg0->state + 1);
    }
}

void func_neo_ark_altar_8017EDF8(Task* arg0)
{
    Gp_MsgPlayerWeapon(0);
    arg0->killCountdown = 0;
    arg0->state         = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EE30(Task* arg0)
{
    u8 temp_a0;

    arg0->killCountdown = arg0->killCountdown + 6;
    if (arg0->killCountdown >= 0x100) {
        arg0->killCountdown = 0xFF;
        arg0->state         = (s32)(arg0->state + 1);
    }
    temp_a0 = (u8)arg0->killCountdown;
    Fade_DrawOverlay(temp_a0, temp_a0, temp_a0, 2);
}

void func_neo_ark_altar_8017EE90(Task* arg0)
{
    Task** temp_s1;

    temp_s1 = arg0->idMap;
    Gp_MsgPlayer3F3(0);
    Game_Session->field_68 = 1;
    *temp_s1               = Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, 2, 0);
    arg0->state            = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF00(Task* arg0)
{
    *(s16*)((u8*)Game_Session + OFFSET_OF(GameSession, field_52)) = 1;
    D_8007216D                                                    = 2;
    Game_Session->field_5                                         = 2;
    arg0->state                                                   = (s32)(arg0->state + 1);
}

void func_neo_ark_altar_8017EF34(Task* arg0)
{
    SetDispMask(1);
    Gp_MsgPlayer3F3(1);
    Gp_MsgPlayerWeapon(1);
    Game_Session->field_68 = 0;
    arg0->state            = 2;
}

void func_neo_ark_altar_8017EF84(void)
{
}
