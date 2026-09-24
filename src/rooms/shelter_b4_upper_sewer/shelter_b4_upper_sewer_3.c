#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b4_upper_sewer.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_80115738;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern SVECTOR D_shelter_b4_upper_sewer_80186490[];
extern SVECTOR D_shelter_b4_upper_sewer_801864B0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864D0[];
extern SVECTOR D_shelter_b4_upper_sewer_801864F0[];
extern SVECTOR D_shelter_b4_upper_sewer_80186520[];

/// Sets or clears `field_4` of the second sprite command in view 13 of the
/// current room's sprite table, from the low byte of `arg0` (zero clears it,
/// anything else sets it to 1).
void func_shelter_b4_upper_sewer_8017E59C(s32 arg0)
{
    GpAreaKey* sess = &gGameSession->at4.loc;
    GpSprtCmd* cmd;

    cmd = Gp_SprtTables[sess->stage - 1][0].field_0[sess->area - 1][12].field_4;
    if ((arg0 & 0xFF) == 0) {
        cmd[1].field_4 = 0;
    } else {
        cmd[1].field_4 = 1;
    }
}

/// Publishes the sewer's effect ids on the task's first tick - two extra ids
/// only while `GameFlag_GetNibble(0xB7)` is 1 - then draws the
/// `func_shelter_b4_upper_sewer_8017F8CC` capsules the current camera view
/// shows. Views 4 and 13 both end on
/// `D_shelter_b4_upper_sewer_801864F0[12]`, and writing that address off the
/// array (rather than through its own symbol) is what keeps view 4's array
/// base live across the first call while the shared tail is cross-jumped.
void func_shelter_b4_upper_sewer_8017E5F8(Task* arg0)
{
    if (arg0->state == 0) {
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x60170;
            D_80115738 = 0x60171;
        }
        D_80115728  = 0x6024E;
        D_80115744  = 0x6025A;
        D_8011573C  = 0x60265;
        D_80115720  = 0x60271;
        D_80115758  = 0x600F0;
        D_8011572C  = 0x600F1;
        D_80115750  = 0x600F2;
        D_80115734  = 0x60224;
        D_80115730  = 0x6022F;
        D_80115754  = 0x6023A;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 3:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_801864F0, 0x200, 0x444);
            break;
        case 4:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[0], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
        case 8: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864D0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[4], 0x200, 0x222);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[16], 0x200, 0x124);
            break;
        }
        case 10: {
            SVECTOR* p = D_shelter_b4_upper_sewer_801864B0;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[2], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x343);
            break;
        }
        case 11: {
            SVECTOR* p = D_shelter_b4_upper_sewer_80186490;
            func_shelter_b4_upper_sewer_8017F8CC(&p[0], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[6], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[18], 0x200, 0x444);
            func_shelter_b4_upper_sewer_8017F8CC(&p[20], 0x200, 0x343);
            break;
        }
        case 12:
            func_shelter_b4_upper_sewer_8017F8CC(D_shelter_b4_upper_sewer_80186520, 0x200, 0x444);
            break;
        case 13:
            func_shelter_b4_upper_sewer_8017F8CC(&D_shelter_b4_upper_sewer_801864F0[12], 0x200, 0x124);
            break;
    }
}

/// Pad word that closes this unit's `.rodata` after the 11-entry jump table
/// above, placing the next unit's rodata where the original has it. Nothing
/// reads it.
const u32 D_shelter_b4_upper_sewer_8017D630 = 0;
