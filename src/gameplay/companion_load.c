#include "gameplay/companion_load.h"

#include "common.h"

#include "gameplay/collision.h"
#include "companion_load.h"
#include "gameplay/hud_sprites.h"
#include "loading.h"
#include "gameplay/message.h"
#include "player_state.h"
#include "gameplay/room.h"
#include "scene_runtime.h"
#include "gameplay/world_collision.h"
#include "world_collision.h"
#include "gameplay/world_state.h"
#include "gameplay/world_targets.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/gameflow.h"
#include "main/gfx.h"
#include "main/mc.h"
#include "main/pad.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/wipsys.h"

/// 8-byte record in `D_80114198` / `D_801141F0` / `D_80114248`. Indexed by
/// `GameFlag_GetNibble(0x4B / 0x4C / 0x4D)`. `field_0` is a per-room byte
/// list, 1-based by `Mc_SaveData[0].at4.loc.area`; `field_4` is the stage id
/// (`Mc_SaveData[0].at4.loc.stage`). `Gp_ApplyNpcRoomSnd` tests the room byte (second
/// table with `& 0xF`) to choose the `Snd_SetModeFlag` argument.
/// `Gp_PickCompanion` uses the same tables to pick `Mc_SaveData[0].companionType`.
typedef struct _GpNpcRoomRec {
    /* 0x0 */ u8*  field_0;
    /* 0x4 */ u8   field_4;
    /* 0x5 */ byte pad_5[3];
} GpNpcRoomRec;
STATIC_ASSERT_SIZEOF(GpNpcRoomRec, 8);

extern GpNpcRoomRec D_80114198[];

extern GpNpcRoomRec D_801141F0[];

extern GpNpcRoomRec D_80114248[];

extern u8 D_80114258[38];

extern u8 D_80114280[38];

extern u8 D_801142A8[38];

extern u8 D_801142D0[49];

extern u8 D_80114304[49];

extern u8 D_80114338[38];

extern u8 D_80114360[38];

extern u8 D_80114388[38];

extern u8 D_801143B0[38];

extern u8 D_801143D8[38];

extern u8 D_80114400[38];

extern u8 D_80114428[33];

extern u8 D_8011444C[33];

extern u8 D_80114470[49];

extern u8 D_801144A4[49];

extern u8 D_801144D8[49];

extern u8 D_8011450C[33];

extern u8 D_80114530[49];

/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
extern u8 D_80114564[36];

static void Gp_ClearFlagBank(s32 arg0);

void func_80724E2C(void);

extern TaskDesc D_80183824[];

