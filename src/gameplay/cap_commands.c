#include "gameplay/captions.h"

#include "types.h"

#include "gameplay/cap.h"
#include "cap.h"
#include "captions.h"
#include "gameplay/direction.h"
#include "gameplay/items.h"
#include "gameplay/message.h"
#include "message.h"
#include "gameplay/object_task.h"
#include "object_task.h"
#include "gameplay/player_actor.h"
#include "player_actor.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/text.h"
#include "main/wipsys.h"

GpCmdReply D_801155A0;

extern TaskDesc Gp_EvtSpawnTable[3];

extern AnimationPlayRequest D_8010FB10;

extern AnimationPlayRequest D_8010FB24;

extern AnimationPlayRequest Gp_WeaponMsgRec;

static const TaskFuncTable3 D_800974C8;

void Gp_EvtCapWeaponTask(Task* arg0);

static void func_800E4020(Task* task);

TaskDesc Gp_EvtSpawnTable[3] = {
    { 0, 32, Gp_EvtCapTask, { NULL } },
    { 0, 32, Gp_EvtCapWeaponTask, { NULL } },
    { 0xFFFF, 0, NULL, { NULL } },
};

AnimationPlayRequest D_8010FB10 = { { 1 }, 32, ANIMATION_BLEND_INTERPOLATE, 5, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest D_8010FB24 = { { 1 }, 33, ANIMATION_BLEND_RESET, 0, ANIMATION_WORLD_COLLISION_DISABLE };

AnimationPlayRequest Gp_WeaponMsgRec = { { 1 }, 1, ANIMATION_BLEND_INTERPOLATE, 8, ANIMATION_WORLD_COLLISION_DISABLE };

TaskDesc D_8010FB4C[3] = {
    { 0, 32, func_800E6EF4, { NULL } },
    { 0, 32, Gp_DelayedMsgTask, { NULL } },
    { 0xFFFF, 0, NULL, { NULL } },
};

GlyphUvwh D_8010FB70[4] = {
    { 16, 96, 16, 16 },
    { 32, 96, 16, 16 },
    { 16, 112, 16, 16 },
    { 32, 112, 16, 16 },
};

s32 D_8010FB80 = 8;

s32 D_8010FB84 = 0;

s32 Gp_CapCaretGrey = 8;

s32 Gp_CapCaretDir = 0;

static const TaskFuncTable3 D_800974C8 = { {
    func_800E31E8,
    func_800E4020,
    taskKill,
} };

void Gp_RunCapCmd(s32 arg0, s16 arg1)
{
    GpCapCmd* rec;
    s32       flagId;
    s32       val;
    s32       i;

    for (;;) {
        rec    = Gp_CapCmds[arg0].command;
        flagId = rec->field_3 | (rec->field_7 << 8);
        switch (rec->field_0) {
            case 0:
                Gp_StartCapSlot(arg0, arg1, 0);
                return;
            case 1:
                if (rec->field_1 & 2) {
                    val = GameFlag_GetNibble(flagId);
                } else {
                    val = rec->field_4;
                }
                if ((rec->field_1 & 4) && rec->field_2 < val) {
                    arg0 = rec->field_8;
                    continue;
                }
                Gp_StartCapSlot(arg0, arg1, val);
                if ((val < rec->field_2) || (rec->field_1 & 4)) {
                    val++;
                } else if (rec->field_1 & 1) {
                    val = 0;
                }
                if (rec->field_1 & 2) {
                    GameFlag_SetNibble(flagId, val);
                } else {
                    rec->field_4 = val;
                }
                return;
            case 2:
                val = GameFlag_GetNibble(flagId);
                Gp_StartCapSlot(arg0, arg1, val);
                return;
            case 3:
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, arg0, 0);
                return;
            case 4:
                val = 0;
                for (i = 0; i < rec->field_6; i++) {
                    if (Gp_GetCurBit2Flag(rec->field_5 + i) == 0 ||
                        Gp_GetCurBit2Flag(rec->field_5 + i) == 1 ||
                        Gp_GetCurBit2Flag(rec->field_5 + i) == 3) {
                        val++;
                    }
                }
                if ((rec->field_1 & 4) && val == 0) {
                    arg0 = rec->field_8;
                    continue;
                }
                Gp_StartCapSlot(arg0, arg1, val);
                return;
            default:
                return;
        }
    }
}

