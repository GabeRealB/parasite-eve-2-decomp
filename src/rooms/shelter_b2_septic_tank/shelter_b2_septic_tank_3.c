#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/fs.h"
#include "main/task.h"
#include "rooms/room_common.h"
#include "rooms/shelter_b2_septic_tank.h"

extern s32     D_8007107C;
extern s8      D_8007217B;
extern s32     D_8011572C;
extern s32     D_80115738;
extern s32     D_8011574C;
extern s32     D_80115750;
extern s32     D_80115758;
extern SVECTOR D_shelter_b2_septic_tank_80183314[];
extern SVECTOR D_shelter_b2_septic_tank_80183344[];
extern SVECTOR D_shelter_b2_septic_tank_80183374[];
extern SVECTOR D_shelter_b2_septic_tank_80183514[];
extern SVECTOR D_shelter_b2_septic_tank_80183524[];
extern SVECTOR D_shelter_b2_septic_tank_80183534[];

/// The water task's drawing state: points the primitive cursor
/// `D_shelter_b2_septic_tank_80187054` at the current buffer's 0xC000-byte
/// slice of one of two primitive areas, chosen by `D_8007217B`, then draws both
/// lists of water surfaces.
void func_shelter_b2_septic_tank_8017EAF8(Task* task)
{
    if (D_8007217B == 0) {
        D_shelter_b2_septic_tank_80187054 = (u8*)D_8005C374 + D_8007107C * 0xC000;
    } else {
        D_shelter_b2_septic_tank_80187054 = (u8*)D_8005C370 + D_8007107C * 0xC000;
    }
    func_shelter_b2_septic_tank_8017DB68(task);
    func_shelter_b2_septic_tank_8017E2DC(task);
}

void func_shelter_b2_septic_tank_8017EB7C(Task* arg0)
{
    switch (arg0->state) {
        case 0:
            D_80115758  = 0x601D4;
            D_8011572C  = 0x601F0;
            D_80115750  = 0x6020C;
            D_8011574C  = 0x6016C;
            D_80115738  = 0x6016D;
            arg0->state = 1;
        case 1:
            switch (Gp_GetViewIndex() & 0xFF) {
                case 2: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 3: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[44], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[46], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
                case 4: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183344;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[6], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[16], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[18], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[20], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[34], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[36], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[48], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[50], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[52], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 5: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183374;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, -0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183514, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183524, 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(D_shelter_b2_septic_tank_80183534, 0x200, 0x800, 0x100);
                    break;
                }
                case 6: {
                    SVECTOR* p = D_shelter_b2_septic_tank_80183314;
                    func_shelter_b2_septic_tank_80180054(&p[0], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[2], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[4], 0x200, 0x400, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[14], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[28], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[30], 0x200, 0x800, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[60], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_80180054(&p[62], 0x200, 0, 0x111);
                    func_shelter_b2_septic_tank_8018083C(&p[70], 0x300, 0x10);
                    func_shelter_b2_septic_tank_8018083C(&p[71], 0x300, 0x100);
                    func_shelter_b2_septic_tank_8018083C(&p[72], 0x300, 0x100);
                    break;
                }
            }
            break;
    }
}

/// Alignment pad closing this unit's `.rodata` after the jump table above, so
/// the next unit's tables keep their addresses. Nothing reads it.
const u32 D_shelter_b2_septic_tank_8017D5F8 = 0;
