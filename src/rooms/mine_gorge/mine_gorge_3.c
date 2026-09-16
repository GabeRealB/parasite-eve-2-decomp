#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"

#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_mine_gorge_8017E280;
extern s8  D_8011540A;
extern u8  D_80062735;

/// Room setup task tick: install the message table and pointer slot, raise the
/// `0x15` script key on the first visit (`Game_Session::field_9 == 1`) while flag
/// nibble `0xC5` is set, and on the first pass through flag nibble `0xBE == 2`
/// arm nibble `0x166`, clear nibble `0xB5` and spawn the room's cap event. Then
/// arm the "among us" byte and advance state.
void func_mine_gorge_8017D8D4(Task* arg0)
{
    arg0->field_24 = &D_mine_gorge_8017E280;
    Game_SetPtrSlot(arg0, 7);
    if ((Game_Session->field_9 == 1) && (GameFlag_GetNibble(0xC5) != 0)) {
        D_8011540A = 0x15;
    }
    if ((GameFlag_GetNibble(0xBE) == 2) && (GameFlag_GetNibble(0x166) == 0)) {
        GameFlag_SetNibble(0x166, 1);
        GameFlag_SetNibble(0xB5, 0);
        Gp_SpawnIfCapIdle(8, 0);
    }
    arg0->state = arg0->state + 1;
    D_80062735  = 1;
}

void func_mine_gorge_8017D998(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/mine_gorge/mine_gorge_3", jtbl_mine_gorge_8017D5D0);
