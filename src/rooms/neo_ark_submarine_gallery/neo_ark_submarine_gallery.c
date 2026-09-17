#include "common.h"
#include "gameplay/1A8.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"

extern GpMsgEntry D_neo_ark_submarine_gallery_80181884[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818BC[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818AC;

/// Staging save location the gallery commits: `field_2` / `field_4` / `field_1`
/// hold what `func_neo_ark_submarine_gallery_8017EA0C` copies out of the
/// incoming location, and `func_neo_ark_submarine_gallery_8017E86C` moves those
/// same three bytes into `Mc_SaveData.field_6` / `field_8` / `field_5`.
extern GpSaveLoc D_neo_ark_submarine_gallery_80185924;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern s32 func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", D_neo_ark_submarine_gallery_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017D678);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017E2CC);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", D_neo_ark_submarine_gallery_8017D614);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017E86C);

s32 func_neo_ark_submarine_gallery_8017EA04(void)
{
    return 0;
}

/// Gallery message handler. Message 0xE, while the incoming location still
/// reports no pending flag, latches the save location the outgoing message
/// carries and starts the cutscene the gallery leads out of. Returns 0 for that
/// message and 1 for every other one.
s32 func_neo_ark_submarine_gallery_8017EA0C(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 0xE) {
        if (src->field_5 == 0) {
            D_neo_ark_submarine_gallery_80185924.field_2 = dst->field_0;
            D_neo_ark_submarine_gallery_80185924.field_4 = dst->field_2;
            D_neo_ark_submarine_gallery_80185924.field_1 = dst->field_3;
            Task_SpawnFromTable(&D_neo_ark_submarine_gallery_801818AC, 0, 0, 0);
        }
        return 0;
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EABC);

s32 func_neo_ark_submarine_gallery_8017EB48(void)
{
    return 0;
}

void func_neo_ark_submarine_gallery_8017EB50(Task* arg0)
{
    arg0->field_24 = D_neo_ark_submarine_gallery_80181884;
    Game_SetPtrSlot(arg0, 7);
    if (Game_Session->field_9 == 4) {
        Task_SpawnFromTable(D_neo_ark_submarine_gallery_801818BC, 0, 0, 0);
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EBC4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EBCC);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EC24);

void func_neo_ark_submarine_gallery_8017EED8(Task* arg0)
{
    if (Game_Session->field_9 != 4) {
        arg0->killCountdown = 0;
    } else {
        arg0->killCountdown = 0x780;
    }
    arg0->state = (s32)(arg0->state + 1);
}

void func_neo_ark_submarine_gallery_8017EF14(Task* arg0)
{
    s32 mode;
    if (Gp_ActorSlots[0] != NULL) {
        mode = 4;
        if (Game_Session->field_9 != mode && Game_Session->field_126 != 0) {
            Game_Session->field_9 = mode;
        }
        if (arg0->killCountdown < 0x780) {
            arg0->killCountdown = (s16)((u16)arg0->killCountdown + 0x10);
        }
        func_neo_ark_submarine_gallery_8017EC24((u16)arg0->killCountdown, mode);
    }
}

void func_neo_ark_submarine_gallery_8017EF8C(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EF94);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017EFEC);
