#include "common.h"
#include "gameplay/1A8.h"
#include "main/display.h"
#include "gameplay/gameplay.h"
#include "gameplay/D4.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/mc.h"
#include "main/sound.h"

extern GpMsgEntry D_neo_ark_submarine_gallery_80181884[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818BC[];
extern TaskDesc   D_neo_ark_submarine_gallery_801818AC;

extern s16 D_80071076;
extern s8  D_801153F4;
extern s8  D_80115690;

/// 0x1E pair the gallery hands `Task_Spawn` for the helper it raises in state 3,
/// the same shape `D_mine_mesa_80189B38` has.
extern GpStateBD8 D_neo_ark_submarine_gallery_8018591C;

/// Staging save location the gallery commits: `field_2` / `field_4` / `field_1`
/// hold what `func_neo_ark_submarine_gallery_8017EA0C` copies out of the
/// incoming location, and `func_neo_ark_submarine_gallery_8017E86C` moves those
/// same three bytes into `Mc_SaveData.field_6` / `field_8` / `field_5`.
extern GpSaveLoc D_neo_ark_submarine_gallery_80185924;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern s32 func_neo_ark_submarine_gallery_8017EC24(u16 arg0, s32 arg1);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017D678);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_submarine_gallery/neo_ark_submarine_gallery", func_neo_ark_submarine_gallery_8017E2CC);

/// Runs the gallery's save sequence once state 0 has asked for the caption.
/// State 1 waits for that caption, state 2 takes the confirm key or backs out,
/// state 3 raises the helper task 0x31 and queues the sound event, state 4
/// waits for that voice, and state 5 - the commit - copies the staged location
/// into `Mc_SaveData` and reloads. Every state advances by one except a
/// confirmed cancel and the commit itself.
void func_neo_ark_submarine_gallery_8017E86C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(9, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                arg0->state++;
            }
            break;
        case 2:
            if (Gp_GetCapEventKey() != 0xA) {
                Task_Kill(arg0);
                Gp_MsgPlayerWeapon(1);
                D_801153F4 = 0;
                break;
            }
            D_801153F4 = 1;
            Gp_TriggerPeIfArmed();
            arg0->state++;
            break;
        case 3:
            D_neo_ark_submarine_gallery_8018591C.field_0 = 0;
            D_neo_ark_submarine_gallery_8018591C.field_1 = 0;
            D_neo_ark_submarine_gallery_8018591C.field_2 = 0x1E;
            Task_Spawn(1, 0x31, 0, (s32)&D_neo_ark_submarine_gallery_8018591C);
            SndEvt_EnqueueType6(0x551E0001, 0, 0);
            arg0->state++;
            break;
        case 4:
            if (SndVoice_HasActiveId(0x551E0001) == 0) {
                arg0->state++;
            }
            break;
        case 5:
            D_80071076          = 1;
            Mc_SaveData.field_6 = D_neo_ark_submarine_gallery_80185924.field_2;
            Mc_SaveData.field_8 = D_neo_ark_submarine_gallery_80185924.field_4;
            Mc_SaveData.field_5 = D_neo_ark_submarine_gallery_80185924.field_1;
            Task_Spawn(0, 0x11, 0x10, 0);
            Task_Kill(arg0);
            break;
    }
}

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
