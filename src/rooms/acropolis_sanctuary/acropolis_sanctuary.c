#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3A34.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"

#include "rooms/room_common.h"

extern s8             D_8007272D;
extern s32            D_acropolis_sanctuary_80180B0C;
extern s32            D_acropolis_sanctuary_80181664;
extern GpObj4A        D_acropolis_sanctuary_80183CAC[];
extern GpAreaApplyRec D_acropolis_sanctuary_80186418[];

/// Room entry fixup. Once the session reaches phase 3 (`GameFlag_GetNibble(2)`
/// still 0), advances that flag and applies the room's one-shot state, then
/// clears the `GpObj4A` bit 0x40 render filter on the objects that stay hidden
/// while `Gp_GetCurBit2Flag(0x1C)` is 2. `mask` is a local because the target
/// CSEs `~0x40` into a register and uses `and` rather than nine `andi`s.
void func_acropolis_sanctuary_8017D5E0(void)
{
    s32      mask;
    GpObj4A* p0;
    GpObj4A* p3;
    GpObj4A* p4;
    GpObj4A* p5;
    GpObj4A* p6;
    GpObj4A* p7;
    GpObj4A* p8;
    GpObj4A* p9;
    GpObj4A* p10;

    if (GameFlag_GetNibble(2) == 0 && Game_Session->field_8 == 3) {
        GameFlag_SetNibble(2, 2);
        func_800E8634((s32)&D_acropolis_sanctuary_80180B0C, 0, (s32)&D_acropolis_sanctuary_80181664);
        Gp_ApplyAreaRecs(D_acropolis_sanctuary_80186418);
        D_8007272D = 6;
        GameFlag_SetNibble(1, 5);
        GameFlag_SetNibble(0x25, 1);
        func_800E3FAC(0xA2, 6);
        GameFlag_SetNibble(3, 0);
        GameFlag_SetNibble(0x155, 5);
    }
    if (Gp_GetCurBit2Flag(0x1C) == 2) {
        mask = ~0x40;
        p0   = &D_acropolis_sanctuary_80183CAC[0];
        p3   = &D_acropolis_sanctuary_80183CAC[3];
        p4   = &D_acropolis_sanctuary_80183CAC[4];
        p5   = &D_acropolis_sanctuary_80183CAC[5];
        p6   = &D_acropolis_sanctuary_80183CAC[6];
        p7   = &D_acropolis_sanctuary_80183CAC[7];
        p8   = &D_acropolis_sanctuary_80183CAC[8];
        p9   = &D_acropolis_sanctuary_80183CAC[9];
        p10  = &D_acropolis_sanctuary_80183CAC[10];

        p0->field_4A  &= mask;
        p3->field_4A  &= mask;
        p4->field_4A  &= mask;
        p5->field_4A  &= mask;
        p6->field_4A  &= mask;
        p7->field_4A  &= mask;
        p8->field_4A  &= mask;
        p9->field_4A  &= mask;
        p10->field_4A &= mask;
    }
}

/// Message gate for the sanctuary's second hotspot: copies the incoming record
/// to the outgoing one, then answers message 0xB. The first time the message is
/// seen for real (`field_5` == 0) it latches nibble 7 to 2 and raises the room's
/// 0x13 bit-2 flag; the answer written back into `field_3` is 1 while nibble 2
/// is still clear and 2 once it is set.
s32 func_acropolis_sanctuary_8017D73C(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 nib;

    *out = *in;
    if (in->msgId == 0xB) {
        if (in->field_5 == 0) {
            if (GameFlag_GetNibble(7) == 0) {
                GameFlag_SetNibble(7, 2);
                Gp_SetCurBit2Flag(0x13, 2);
            }
        }
        if (in->msgId == 0xB && in->field_5 == 0) {
            nib = GameFlag_GetNibble(2);
            if (nib == 0) {
                nib = 1;
            } else {
                nib = 2;
            }
            out->field_3 = nib;
        }
    }
    return 1;
}

INCLUDE_RODATA("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary", D_acropolis_sanctuary_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary", D_acropolis_sanctuary_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary", D_acropolis_sanctuary_8017D5D0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary", D_acropolis_sanctuary_8017D5D8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_sanctuary/acropolis_sanctuary", D_acropolis_sanctuary_8017D5DC);