void Gp_EvtCapWeaponTask(Task* arg0)
{
    s32                  flags;
    GameActor*           actor;
    s32                  mode;
    AnimationPlayRequest recB;
    AnimationPlayRequest recA;

    flags = arg0->spawnArg2.value;
    actor = gameGetPtrSlot(3)->work;
    switch (arg0->state) {
        case 0:
            if ((flags & 1) && (flags != 0xFF)) {
                recA              = Gp_WeaponMsgRec;
                recA.source.index = Gp_WeaponIdBase[Mc_SaveData[0].state.characterId - 1] + Player_Status.weapon;
                Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &recA, 0);
            }
            recB              = D_8010FB10;
            recB.source.index = Gp_WeaponIdBase[Mc_SaveData[0].state.characterId - 1] + Player_Status.weapon;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 0, 0);
            arg0->state++;
            break;
        case 1:
            arg0->state++;
            break;
        case 2:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                taskKill(arg0);
            }
            break;
        case 3:
            if ((flags & 1) && (flags != 0xFF)) {
                Gp_StateF0.field_4 = 1;
            }
            if ((flags & 2) && (flags != 0xFF)) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 0, 0);
            }
            if ((flags & 4) && (flags != 0xFF)) {
                mode = 2;
            } else if ((flags & 1) == 0) {
                mode = 3;
            } else {
                mode = 0;
            }
            if (flags == 0xFF) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F0, arg0->spawnArg1.value, mode);
            } else {
                Gp_RunCapCmd(arg0->spawnArg1.value, mode);
            }
            arg0->state++;
            break;
        case 4:
            if (Gp_CapBusy() == 0) {
                Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, 1, 0);
                arg0->state++;
            }
            break;
        case 5:
            if (D_80115598 != 0) {
                Gp_DispatchMsg(gameGetPtrSlot(7), 0x13F2, arg0->spawnArg2.value + 0x64, 0);
            }
            recB              = D_8010FB24;
            recB.source.index = Gp_WeaponIdBase[Mc_SaveData[0].state.characterId - 1] + Player_Status.weapon;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3FA, 1, 0);
            arg0->state++;
            break;
        case 6:
            arg0->state++;
            break;
        case 7:
            if (Gp_DispatchMsg(gameGetPtrSlot(3), 0x3ED, 0, 0) == 0) {
                arg0->state++;
            }
            if (actor->field_954 != 2) {
                taskKill(arg0);
                Gp_StateF0.field_4 = 0;
            }
            break;
        case 8:
            taskKill(arg0);
            Gp_StateF0.field_4 = 0;
            Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
            break;
    }
}

const char         Gp_StrCapMagic[] = "CAP";
const _GpCapLayout D_80097518       = { 0 };
const char         Gp_StrEvsFmt[]   = "evs%d_%d_%d.txt";

const TaskFuncTable3 Gp_CapTaskStates = { {
    Gp_InitCapTask,
    Gp_CapTaskState1,
    taskKill,
} };

void Gp_SetNibbleIf(s32 arg0, s32 arg1)
{
    if (arg0 != 0) {
        GameFlag_SetNibble(arg0, arg1);
    }
}

void Gp_RunCapCmd1(s32 arg0)
{
    Gp_RunCapCmd(arg0, 1);
}

void Gp_MsgPlayer3F3(s32 arg0)
{
    Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F3, arg0, 0);
}

void Gp_MsgPlayerWeapon(s32 arg0)
{
    AnimationPlayRequest sp;

    if (arg0 == 0) {
        sp              = Gp_WeaponMsgRec;
        sp.source.index = Gp_WeaponIdBase[Mc_SaveData[0].state.characterId - 1] + Player_Status.weapon;
        Gp_DispatchMsgPtr(gameGetPtrSlot(3), ANIMATION_MESSAGE_PLAY, &sp, 0);
    } else {
        Gp_DispatchMsg(gameGetPtrSlot(3), 0x3F1, 0, 0);
    }
}

