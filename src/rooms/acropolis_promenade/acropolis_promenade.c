#include "common.h"

#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/task.h"

#include "rooms/room_common.h"

extern s16 D_8007107A;
extern s8  D_8007272D;

extern s32      D_acropolis_promenade_80180F00;
extern s32      D_acropolis_promenade_80181068;
extern s32      D_acropolis_promenade_80181140;
extern s32      D_acropolis_promenade_80181144;
extern TaskDesc D_acropolis_promenade_80181148;

extern RoomEventMsg D_acropolis_promenade_801862D0;

void func_acropolis_promenade_8017D5E4(void)
{
    u8 temp;
    u8 f0;

    if (D_acropolis_promenade_80181140 == 0) {
        if (Game_Session->field_8 == 4) {
            D_acropolis_promenade_80181140 = 1;
            Task_SpawnFromTable(&D_acropolis_promenade_80181148, 2, 0, 0);
        }
    }
    if (Mc_SaveData.field_5C5 == 6) {
        Mc_SaveData.field_5C5 = 5;
    }
    temp = Game_Session->field_9;
    if (temp == 1) {
        Game_Session->field_69 = 0xA;
        f0                     = Gp_StateF0.field_0;
        if (f0 == temp) {
            D_acropolis_promenade_80181144 = f0;
        }
        if ((D_acropolis_promenade_80181144 == temp) && (f0 != D_acropolis_promenade_80181144)) {
            D_acropolis_promenade_80181144 = 2;
            SndEvt_EnqueueType2(0, 0x3C);
        }
        if ((D_acropolis_promenade_80181144 == 2) && (Game_Session->field_126 != 0)) {
            D_acropolis_promenade_80181144 = 0;
            func_800E8634((s32)&D_acropolis_promenade_80180F00, 0, (s32)&D_acropolis_promenade_80181068);
        }
    }
}

/// Message gate for the promenade's three hotspots: copies the incoming record
/// to the outgoing one, then edits the copy according to the message id and the
/// game's progress nibbles.
///
/// Message 0xA answers with the `field_2` refusal code 1 while the disc has no
/// stream file open (`D_8007107A < 0 || D_8006AC30.sector == 0`) or nibble 1 is
/// not yet at 4; the first pass at 4 advances it to 5 instead of refusing.
/// Message 0xC, while nibble 2 is still 0, refuses with code 3, latches the
/// answered record into `D_acropolis_promenade_801862D0` for the room's own
/// script to pick up, and arms `D_8007272D` with 4. Message 0xE spawns the
/// capsule sequence the first time (nibble 2 still 0) and afterwards reports
/// through `field_3` whether nibble 2 has reached 3.
///
/// `field_5` non-zero means "report only", which suppresses every side effect.
s32 func_acropolis_promenade_8017D70C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    RoomEventMsg unused;
    u16          msgId;

    *out = *in;
    if (in->msgId == 0xA && in->field_5 == 0) {
        if (D_8007107A < 0 || D_8006AC30.sector == 0) {
            out->field_2 = 1;
        }
        if (GameFlag_GetNibble(1) == 4) {
            GameFlag_SetNibble(1, 5);
        } else {
            out->field_2 = 1;
        }
    }
    if (in->msgId == 0xC && GameFlag_GetNibble(2) == 0) {
        if (in->field_5 == 0) {
            out->field_2                   = 3;
            D_acropolis_promenade_801862D0 = *out;
            D_8007272D                     = 4;
        }
        return 1;
    }
    msgId = in->msgId;
    if (msgId == 0xE) {
        if (GameFlag_GetNibble(2) == 0) {
            if (in->field_5 == 0) {
                Gp_SpawnIfCapIdle(2, 1);
                Gp_SetNibbleIf(in->field_6, 2);
            }
            return 0;
        }
        if (in->msgId == msgId) {
            if (in->field_5 == 0) {
                if (GameFlag_GetNibble(2) == 3) {
                    out->field_3 = 2;
                } else {
                    out->field_3 = 1;
                }
            }
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_promenade/acropolis_promenade", D_acropolis_promenade_8017D5D0);
