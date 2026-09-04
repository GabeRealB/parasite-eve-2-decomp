#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/1BC.h"
#include "gameplay/3CD8.h"

#include "gameplay/3A34.h"
#include "gameplay/D4.h"
#include "gameplay/gameplay.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/acropolis_square.h"

extern u8                      D_8007216C;
extern s8                      D_80072310;
extern TaskDesc                D_acropolis_square_801837A0;
extern s32                     D_acropolis_square_8018382C;
extern AcropolisSquareCutscene D_acropolis_square_801888AC;
extern s32                     D_acropolis_square_80183834;
extern s32                     D_acropolis_square_801838DC;
extern s32                     D_acropolis_square_80188898;
extern s32                     D_acropolis_square_801888A0;
extern s32                     D_acropolis_square_801888A4;
extern GsCOORDINATE2           D_acropolis_square_801888CC;

INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_5", func_acropolis_square_80181794);
s32 func_acropolis_square_801819BC(Task* task, s32 msgId, s32 arg2, s32 arg3)
{
    s32 var_a0;

    if (arg2 == 2) {
        if (Mc_SaveData.field_8 == 7) {
            Mc_SaveData.field_8 = 1;
        }
        D_acropolis_square_801888AC.camera   = 9;
        D_acropolis_square_801888AC.field_1  = 1;
        D_acropolis_square_801888AC.capFile  = 1;
        D_acropolis_square_801888AC.sndStart = 0x51010001;
        D_acropolis_square_801888AC.snd8     = 0x51010007;
        D_acropolis_square_801888AC.snd10    = 0x51010006;
        D_acropolis_square_801888AC.sndC     = 0x5101000B;
        D_acropolis_square_801888AC.field_2  = D_acropolis_square_8018382C;
        D_acropolis_square_8018382C          = 0;
        Task_SpawnFromTable(&D_acropolis_square_801837A0, 0, 2, (s32)&D_acropolis_square_801888AC);
    }
    if ((arg2 == 0xE) && (GameFlag_GetNibble(0x124) == 0)) {
        GameFlag_SetNibble(0x124, 1);
        Gp_SpawnIfCapIdle(0xE, 1);
    }
    if ((arg2 == 0x10) && (GameFlag_GetNibble(0x156) == 0)) {
        GameFlag_SetNibble(0x156, 1);
        var_a0 = 0x11;
        if (D_80072310 != 1) {
            var_a0 = 0x10;
        }
        Gp_SpawnIfCapIdle(var_a0, 1);
    }
    return 0;
}
/// Siren task for the square. States 0-2 arm the scene and tick, 3 fires the
/// first siren blast, 4 repeats it every 0x79 frames until the player answers,
/// and 5 waits for the scripted phase to advance before handing the scene off
/// to the slot-5 task and killing itself.
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_5", func_acropolis_square_80181AEC);
/// Scrolling backdrop task: three 256x240 sprite strips (the last one half
/// width) tiled across the screen from `D_acropolis_square_801888A0`, each with
/// its own texture page. States 0-3 slide the strip in and hold it for a while,
/// state 4 kills the task; every state still draws.
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_5", func_acropolis_square_80181DD0);
INCLUDE_ASM("rooms/nonmatchings/acropolis_square/acropolis_square_5", func_acropolis_square_80182048);

s32 func_acropolis_square_801820D8(Task* task, s32 msgId, GpMsg13EF* arg2)
{
    if (arg2->field_2 == 0) {
        Gp_SpawnIfCapIdle(5, 0);
    }
    return 0;
}
/* The retail overlay has a zero word at 0x8017D6F0, right after this TU's
 * second jump table: aspsx rounded each object's `.rdata` up to the 8-byte
 * alignment GCC gives a jump table, and modern gas does not. Ask for the pad
 * explicitly, or every rodata symbol from 0x8017D6F4 on - and with it the whole
 * `.text` - shifts down by 4. */
#if !defined(SPLAT) && !defined(M2CTX) && !defined(PERMUTER) && !defined(SKIP_ASM)
__asm__(".section .rodata\n"
        "\t.align 3\n"
        ".section .text\n");
#endif