GpNpcRoomRec D_80114198[11] = {
    { NULL, 0, { 0, 0, 0 } },
    { D_80114360, 2, { 0, 0, 0 } },
    { D_80114388, 2, { 0, 0, 0 } },
    { D_801143B0, 2, { 0, 0, 0 } },
    { D_80114388, 2, { 0, 0, 0 } },
    { D_801143D8, 3, { 0, 0, 0 } },
    { D_80114388, 3, { 0, 0, 0 } },
    { D_80114400, 3, { 0, 0, 0 } },
    { D_80114428, 5, { 0, 0, 0 } },
    { D_8011444C, 5, { 0, 0, 0 } },
    { D_80114470, 4, { 0, 0, 0 } },
};
GpNpcRoomRec D_801141F0[11] = {
    { NULL, 0, { 0, 0, 0 } },
    { D_80114258, 3, { 0, 0, 0 } },
    { D_80114280, 3, { 0, 0, 0 } },
    { D_801142A8, 3, { 0, 0, 0 } },
    { D_801142D0, 4, { 0, 0, 0 } },
    { D_80114304, 4, { 0, 0, 0 } },
    { D_80114338, 3, { 0, 0, 0 } },
    { D_801144A4, 4, { 0, 0, 0 } },
    { D_801144D8, 4, { 0, 0, 0 } },
    { D_8011450C, 5, { 0, 0, 0 } },
    { D_80114530, 4, { 0, 0, 0 } },
};
GpNpcRoomRec D_80114248[2] = {
    { NULL, 0, { 0, 0, 0 } },
    { D_80114564, 5, { 0, 0, 0 } },
};
u8 D_80114258[38] = { 17, 0, 17, 0, 17, 17, 17, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114280[38] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142A8[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801142D0[49] = { 17, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114304[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 49, 49, 49, 49, 49, 49, 0, 0, 0 };
u8 D_80114338[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0 };
u8 D_80114360[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114388[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143B0[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801143D8[38] = { 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114400[38] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114428[33] = { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_8011444C[33] = { 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114470[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_801144A4[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_801144D8[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 33, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0 };
u8 D_8011450C[33] = { 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
u8 D_80114530[49] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 65, 65, 65, 65, 65, 65, 0, 0, 0 };
/// 33 room flags, then the unexplained 3D F0 71 tail.
/// The tail is not a room-mask entry.
u8 D_80114564[36] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 61, 240, 113 };

s32 Gp_PickCompanion(void)
{
    McSaveData* save;
    u8*         bytes;
    s32         stage;
    u8          variant;

    save  = &Mc_SaveData[0];
    stage = save->at4.loc.stage;
    bytes = D_80114198[GameFlag_GetNibble(0x4B)].field_0;
    if (bytes != NULL && D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage && bytes[save->at4.loc.area - 1] != 0) {
        GameSession* sess = gGameSession;

        save->companionType    = 2;
        save->companionVariant = 0;
        return (sess->companionType != 2) * 2;
    }

    bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
    if (bytes != NULL && D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage && (bytes[Mc_SaveData[0].at4.loc.area - 1] & 0xF)) {
        GameSession* sess = gGameSession;

        Mc_SaveData[0].companionType = 1;
        if (sess->companionType == 1) {
            variant = bytes[Mc_SaveData[0].at4.loc.area - 1] >> 4;
            if (sess->companionVariant == variant) {
                Mc_SaveData[0].companionVariant = variant;
                return 0;
            }
        }
        Mc_SaveData[0].companionVariant = bytes[Mc_SaveData[0].at4.loc.area - 1] >> 4;
        gGameSession->companionVariant  = bytes[Mc_SaveData[0].at4.loc.area - 1] >> 4;
        return 1;
    }

    bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
    if (bytes != NULL && D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage && bytes[Mc_SaveData[0].at4.loc.area - 1] != 0) {
        GameSession* sess = gGameSession;

        Mc_SaveData[0].companionType    = 3;
        Mc_SaveData[0].companionVariant = 0;
        if (sess->companionType == 3) {
            return 0;
        }
        return 3;
    }

    Mc_SaveData[0].companionType    = 0;
    Mc_SaveData[0].companionVariant = 0;
    gGameSession->companionType     = 0;
    gGameSession->companionVariant  = 0;
    return 0;
}

void Gp_ApplyNpcRoomSnd(void)
{
    McSaveData* save;
    u8*         bytes;
    s32         stage;
    s32         flag;

    save  = &Mc_SaveData[0];
    stage = save->at4.loc.stage;
    if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) != GP_LOC_KEY(3, 32, 0, 0)) {
        bytes = D_80114198[GameFlag_GetNibble(0x4B)].field_0;
        if (bytes != NULL) {
            if (D_80114198[GameFlag_GetNibble(0x4B)].field_4 == stage) {
                if (bytes[save->at4.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_801141F0[GameFlag_GetNibble(0x4C)].field_0;
        if (bytes != NULL) {
            if (D_801141F0[GameFlag_GetNibble(0x4C)].field_4 == stage) {
                if (bytes[Mc_SaveData[0].at4.loc.area - 1] & 0xF) {
                    flag = 1;
                    goto done;
                }
            }
        }
        bytes = D_80114248[GameFlag_GetNibble(0x4D)].field_0;
        if (bytes != NULL) {
            if (D_80114248[GameFlag_GetNibble(0x4D)].field_4 == stage) {
                if (bytes[Mc_SaveData[0].at4.loc.area - 1] != 0) {
                    flag = 1;
                    goto done;
                }
            }
        }
    }
    flag = 0;
done:
    Snd_SetModeFlag(flag);
}

void Gp_SetupCompanionActor(GpActorArg* arg0, u16* arg1)
{
    McSaveData* save;
    s32         field;

    save  = &Mc_SaveData[0];
    field = save->companionType;
    if (field != 0) {
        if (field == 2) {
            Gp_SpawnAlly(arg0, save->companionType, GameFlag_GetNibble(0x4B), arg1);
        } else {
            Gp_SpawnAlly(arg0, field, 0, arg1);
        }
    }
}

static void Gp_ClearFlagBank(s32 arg0)
{
    GpFlagBank* bank;

    bank             = Gp_FlagBanks[arg0];
    bank->field_4[0] = 0;
    bank->field_4[1] = 0;
}

void Gp_MarkAreaVisited(GpAreaKey* arg0)
{
    McSaveData* save;
    GpFlagBank* bank;
    s32         which;
    s32         bit;
    s32         mask;
    s32         flags;

    bank = Gp_FlagBanks[arg0->stage];
    save = &Mc_SaveData[0];
    if ((((s8)save->visitFlags >> arg0->stage) & 1) == 0) {
        save->visitFlags |= 1 << arg0->stage;
        if (gDisplayState.field_112 != 0) {
            func_80724E2C();
        }
    }

    which = 0;
    if (arg0->area >= 0x21) {
        which = 1;
        bit   = arg0->area - 0x21;
    } else {
        bit = arg0->area - 1;
    }

    mask  = 1;
    flags = bank->field_4[which];
    if (((mask << bit) & flags) == 0) {
        bank->field_4[which] = flags | (mask << bit);
        Gp_SetAreaFlag0(arg0);
    }
}

void func_800ABFF8(void)
{
}

void func_800AC000(void)
{
}

void Gp_SessionState1(Task* task)
{
    DisplayState* ds;
    s32           temp;

    ds             = &gDisplayState;
    ds->skipDraw   = 1;
    ds->holdState |= 0x80;
    temp           = task->spawnArg1 & 0xF;
    if (temp != 0) {
        if (temp == 1) {
            ds->at100.flags.imageSource = 0;
        }
    }
    task->state++;
}

void Gp_ResumeSessionTask(Task* task)
{
    SndBank_SetEnableFlags(0, 0x40000000);
    if (gGameSession->deathVariant != 0) {
        taskKill(task);
        return;
    }
    if ((task->spawnArg1 & 0x10) == 0) {
        if (Gp_StateF0.prefix.bytes.field_0 == 2) {
            Gp_StateF0.prefix.bytes.field_0 = 3;
        }
        Gp_TriggerPeIfArmed();
    }
    task->state++;
}

void func_800AC0F0(Task* task)
{
    TaskFuncTable3 sp;

    sp = Gp_SessionStates;
    Pad_SetCooldown(0);
    *(volatile u8*)&Gp_StateF0.field_4 = 1;
    sp.funcs[((volatile Task*)task)->state](task);
}

void Gp_LoadFinishTask(Task* task)
{
    if (CdCmd_Queue.field_224 == 0) {
        Gpu_ClearOTag(0);
        Gpu_ClearOTag(1);
        Pad_RemapState->field_3 = 0;
        taskKill(task);
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            func_800AA548(1);
        } else {
            func_800AA548(0);
        }
        gDisplayState.holdCount  = 0;
        gDisplayState.holdState &= 0x7F;
        Display_AcquireRef();
        Task_Spawn(0, 0x21, 0, 0);
        if ((GP_LOC_WORD(gGameSession->at4.loc) & GP_LOC_STAGE_AREA) == GP_LOC_KEY(1, 5, 0, 0)) {
            Task_SpawnFromTable(D_80183824, 0, 0, 0);
            CdCmd_SetupMdecBuffers();
            CdCmd_SelectMdecBuffer();
        }
    }
}

void Gp_LoadStateTask(Task* task)
{
    TaskFuncTable8 sp;
    DisplayState*  ds;

    sp = Gp_LoadStateFns;
    Pad_SetCooldown(0);
    ds = &gDisplayState;
    if (ds->demoScene != 0) {
        if (Pad_ReadButtonsInv(0) & 0x800) {
            if (CdCmd_IsIdle() & 0xFFFF) {
                Wip_SysFlags.field_4 = 1;
                ds->gameMode         = 1;
                return;
            }
        }
    }
    sp.funcs[task->state](task);
}

void Gp_FlashWhiteTask(Task* task)
{
    CdCmdQueue* queue;
    u8          fade;

    queue = &CdCmd_Queue;
    switch (task->state) {
        case 0:
            task->killCountdown = 0;
            task->state++;
        case 1:
            Fade_DrawOverlay(0xFF, 0xFF, 0xFF, 2);
            task->killCountdown++;
            if (task->killCountdown < 3) {
                return;
            }
            task->killCountdown = 0xFF;
            task->state++;
            break;
        case 2:
            fade = task->killCountdown;
            Fade_DrawOverlay(fade, fade, fade, 2);
            task->killCountdown -= 0x1E;
            if (task->killCountdown > 0) {
                return;
            }
            if ((s16)queue->field_248 != 0) {
                queue->field_248 = 0;
                queue->field_244 = 0;
            }
            Display_ReleaseRef();
            taskKill(task);
            break;
    }
}

s32 Gp_DispatchMsg(Task* arg0, s32 arg1, s32 arg2, s32 arg3)
{
    GpMsgEntry* temp;
    GpMsgEntry* entry;

    temp = arg0->msgTable;
    if (temp == NULL) {
        return 0;
    }
    entry = temp;
    if (entry->id != arg1) {
        do {
            if (entry->id == 0x7FFFFFFF) {
                return 0;
            }
            entry++;
        } while (entry->id != arg1);
    }
    return entry->handler(arg0, arg1, arg2, arg3);
}

void Gp_LinkRoomObjectsSpawn(Task* task)
{
    GpAreaKey*    sess;
    GpRoomObjRec* recs;
    GpGridParams* grid;
    GpObj4A*      list1;
    GpObj4A*      list2;
    GpObj3A*      list3;
    s32           i;
    Task*         spawned;

    sess = &gGameSession->at4.loc;
    recs = Gp_RoomObjTables[sess->stage - 1]->field_0[sess->area - 1];
    if (recs != NULL) {
        grid  = recs[sess->room - 1].field_0;
        list1 = recs[sess->room - 1].field_4;
        list2 = recs[sess->room - 1].field_8;
        list3 = recs[sess->room - 1].field_C;
        if (grid != NULL) {
            grid->field_0 = &gGfxViewCoord;
            Gp_GridParams = grid;
        }
        if (list1 != NULL) {
            for (i = 0;; i++) {
                list1[i].field_8 = &gGfxViewCoord;
                Gp_LinkObj4A(1, &list1[i]);
                list1[i].field_4A |= 0x40;
                if (list1[i].field_4A & 0x80) {
                    break;
                }
            }
        }
        if (list2 != NULL) {
            for (i = 0;; i++) {
                list2[i].field_8 = &gGfxViewCoord;
                Gp_LinkObj4A(0, &list2[i]);
                list2[i].field_4A |= 0x40;
                if (list2[i].field_4A & 0x80) {
                    break;
                }
            }
        }
        if (list3 != NULL) {
            for (i = 0;; i++) {
                Gp_LinkObj3A(0, &list3[i]);
                list3[i].field_3A |= 0x40;
                if (list3[i].field_3A & 0x80) {
                    break;
                }
            }
        }
    }
    spawned = Task_Spawn(0, 0x1B, 0, 0);
    if (spawned != NULL) {
        Task_Reparent(task, spawned);
    }
    gGameSession->roomObjsDirty = 0;
    task->state++;
}
