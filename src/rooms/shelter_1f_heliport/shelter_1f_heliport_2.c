#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"

#include "rooms/room_common.h"

extern void func_80131FBC(void);
extern void func_80132038(void);
extern void func_80132110(void);
extern void func_801322A0(void);
extern void func_80149E38(void);
extern void func_80149EBC(void);

extern TaskDesc D_80136CDC;
extern TaskDesc D_shelter_1f_heliport_801811C8;

extern SVECTOR D_shelter_1f_heliport_80181204;
extern s16     D_shelter_1f_heliport_80181206; // D_shelter_1f_heliport_80181204.vy

void func_shelter_1f_heliport_8018085C(GsCOORDINATE2* coord, SVECTOR* offset);

/// Event parameters latched into the room's pending event when a stage-3
/// request starts it: the cap command the event task runs, the stage sound it
/// then plays, the game flag checked and set on start, and a flag that makes
/// the event task start helper task 0x31.
typedef struct _Shelter1fHeliportEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} _Shelter1fHeliportEvent;

extern void func_80179B14(RoomEventMsg* src, RoomEventMsg* dst);

extern s16 D_80071076;
extern u8  D_801153F4;
extern u8  D_80115690;

extern TaskDesc                D_shelter_1f_heliport_80181194;
extern GpStateBD8              D_shelter_1f_heliport_80182CA0;
extern RoomEventMsg            D_shelter_1f_heliport_80182CA8;
extern s8                      D_shelter_1f_heliport_80182CB0;
extern _Shelter1fHeliportEvent D_shelter_1f_heliport_80182CB4;

/// The room's own event task, spawned by its message handler. State 0 runs
/// the latched event's CAP command; state 1 waits for it to finish and, when
/// the event asks for it, starts helper task 0x31; states 2 and 3 play the
/// event's stage sound and wait for it; state 4 writes the latched message's
/// destination into the save data and hands over to task type 0x11.
void func_shelter_1f_heliport_8017FF08(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd(D_shelter_1f_heliport_80182CB4.field_0, 0);
            D_80115690 = 1;
            arg0->state++;
            break;
        case 1:
            if (Gp_CapBusy() == 0) {
                if (D_shelter_1f_heliport_80182CB4.field_A != 0) {
                    D_shelter_1f_heliport_80182CA0.field_0 = 0;
                    D_shelter_1f_heliport_80182CA0.field_1 = 0;
                    D_shelter_1f_heliport_80182CA0.field_2 = 0x1E;
                    Task_Spawn(1, 0x31, 0, (s32)&D_shelter_1f_heliport_80182CA0);
                }
                arg0->state++;
            }
            break;
        case 2:
            if (D_shelter_1f_heliport_80182CB4.field_4 != 0) {
                Gp_EnqueueStageSnd6(D_shelter_1f_heliport_80182CB4.field_4, 0, 0);
                arg0->state++;
            } else {
                arg0->state = 4;
            }
            break;
        case 3:
            if (SndVoice_HasActiveId(Gp_PackStageSndId(D_shelter_1f_heliport_80182CB4.field_4)) == 0) {
                arg0->state++;
            }
            break;
        case 4:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_shelter_1f_heliport_80182CA8.msgId;
            Mc_SaveData.at4.loc.warp = D_shelter_1f_heliport_80182CA8.field_2;
            Mc_SaveData.at4.loc.room = D_shelter_1f_heliport_80182CA8.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
    }
}

/// State handlers of the room's controller task.
INCLUDE_RODATA("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", D_shelter_1f_heliport_8017D710);

static __inline__ s32 _shelter1fHeliportStartEvent(RoomEventMsg* dst, _Shelter1fHeliportEvent* event)
{
    D_shelter_1f_heliport_80182CB0 = 0;
    if (GameFlag_GetNibble(event->field_8) == 0 || event->field_8 == 0) {
        if (dst->field_5 == 0) {
            D_shelter_1f_heliport_80182CA8 = *dst;
            D_shelter_1f_heliport_80182CB4 = *event;
            if (event->field_8 != 0) {
                GameFlag_SetNibble(event->field_8, 1);
            }
            Task_SpawnFromTable(&D_shelter_1f_heliport_80181194, 0, 0, 0);
            D_shelter_1f_heliport_80182CB0 = 1;
        }
        return 2;
    }
    return 1;
}

s32 func_shelter_1f_heliport_801800A0(Task* task, s32 msgId, RoomEventMsg* src, RoomEventMsg* dst)
{
    _Shelter1fHeliportEvent event;

    *dst = *src;
    func_80179B14(src, dst);
    if (src->msgId == 0x1C && src->field_5 == 0) {
        SndEvt_EnqueueType7(0x55040006, 1);
        SndEvt_EnqueueType7(0x55040007, 1);
    }
    if (src->msgId == 3) {
        if (GameFlag_GetNibble(0xE3) == 0 && gGameSession->at4.loc.place == 1) {
            if (src->field_5 == 0) {
                Gp_RunCapCmd1(0x2B);
            }
            return 2;
        }
        event.field_0 = 0x29;
        event.field_4 = 0x55040001;
        event.field_8 = 0;
        event.field_A = 1;
        if (src->field_5 == 0) {
            SndEvt_EnqueueType7(0x55040006, 1);
            SndEvt_EnqueueType7(0x55040007, 1);
        }
        return _shelter1fHeliportStartEvent(dst, &event);
    }
    return 1;
}

void func_shelter_1f_heliport_801802AC(s32 arg0)
{
    Task* task;
    Task* slotA;

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xE4) == 1) {
        D_shelter_1f_heliport_80181206 = 0;
    } else {
        D_shelter_1f_heliport_80181206 = 0x2710;
    }
    func_shelter_1f_heliport_8018085C(((TmdObject*)task->extra)->coords, &D_shelter_1f_heliport_80181204);
}

s32 func_shelter_1f_heliport_80180334(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x124 && GameFlag_GetNibble(0xE4) == 1 && gameGetPtrSlot(0xA) != NULL) {
        found = 0;
        node  = Gp_PendingObj4C;
        while (node != NULL) {
            if (node->field_46 == 5 && node->field_48 == 0xFF && node->field_4B != 0) {
                found = 1;
                break;
            }
            node  = node->next;
            found = 0;
        }

        if (found != 0) {
            gGameSession->hideHud    = 1;
            gGameSession->eventState = 1;
            GameFlag_SetNibble(0x4B, 9);
            Task_SpawnOnDefaultList(&D_80136CDC, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_shelter_1f_heliport_8018041C(s32 arg0, s32 arg1, s32 arg2)
{
    s32 need;

    switch (arg2) {
        case 0x21:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&D_shelter_1f_heliport_801811C8, 0, 0x21, 0);
            break;
        case 0x22:
            need = 1;
            if (gGameSession->at4.loc.place == 1) {
                need = 2;
            }
            Gp_SpawnIfCapIdle(GameFlag_GetNibble(0x104) >= need ? 0x22 : 0x25, 0);
            break;
    }
    return 0;
}

s32 func_shelter_1f_heliport_801804BC(s32 arg0, s32 arg1, RoomEventMsg* in)
{
    switch (in->field_2) {
        case 1:
            if (gGameSession->at4.loc.place == 1) {
                func_80149EBC();
            }
            if (gGameSession->at4.loc.place == 2) {
                func_80149E38();
            }
            break;
        case 2:
            func_80132038();
            break;
        case 3:
            func_80131FBC();
            break;
        case 4:
            func_80132110();
            break;
        case 5:
            func_801322A0();
            break;
    }
    return 0;
}
