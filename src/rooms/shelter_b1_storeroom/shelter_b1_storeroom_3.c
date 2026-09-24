#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"
#include "rooms/room_common.h"

extern s32 D_80115720;
extern s32 D_80115728;
extern s32 D_8011572C;
extern s32 D_80115730;
extern s32 D_80115734;
extern s32 D_8011573C;
extern s32 D_80115744;
extern s32 D_80115750;
extern s32 D_80115754;
extern s32 D_80115758;

extern SVECTOR D_shelter_b1_storeroom_80184998[];
extern SVECTOR D_shelter_b1_storeroom_80184A18[];
extern SVECTOR D_shelter_b1_storeroom_80184A38[];
extern SVECTOR D_shelter_b1_storeroom_80184A98[];
extern SVECTOR D_shelter_b1_storeroom_80184AB8[];

void func_shelter_b1_storeroom_8017D7EC(Task* arg0)
{
    if (arg0->state == 0) {
        D_80115728  = 0x60246;
        D_80115744  = 0x60252;
        D_8011573C  = 0x6025D;
        D_80115720  = 0x60269;
        D_80115758  = 0x601CA;
        D_8011572C  = 0x601E6;
        D_80115750  = 0x60202;
        D_80115734  = 0x6021E;
        D_80115730  = 0x60229;
        D_80115754  = 0x60234;
        arg0->state = 1;
    }

    switch (Gp_GetViewIndex() & 0xFF) {
        case 2:
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[0], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[2], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[8], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[10], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[16], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A38[18], 0x100, 0x444);
            Room_Draw13(&D_shelter_b1_storeroom_80184A38[26], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184A38[27], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184A38[28], 0x100, 0x222);
            break;
        case 3:
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[0], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[2], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[4], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[6], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[8], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[10], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[12], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[14], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[20], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[22], 0x100, 0x222);
            break;
        case 4:
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[0], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[8], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[16], 0x100, 0x333);
            Room_Draw01(&D_shelter_b1_storeroom_80184A18[18], 0x100, 0x333);
            Room_Draw13(&D_shelter_b1_storeroom_80184A18[27], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184A18[28], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184A18[29], 0x100, 0x222);
            break;
        case 5:
            Room_Draw01(&D_shelter_b1_storeroom_80184998[0], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[2], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[4], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[6], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[8], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[10], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[12], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[32], 0x100, 0x222);
            Room_Draw01(&D_shelter_b1_storeroom_80184998[34], 0x100, 0x222);
            break;
        case 6:
            Room_Draw01(&D_shelter_b1_storeroom_80184AB8[0], 0x100, 0x444);
            Room_Draw01(&D_shelter_b1_storeroom_80184AB8[2], 0x100, 0x444);
            Room_Draw13(&D_shelter_b1_storeroom_80184AB8[4], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184AB8[5], 0x100, 0x222);
            Room_Draw13(&D_shelter_b1_storeroom_80184AB8[6], 0x100, 0x222);
            break;
        case 7:
            Room_Draw01(&D_shelter_b1_storeroom_80184A98[0], 0x100, 0x333);
            Room_Draw01(&D_shelter_b1_storeroom_80184A98[2], 0x100, 0x333);
            break;
    }
}
