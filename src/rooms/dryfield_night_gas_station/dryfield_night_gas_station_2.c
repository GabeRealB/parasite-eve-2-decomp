#include "common.h"

#include "gameplay/1A8.h"
#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s8  D_80114C12;
extern u8  D_80115598;
extern u8  D_80115768;
extern s8  D_8007272D;
extern s32 D_dryfield_night_gas_station_80184034;
extern s32 D_dryfield_night_gas_station_80184098;
extern s32 D_dryfield_night_gas_station_801840AC;
extern s32 D_dryfield_night_gas_station_801841FC;
extern s32 D_dryfield_night_gas_station_80188B0C;
extern s32 D_dryfield_night_gas_station_80188B64;
extern s32 D_dryfield_night_gas_station_80188BF4;
extern s32 D_dryfield_night_gas_station_80189014;
extern s32 D_dryfield_night_gas_station_8018920C;
extern s32 D_dryfield_night_gas_station_801892E4;
extern s32 D_dryfield_night_gas_station_80189A7C;

extern TaskDesc D_dryfield_night_gas_station_8018406C[];

extern GpAreaApplyRec D_dryfield_night_gas_station_801907A0;

/// Maps a cap (cutscene) script event key to the stage sound it should play in
/// the night gas station, then enqueues it as a type-6 sound event. Event key
/// 0x83 only plays if a cap script is still reporting an event key. Keys with
/// no sound are ignored. Always returns 0.
s32 func_dryfield_night_gas_station_8017F6B8(s32 arg0, s32 arg1, s32 arg2)
{
    s32 id;

    switch (arg2) {
        case 5:
            id = 0x52010005;
            goto play;
        case 7:
            id = 0x52010007;
            goto play;
        case 0xA:
            id = 0x5201000A;
            goto play;
        case 0xD:
            id = 0x5201000D;
            goto play;
        case 0x11:
            id = 0x52010011;
            goto play;
        case 0x13:
            id = 0x52010013;
            goto play;
        case 0x6D:
        case 0x82:
            id = 0x5201000B;
            goto play;
        case 0x73:
            id = 0x5201000E;
            goto play;
        case 0x83:
            if (Gp_GetCapEventKey() == 0) {
                break;
            }
            id = 0x52010012;
        play:
            Gp_EnqueueStageSnd6(id, 0, 0);
            break;
    }
    return 0;
}

/// Message handler for msg 0x117: walks the `Gp_PendingObj4C` list looking for
/// an object in mode 5 whose `field_48` is 0xFF and which is still pending, and
/// on a hit flips `gGameSession->eventState` / `field_68` and spawns the night gas
/// station cutscene task. Answers 1 only when it found one.
s32 func_dryfield_night_gas_station_8017F7E0(s32 arg0, s32 arg1, s32 arg2)
{
    GpObj4C* node;
    s32      found;

    if (arg2 == 0x117) {
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
            gGameSession->eventState = 1;
            gGameSession->hideHud    = 1;
            Task_SpawnOnDefaultList(D_dryfield_night_gas_station_8018406C, 0, 0, 0);
            return 1;
        }
    }
    return 0;
}

s32 func_dryfield_night_gas_station_8017F89C(s32 arg0, s32 arg1, s32 arg2)
{
    s16 var_a2;

    if (arg2 == 1) {
        Gp_RunCapCmd1(0x11);
    }
    if (arg2 == 5) {
        if (Gp_HasCollectedBit(0x118) == 0) {
            Gp_HasCollectedBit(0x117);
            var_a2 = 0;
        } else {
            var_a2 = 1;
        }
        Gp_StartCapSlot(0x12, 1, var_a2);
    }
    if ((arg2 == 0x17) && (gGameSession->at4.loc.room == 4)) {
        if (Gp_HasCollectedBit(0x11E) != 0) {
            if (GameFlag_GetNibble(0xD4) == 0) {
                GameFlag_SetNibble(0xD4, 1);
            } else {
                GameFlag_SetNibble(0xD4, 2);
            }
        }
        Gp_SpawnIfCapIdle(GameFlag_GetNibble(0xD4) != 0 ? (GameFlag_GetNibble(0xD4) == 1 ? 0x20 : 0x1F) : arg2, 0);
    }
    return 0;
}

/// Handler for slot-7 msg `0x13EF` in `D_dryfield_night_gas_station_80184034`:
/// the directed action selected by `field_2` 0xE runs the room's cutscene script
/// blob at `D_dryfield_night_gas_station_8018920C`, but only once nibble 0x63 has
/// reached 2 and pointer slot 0xA is live.
s32 func_dryfield_night_gas_station_8017F990(Task* task, s32 msgId, GpMsg13EF* msg)
{
    if ((msg->field_2 == 0xE) && (Game_GetPtrSlot(0xA) != NULL) && (GameFlag_GetNibble(0x63) >= 2)) {
        func_800E8614((s32)&D_dryfield_night_gas_station_8018920C, 0);
    }
    return 0;
}

/// Arms the room's night sequence, once: while nibble 0x63 is still clear it
/// sets that nibble, plays the script blob at
/// `D_dryfield_night_gas_station_80188B64`, raises `gGameSession->flowFlags`
/// bit 0x80, applies the room's area records, clears nibbles 0x62 and 0x45 and
/// queues sound event 0x64.
s32 func_dryfield_night_gas_station_8017F9E8(void)
{
    if (GameFlag_GetNibble(0x63) == 0) {
        GameFlag_SetNibble(0x63, 1);
        func_800E8614((s32)&D_dryfield_night_gas_station_80188B64, 1);
        gGameSession->flowFlags |= 0x80;
        Gp_ApplyAreaRecs(&D_dryfield_night_gas_station_801907A0);
        GameFlag_SetNibble(0x62, 0);
        GameFlag_SetNibble(0x45, 0);
        SndEvt_EnqueueType2(0, 0x64);
    }
    return 0;
}

/// Tears the room's scripted sequence down: raises `gGameSession->hideHud`
/// and `D_80115768`, hides the display, clears collection bit 0x117, installs
/// the room's two cap files, runs the 0xA2/0x16 event and kills its own task.
void func_dryfield_night_gas_station_8017FA6C(Task* arg0)
{
    gGameSession->hideHud = 1;
    D_80115768            = 1;
    SetDispMask(0);
    Gp_ClearCollectedBit(0x117);
    func_800E8634((s32)&D_dryfield_night_gas_station_801840AC, 0, (s32)&D_dryfield_night_gas_station_801841FC);
    func_800E3FAC(0xA2, 0x16);
    D_8007272D = 4;
    taskKill(arg0);
}

/// Runs the room's one-shot post-sequence event: with the session still on its
/// first mode and nibble 0x63 reading 1 — and the cutscene flag agreeing — it
/// advances the nibble to 2 and plays the cap pair
/// `D_dryfield_night_gas_station_80188BF4` / `_80189014`.
void func_dryfield_night_gas_station_8017FAEC(void)
{
    s32 temp_v0;

    if (gGameSession->eventState == 0) {
        temp_v0 = GameFlag_GetNibble(0x63);
        if ((temp_v0 == 1) && (D_80114C12 != temp_v0)) {
            GameFlag_SetNibble(0x63, 2);
            func_800E8634((s32)&D_dryfield_night_gas_station_80188BF4, 0, (s32)&D_dryfield_night_gas_station_80189014);
        }
    }
}
