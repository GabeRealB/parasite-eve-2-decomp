#include "common.h"

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "rooms/acropolis_cafeteria.h"

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", D_acropolis_cafeteria_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", RoomsShared8017d878Table);

static const char CafeteriaPlayerLabel[12] = "Player";

void func_acropolis_cafeteria_8017D6AC(void)
{
    if (Display_State.field_112 != 0) {
        func_80724608(Game_GetPtrSlot(3), -0x8C, -0x32, (void*)CafeteriaPlayerLabel);
        func_807245E4(Game_GetPtrSlot(3));
    }
}

/// Copies the room message, selects its response, and starts capture slots 5
/// or 6 when the room's progress permits. field_5 suppresses side effects.
s32 func_acropolis_cafeteria_8017D700(s32 arg0, s32 arg1, RoomEventMsg* in, RoomEventMsg* out)
{
    s32 msgId;
    s32 responseId;

    *out = *in;
    if (in->msgId == 7 && in->field_2 == 4) {
        if (GameFlag_GetNibble(0) >= 3) {
            return 1;
        }
        if (in->field_5 == 0) {
            Gp_SetNibbleIf(in->field_6, 2);
            Gp_StartCapSlot(5, 1, 0);
        }
        return 0;
    }
    msgId = in->msgId;
    if (msgId == 3) {
        if (GameFlag_GetNibble(0) < 2) {
            if (D_acropolis_cafeteria_80184164 == 0) {
                if (in->msgId == msgId && in->field_5 == 0) {
                    if (GameFlag_GetNibble(0) < 2) {
                        // Keep these stores separate from the later response branches.
                        if (GameFlag_GetNibble(0x21) < 2) {
                            s8 response = 1;
                            SOFT_BARRIER();
                            out->field_3 = response;
                        } else {
                            s8 response = 2;
                            SOFT_BARRIER();
                            out->field_3 = response;
                        }
                    } else {
                        out->field_3 = msgId;
                    }
                }
                return 1;
            }
            if (D_acropolis_cafeteria_80184164 == 2) {
                if (in->field_5 == 0) {
                    Gp_StartCapSlot(6, 1, 0);
                }
            }
            return 0;
        }
        if (GameFlag_GetNibble(0xE) == msgId && in->field_5 == 0) {
            GameFlag_SetNibble(0xE, 2);
        }
        responseId = in->msgId;
        if (responseId == 3 && in->field_5 == 0) {
            if (GameFlag_GetNibble(0) < 2) {
                if (GameFlag_GetNibble(0x21) < 2) {
                    out->field_3 = 1;
                } else {
                    out->field_3 = 2;
                }
            } else {
                out->field_3 = responseId;
            }
        }
    }
    return 1;
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_cafeteria/acropolis_cafeteria", func_acropolis_cafeteria_8017D8F8);
