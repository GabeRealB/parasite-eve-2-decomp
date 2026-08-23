#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"

extern s32* Gp_CapCmds;

void Gp_RunCapCmd(s32 arg0, s16 arg1)
{
    register GpCapCmd* rec asm("s2");
    s32                flagId;
    s32                val;
    s32                i;

    for (;;) {
        rec    = (GpCapCmd*)Gp_CapCmds[arg0];
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
                if (rec->field_1 & 4) {
                    if (rec->field_2 < val) {
                        arg0 = rec->field_8;
                        continue;
                    }
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
                Gp_StartCapSlot(arg0, arg1, GameFlag_GetNibble(flagId));
                return;
            case 3:
                Gp_DispatchMsg(Game_GetPtrSlot(7), 0x13F0, arg0, 0);
                return;
            case 4:
                i   = 0;
                val = i;
                if (rec->field_6 != 0) {
                    do {
                        if (Gp_GetCurBit2Flag(rec->field_5 + i) == 0 ||
                            Gp_GetCurBit2Flag(rec->field_5 + i) == 1 ||
                            Gp_GetCurBit2Flag(rec->field_5 + i) == 3) {
                            val++;
                        }
                        i++;
                    } while (i < rec->field_6);
                }
                if (rec->field_1 & 4) {
                    if (val == 0) {
                        arg0 = rec->field_8;
                        continue;
                    }
                }
                Gp_StartCapSlot(arg0, arg1, val);
                return;
        }
        return;
    }
}

static const s32 s_jtbl_pad = 0;
