#include "common.h"
#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"

extern u8  D_8007216D;
extern s16 D_neo_ark_shrine_80186868;
extern s16 D_neo_ark_shrine_8018686A;
extern s16 D_neo_ark_shrine_8018686C[16];

INCLUDE_ASM("rooms/nonmatchings/neo_ark_shrine/neo_ark_shrine_2", func_neo_ark_shrine_8017DF7C);

s16 func_neo_ark_shrine_8017E254(void)
{
    s32 flag;

    if (D_neo_ark_shrine_8018686C[0] == 9 && D_neo_ark_shrine_8018686C[1] == 10 &&
        D_neo_ark_shrine_8018686C[2] == 11 && D_neo_ark_shrine_8018686C[3] == 12 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_8018686C[0] == 12 && D_neo_ark_shrine_8018686C[1] == 11 &&
        D_neo_ark_shrine_8018686C[2] == 10 && D_neo_ark_shrine_8018686C[3] == 9 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 3;
    }
    if (D_neo_ark_shrine_80186868 == 1) {
        return 4;
    }
    if (D_neo_ark_shrine_8018686C[3] == 1 && D_neo_ark_shrine_8018686C[6] == 2 &&
        D_neo_ark_shrine_8018686C[9] == 3 && D_neo_ark_shrine_8018686C[12] == 4 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    if (D_neo_ark_shrine_8018686C[3] == 4 && D_neo_ark_shrine_8018686C[6] == 3 &&
        D_neo_ark_shrine_8018686C[9] == 2 && D_neo_ark_shrine_8018686C[12] == 1 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 1;
    }
    flag = D_neo_ark_shrine_8018686A;
    if (flag == 1) {
        D_neo_ark_shrine_8018686A = 0;
        if (GameFlag_GetNibble(0xE9) == 0) {
            D_8007216D                 = flag;
            gGameSession->at4.loc.room = flag;
        } else {
            D_8007216D                 = 4;
            gGameSession->at4.loc.room = 4;
        }
        gGameSession->roomObjsDirty = 1;
        SndEvt_EnqueueType6(0x5515000A, 0, 0);
        Gp_SpawnPadLerp(0x28, 0x30, 0x60);
    }
    if (D_neo_ark_shrine_8018686C[0] == 5 && D_neo_ark_shrine_8018686C[4] == 6 &&
        D_neo_ark_shrine_8018686C[8] == 7 && D_neo_ark_shrine_8018686C[12] == 8 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    if (D_neo_ark_shrine_8018686C[0] == 8 && D_neo_ark_shrine_8018686C[4] == 7 &&
        D_neo_ark_shrine_8018686C[8] == 6 && D_neo_ark_shrine_8018686C[12] == 5 &&
        D_neo_ark_shrine_8018686C[15] == 0) {
        return 2;
    }
    return 0;
}
