#include "common.h"

#include <psyq/abs.h>

#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/sound.h"
#include "rooms/neo_ark_shrine.h"

extern u8               D_8007216D;
extern s16              D_neo_ark_shrine_80186868;
extern s16              D_neo_ark_shrine_8018686A;
extern s16              D_neo_ark_shrine_8018686C[16];
extern NeoArkShrineSlot D_neo_ark_shrine_8018252C[16];
extern NeoArkShrineSlot D_neo_ark_shrine_801825AC[16];
extern NeoArkShrineSlot D_neo_ark_shrine_8018688C[16];
extern NeoArkShrineSlot D_neo_ark_shrine_801868CC[16];

/// Animates and draws the shrine's sliding-tile puzzle. Each tile's target
/// position is taken from the board position it now occupies; its drawn
/// position eases halfway there every frame and snaps once both axes are
/// within four units. Every tile but tile 0, the gap, is then drawn as a 32x32
/// textured quad.
void func_neo_ark_shrine_8017DF7C(void)
{
    s32               i;
    s32               tile;
    NeoArkShrineSlot* cur;
    NeoArkShrineSlot* tgt;
    POLY_FT4*         prim;

    for (i = 0; i < 16; i++) {
        tile                              = D_neo_ark_shrine_8018686C[i];
        D_neo_ark_shrine_801868CC[tile].x = D_neo_ark_shrine_8018252C[i].x;
        D_neo_ark_shrine_801868CC[tile].y = D_neo_ark_shrine_8018252C[i].y;
    }

    for (i = 0; i < 16; i++) {
        tile    = D_neo_ark_shrine_8018686C[i];
        cur     = &D_neo_ark_shrine_8018688C[tile];
        tgt     = &D_neo_ark_shrine_801868CC[tile];
        cur->x += ((s16)tgt->x - (s16)cur->x) >> 1;
        cur->y += ((s16)tgt->y - (s16)cur->y) >> 1;
        if (ABS((s16)cur->x - (s16)tgt->x) < 4 &&
            ABS((s16)D_neo_ark_shrine_8018688C[tile].y - (s16)D_neo_ark_shrine_801868CC[tile].y) < 4) {
            D_neo_ark_shrine_8018688C[tile].x = D_neo_ark_shrine_801868CC[tile].x;
            D_neo_ark_shrine_8018688C[tile].y = D_neo_ark_shrine_801868CC[tile].y;
        }
        if (tile != 0) {
            prim           = (POLY_FT4*)gGpuPrimCursor;
            gGpuPrimCursor = (u8*)(prim + 1);
            setPolyFT4(prim);
            setUVWH(prim, D_neo_ark_shrine_801825AC[tile].x, D_neo_ark_shrine_801825AC[tile].y, 0x20, 0x20);
            prim->tpage = 0x8D;
            prim->clut  = 0x3FC0;
            setShadeTex(prim, 1);
            setXYWH(prim, D_neo_ark_shrine_8018688C[tile].x, D_neo_ark_shrine_8018688C[tile].y, 0x20, 0x20);
            addPrim(&gGpuCurrentOt[10], prim);
        }
    }
}

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