void Gp_MsgSlot4Chain(s32 arg0, s32 arg1)
{
    Task* out;

    arg0 = (arg0 << 12) | (gGameSession->location.loc.stage << 8) | gGameSession->location.loc.area;
    Gp_DispatchMsgReply(gameGetPtrSlot(4), 0x7D0, arg0, &out);
    if (out != 0) {
        Gp_DispatchMsg(out, 0x7D5, arg1, 0);
    }
}

void Gp_PlayerWeaponId(s32* arg0)
{
    *arg0 = Gp_WeaponIdBase[Mc_SaveData[0].state.characterId - 1] + Player_Status.weapon;
}

void Gp_AllyAnimId(s32* arg0)
{
    *arg0 = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
}

void Gp_FillPlayerHpMp(void)
{
    PlayerStatus* p;

    p     = &Player_Status;
    p->hp = p->hpMax;
    p->mp = p->mpMax;
}

void Gp_FillAllyHp(void)
{
    Mc_SaveData[0].state.companionHp = Mc_SaveData[0].state.companionHpMax;
}

void Gp_SpawnIfCapIdle(s32 arg0, s32 arg1)
{
    if (Gp_CapBusy() == 0) {
        Task_SpawnFromTable(Gp_EvtSpawnTable, 0, arg1, arg0);
    }
}

void Gp_EnqueueStageSnd6(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    SndEvt_EnqueueType6(arg0, (s8)arg1, (s8)arg2);
}

s32 Gp_PackStageSndId(s32 arg0)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    return arg0;
}

void Gp_EnqueueStageSnd7(s32 arg0, s32 arg1)
{
    if (arg0 & 0xF000000) {
        arg0 &= 0xF0FFFFFF;
        arg0 |= gGameSession->location.loc.stage << 24;
    }
    SndEvt_EnqueueType7(arg0, arg1 & 0xFFFF);
}

void Gp_MsgAlly3F3(s32 arg0)
{
    Task* slot;

    slot = gameGetPtrSlot(0xA);
    if (slot != NULL) {
        Gp_DispatchMsg(slot, 0x3F3, arg0, 0);
    }
}

void Gp_MsgAllyWeapon(s32 arg0)
{
    Task*                slot;
    AnimationPlayRequest sp;

    slot = gameGetPtrSlot(0xA);
    if (slot != NULL) {
        if (arg0 == 0) {
            sp              = Gp_WeaponMsgRec;
            sp.source.index = Gp_AllyIdBase[Mc_SaveData[0].state.companionType - 1] + Mc_SaveData[0].state.companionVariant;
            Gp_DispatchMsgPtr(slot, ANIMATION_MESSAGE_PLAY, &sp, 0);
        } else {
            Gp_DispatchMsg(slot, 0x3F1, 0, 0);
        }
    }
}

void func_800E3FAC(s32 arg0, s32 arg1)
{
    gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[arg0 / 2] = arg1;
}

s32 func_800E3FCC(s32 arg0)
{
    return gGameFlagNibbleBanks[GAME_FLAG_NIBBLE_BANK_LIVE].payload.packedFlags[arg0 / 2];
}

/// Location-message fallback of `D_8010FAD4`, the table installed on pointer
/// slot 7: copies the requested location onto the outgoing record and answers
/// 1, leaving the decision to whoever reads the reply.
s32 func_800E3FF0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    *dst = *src;
    return 1;
}

s32 func_800E4018(void)
{
    return 0;
}

static void func_800E4020(Task* task)
{
}

void func_800E4028(Task* arg0)
{
    TaskFuncTable3 sp;

    sp = D_800974C8;
    sp.funcs[arg0->state](arg0);
}

void Gp_ClearAllFlagNibbles(void)
{
    s32 i;

    for (i = 0; i < GAME_FLAG_NIBBLE_COUNT; i++) {
        GameFlag_SetNibble(i, 0);
    }
}

void Gp_SpawnEvt1(s32 arg0, s32 arg1)
{
    Task_SpawnFromTable(Gp_EvtSpawnTable, 1, arg0, arg1);
}
