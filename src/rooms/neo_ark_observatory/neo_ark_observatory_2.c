#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "main/tmd.h"
#include "rooms/neo_ark_observatory.h"

extern SVECTOR D_neo_ark_observatory_80181368;
extern s16     D_neo_ark_observatory_8018136A; // D_neo_ark_observatory_80181368.vy
extern s16     D_80071076;

void func_neo_ark_observatory_8017FE34(GsCOORDINATE2* coord, SVECTOR* offset);

extern void func_80132220(void);
extern s8   D_8007272D;

extern TaskDesc D_80137EE4;
extern TaskDesc D_80138694;
extern TaskDesc D_8013C72C;
extern TaskDesc D_8013CAEC;
extern TaskDesc D_8013FC58;
extern TaskDesc D_80140078;
extern TaskDesc D_neo_ark_observatory_80180DD4;
extern TaskDesc D_neo_ark_observatory_80181200;
extern TaskDesc D_neo_ark_observatory_801812C0;

/// A marker resolver: fills in the marker state for the room id in its first
/// record, writing through the second.
typedef s32 (*_MapMarkerResolve)(MapMarkerRec*, MapMarkerOut*);

/// Fills in the marker state of one room's spot on the map. Most rooms have no
/// marker; the six that do read a GameFlag nibble, either straight (plus one -
/// rooms 5 / 41 / 45) or folded into a fixed state (rooms 2, 16, 20). The
/// caller passes one buffer as both records, so the state lands on top of the
/// room id it was derived from.
s32 func_neo_ark_observatory_8017F44C(MapMarkerRec* arg0, MapMarkerOut* arg1)
{
    if (arg0->field_5 == 0) {
        switch (arg0->field_0) {
            case 2:
                if (GameFlag_GetNibble(0x10F) != 0) {
                    arg1->field_3 = 2;
                }
                if (GameFlag_GetNibble(0x11A) >= 2) {
                    arg1->field_3 = 3;
                }
                break;
            case 5:
                arg1->field_3 = GameFlag_GetNibble(0xA4) + 1;
                break;
            case 16:
                if (GameFlag_GetNibble(0x7A) >= 6) {
                    arg1->field_3 = 3;
                }
                break;
            case 20:
                switch (GameFlag_GetNibble(0xF4)) {
                    case 0:
                        arg1->field_3 = 1;
                        break;
                    case 1:
                        arg1->field_3 = 6;
                        break;
                    case 2:
                        arg1->field_3 = 7;
                        break;
                    case 3:
                        arg1->field_3 = 8;
                        break;
                    default:
                        arg1->field_3 = 1;
                        break;
                }
                break;
            case 45:
                arg1->field_3 = GameFlag_GetNibble(0xB7) + 1;
                break;
            case 41:
                arg1->field_3 = GameFlag_GetNibble(0xB6) + 1;
                break;
            case 3:
            case 4:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 24:
            case 25:
            case 26:
            case 27:
            case 28:
            case 29:
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
            case 38:
            case 39:
            case 40:
            case 42:
            case 43:
            case 44:
            default:
                break;
        }
    }
    return 1;
}

