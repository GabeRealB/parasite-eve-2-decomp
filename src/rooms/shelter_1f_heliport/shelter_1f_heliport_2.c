#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
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
/// request starts it: a cap command, a stage sound, the game flag checked and
/// set on start, and a trailing byte the event controller reads.
typedef struct _Shelter1fHeliportEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} _Shelter1fHeliportEvent;

extern void func_80179B14(GpSaveLoc* src, GpSaveLoc* dst);

extern TaskDesc                D_shelter_1f_heliport_80181194;
extern GpSaveLoc               D_shelter_1f_heliport_80182CA8;
extern s8                      D_shelter_1f_heliport_80182CB0;
extern _Shelter1fHeliportEvent D_shelter_1f_heliport_80182CB4;

INCLUDE_ASM("rooms/nonmatchings/shelter_1f_heliport/shelter_1f_heliport_2", func_shelter_1f_heliport_8017FF08);

static __inline__ s32 _shelter1fHeliportStartEvent(GpSaveLoc* dst, _Shelter1fHeliportEvent* event)
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

s32 func_shelter_1f_heliport_801800A0(Task* task, s32 msgId, GpSaveLoc* src, GpSaveLoc* dst)
{
    _Shelter1fHeliportEvent event;

    *dst = *src;
    func_80179B14(src, dst);
    if (*(u16*)src == 0x1C && src->field_5 == 0) {
        SndEvt_EnqueueType7(0x55040006, 1);
        SndEvt_EnqueueType7(0x55040007, 1);
    }
    if (*(u16*)src == 3) {
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
