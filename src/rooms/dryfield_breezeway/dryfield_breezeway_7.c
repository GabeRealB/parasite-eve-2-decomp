#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/dryfield_breezeway.h"
#include "rooms/room_common.h"

extern s16 D_80071076;
extern u8  D_801153F4;

/// The message and request the gate latched, and the descriptor of the event
/// task it spawns to act on them.
extern RoomEventMsg D_dryfield_breezeway_8018439C;
extern RoomEventReq D_dryfield_breezeway_801843AC;
extern TaskDesc     D_dryfield_breezeway_80181DD4;

/// The room's event gate, called by `func_dryfield_breezeway_8017D940` with the
/// request it builds on the stack. A set flag nibble (or a clear one, for a
/// negative `flagId`) means the event has already happened and the answer is
/// 1; a missing collected-bit prerequisite runs the request's `field_4` CAP
/// command and answers 0; otherwise the request and message are latched into
/// `D_dryfield_breezeway_801843AC` / `D_dryfield_breezeway_8018439C`, the flag
/// nibble is written, the event task is spawned and
/// `D_dryfield_breezeway_801843A4` is raised, for 2. A non-zero `field_5` on
/// the message asks what would happen and suppresses all of those effects.
s32 func_dryfield_breezeway_8017D638(RoomEventReq* req, RoomEventMsg* msg)
{
    s32 flag;
    s32 id;
    s32 mode;
    s32 got;
    s32 ret;
    s32 neg;

    flag                          = req->flagId;
    D_dryfield_breezeway_801843A4 = 0;
    neg                           = flag < 0;
    got                           = (s16)flag;
    if (neg) {
        flag = -flag;
        got  = GameFlag_GetNibble(flag) == 0;
    } else {
        got = GameFlag_GetNibble(got);
    }
    ret = 1;
    if (got == 0) {
        if (Gp_HasCollectedBit(req->itemId) != 0 || req->itemId == 0) {
            ret = 2;
            if (msg->field_5 == 0) {
                D_dryfield_breezeway_8018439C = *msg;
                D_dryfield_breezeway_801843AC = *req;
                id                            = req->flagId;
                mode                          = 1;
                if (id < 0) {
                    id   = -id;
                    mode = 0;
                }
                GameFlag_SetNibble(id, mode);
                Task_SpawnFromTable(&D_dryfield_breezeway_80181DD4, 0, 0, 0);
                D_dryfield_breezeway_801843A4 = 1;
                return 2;
            }
            return ret;
        }
        ret = 0;
        if (msg->field_5 == 0) {
            Gp_RunCapCmd1(req->field_4);
            Gp_SetNibbleIf(msg->field_6, 2);
            ret = 0;
        }
        return ret;
    }
    return ret;
}

/// The event task `D_dryfield_breezeway_80181DD4` describes, spawned by the gate
/// above once it has latched a request: it runs the request's CAP command,
/// plays and waits out its two sounds (`field_8`, then `field_C`, either
/// skipped when zero), then writes the latched message's destination into the
/// save's location and spawns the room-change task, killing itself.
void func_dryfield_breezeway_8017D79C(Task* task)
{
    switch (task->state) {
        case 0:
            D_801153F4 = 1;
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(D_dryfield_breezeway_801843AC.field_0);
            if (D_dryfield_breezeway_801843AC.field_8 != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_8, 0, 0);
                task->state++;
            } else {
                task->state = 2;
            }
            break;
        case 1:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_8) == 0) {
                task->state++;
            }
            break;
        case 2:
            task->state++;
            break;
        case 3:
            if (D_dryfield_breezeway_801843AC.field_C != 0) {
                SndEvt_EnqueueType6(D_dryfield_breezeway_801843AC.field_C, 0, 0);
                task->state++;
            } else {
                task->state = 5;
            }
            break;
        case 4:
            if (SndVoice_HasActiveId(D_dryfield_breezeway_801843AC.field_C) == 0) {
                task->state++;
            }
            break;
        case 5:
            SndEvt_EnqueueType7(0x80000000, 0);
            D_80071076               = 1;
            Mc_SaveData.at4.loc.area = D_dryfield_breezeway_8018439C.msgId;
            Mc_SaveData.at4.loc.warp = D_dryfield_breezeway_8018439C.field_2;
            Mc_SaveData.at4.loc.room = (u8)D_dryfield_breezeway_8018439C.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(task);
            break;
    }
}