/// Departure task. State 0 stages the descriptor's halfword into a `GpMsg3EE`
/// record and sends it to the slot-3 game pointer as message 0x3EE - the
/// all-ones halfword is the "nothing staged" marker, and the task skips to
/// state 2 rather than sending it. State 1 polls that same pointer with 0x3F0,
/// states 2 and 3 queue the descriptor's sound event and wait for the voice to
/// go quiet, and each of them advances the state once its call reports 0.
/// State 4 commits the save location the descriptor names, re-spawns the
/// player task as type 0x11 and kills itself.
void func_neo_ark_observatory_8017F588(Task* arg0)
{
    GpMsg3EE msg;
    void*    slot;

    slot = gameGetPtrSlot(3);
    switch (arg0->state) {
        case 0:
            msg.field_12 = D_neo_ark_observatory_80187A34;
            if (msg.field_12 == -1) {
                arg0->state = 2;
                break;
            }
            Gp_DispatchMsg(slot, 0x3EE, (s32)&msg, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 1:
            if (Gp_DispatchMsg(slot, 0x3F0, 0, 0) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 2:
            if (D_neo_ark_observatory_80187A38 == 0) {
                arg0->state = 4;
                break;
            }
            SndEvt_EnqueueType6(D_neo_ark_observatory_80187A38, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            break;
        case 3:
            if (SndVoice_HasActiveId(D_neo_ark_observatory_80187A38) == 0) {
                arg0->state = (s32)(arg0->state + 1);
            }
            break;
        case 4:
            SndEvt_EnqueueType7((s32)0x80000000, 0);
            D_80071076                = 1;
            Mc_SaveData.at4.loc.stage = D_neo_ark_observatory_80187A30.field_0;
            Mc_SaveData.at4.loc.area  = D_neo_ark_observatory_80187A30.field_1;
            Mc_SaveData.at4.loc.warp  = D_neo_ark_observatory_80187A30.field_2;
            Mc_SaveData.at4.loc.room  = D_neo_ark_observatory_80187A30.field_3;
            Task_Spawn(0, 0x11, 0, 0);
            taskKill(arg0);
            break;
        default:
            break;
    }
}

/// Copies the destination bytes of `desc` into a marker record, lets `resolve`
/// rewrite the record in place, and copies the result back into `desc`.
static __inline__ void _neoArkObservatoryStageMarker(NeoArkObservatoryEventDesc* desc, _MapMarkerResolve resolve)
{
    MapMarkerRec rec;

    rec.field_0  = desc->field_1;
    rec.pad_2[0] = desc->field_2;
    rec.pad_2[1] = desc->field_3;
    rec.field_5  = 0;
    resolve(&rec, (MapMarkerOut*)&rec);
    desc->field_1 = rec.field_0;
    desc->field_2 = rec.pad_2[0];
    desc->field_3 = rec.pad_2[1];
}

s32 func_neo_ark_observatory_8017F6F8(Task* arg0, s32 arg1, GpMsg13EF* arg2, s32 arg3)
{
    NeoArkObservatoryEventDesc desc;
    _MapMarkerResolve          resolve;
    s32                        temp;

    if (arg2->field_2 == 0xA) {
        if (GameFlag_GetNibble(0xD1) == 2) {
            GameFlag_SetNibble(0x4C, 8);
        }
        if (GameFlag_GetNibble(0xF7) == 0) {
            temp = GameFlag_GetNibble(0xDF);
            if (temp == 1) {
                _MapMarkerResolve resolve;

                GameFlag_SetNibble(0xF7, 1);
                desc.field_0 = 4;
                desc.field_1 = 0x12;
                desc.field_2 = 3;
                desc.field_3 = temp;
                desc.field_8 = 0x55070005;
                desc.field_4 = 0x400;
                resolve      = func_neo_ark_observatory_8017F44C;
                Gp_MsgPlayerWeapon(0);
                _neoArkObservatoryStageMarker(&desc, resolve);
                D_neo_ark_observatory_80187A30 = desc;
                Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
                return 0;
            }
        }
        desc.field_0 = 4;
        desc.field_1 = arg2->field_3;
        desc.field_3 = 1;
        desc.field_2 = 4;
        desc.field_8 = 0x55070005;
        desc.field_4 = 0x400;
        resolve      = func_neo_ark_observatory_8017F44C;
        Gp_MsgPlayerWeapon(0);
        _neoArkObservatoryStageMarker(&desc, resolve);
        D_neo_ark_observatory_80187A30 = desc;
        Task_SpawnFromTable(&D_neo_ark_observatory_80180DD4, 0, 0, 0);
    }
    if (arg2->field_2 == 1 && GameFlag_GetNibble(0xD7) == 0) {
        GameFlag_SetNibble(0xD7, 1);
        if (GameFlag_GetNibble(0x83) != 0) {
            func_800E3FAC(0xA2, 0x2C);
            func_800E8634((s32)&D_8013C72C, 0, (s32)&D_8013CAEC);
        } else {
            func_800E3FAC(0xA2, 0x2D);
            GameFlag_SetNibble(0xD1, 3);
            func_800E8634((s32)&D_80137EE4, 0, (s32)&D_80138694);
        }
    }
    if (arg2->field_2 == 2) {
        if (GameFlag_GetNibble(0xE1) == 0) {
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 6);
            GameFlag_SetNibble(0xE1, 1);
            D_8007272D = 0x15;
            func_800E8634((s32)&D_8013FC58, 0, (s32)&D_80140078);
        }
    }
    if (arg2->field_2 == 3 && gameGetPtrSlot(0xA) != NULL && gGameSession->at4.loc.view == 2) {
        func_80132220();
    }
    if (arg2->field_2 == 4 && GameFlag_GetNibble(0xDE) != 0 && GameFlag_GetNibble(0x16E) == 0) {
        GameFlag_SetNibble(0x16E, 1);
        func_800E8634((s32)&D_neo_ark_observatory_80181200, 0, (s32)&D_neo_ark_observatory_801812C0);
    }
    return 0;
}

/// Aims the observatory's model at slot 0xA when one is resident, otherwise at
/// the slot-3 game pointer, and publishes the y of the vertex offset
/// `func_neo_ark_observatory_8017FE34` adds to the transformed model: 0 while
/// flag 0xD7 is set, 10000 while it is clear or no slot-0xA model is resident.
void func_neo_ark_observatory_8017FA98(void)
{
    Task* task;
    Task* slotA;

    task  = gameGetPtrSlot(0xA);
    slotA = task;
    if (task == NULL) {
        task = gameGetPtrSlot(3);
    }
    if (slotA != NULL && GameFlag_GetNibble(0xD7) != 0) {
        D_neo_ark_observatory_8018136A = 0;
    } else {
        D_neo_ark_observatory_8018136A = 0x2710;
    }
    func_neo_ark_observatory_8017FE34(((TmdObject*)task->extra)->coords, &D_neo_ark_observatory_80181368);
}

void func_neo_ark_observatory_8017FB1C(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x300, 0);
            Gp_SpawnIfCapIdle(task->spawnArg1, 0);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            task->state++;
            break;
        case 2:
            Gp_MsgPlayerWeapon(1);
            Gp_ResetCap();
            taskKill(task);
            break;
    }
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_2", D_neo_ark_observatory_8017D698);
