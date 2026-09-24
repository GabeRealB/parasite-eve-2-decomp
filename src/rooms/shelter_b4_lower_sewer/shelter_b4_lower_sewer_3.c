#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b4_lower_sewer.h"

extern s32 D_8007107C;
extern s8  D_8007217B;
extern s32 D_8011572C;
extern s32 D_80115738;
extern s32 D_8011574C;
extern s32 D_80115750;
extern s32 D_80115758;

extern SVECTOR D_shelter_b4_lower_sewer_80181EA4[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F04[];
extern SVECTOR D_shelter_b4_lower_sewer_80181F14[];

/// Drawing state of the water task: points the primitive cursor
/// `D_shelter_b4_lower_sewer_80183E14` at `D_8005C374` or `D_8005C370`, chosen
/// by `D_8007217B`, plus 0xC000 bytes per `D_8007107C`, then draws both sets
/// of water surfaces.
void func_shelter_b4_lower_sewer_8017E37C(Task* task)
{
    if (D_8007217B == 0) {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b4_lower_sewer_80183E14 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b4_lower_sewer_8017D72C(task);
    func_shelter_b4_lower_sewer_8017DE8C(task);
}

void func_shelter_b4_lower_sewer_8017E400(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115758 = 0x600ED;
        D_8011572C = 0x600EE;
        D_80115750 = 0x600EF;
        if (GameFlag_GetNibble(0xB7) == 1) {
            D_8011574C = 0x6016E;
            D_80115738 = 0x6016F;
        }
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
        case 6: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F14;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            break;
        }
        case 3: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            break;
        }
        case 4: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 5: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 7: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181F04;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[8], 0x200, 0x222);
            break;
        }
        case 8: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[6], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[8], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[22], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
        case 9: {
            SVECTOR* p = D_shelter_b4_lower_sewer_80181EA4;
            func_shelter_b4_lower_sewer_8017E6A0(&p[0], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[2], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[4], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[24], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[26], 0x200, 0x222);
            func_shelter_b4_lower_sewer_8017E6A0(&p[28], 0x200, 0x222);
            break;
        }
    }
}
