#include "common.h"

#include <psyq/libgte.h>

#include "gameplay/D4.h"
#include "main/task.h"

extern SVECTOR D_neo_ark_observatory_80181434[];
extern SVECTOR D_neo_ark_observatory_801814E4[];
extern SVECTOR D_neo_ark_observatory_801814F4[];
extern SVECTOR D_neo_ark_observatory_801814FC[];
extern SVECTOR D_neo_ark_observatory_8018150C[];
extern SVECTOR D_neo_ark_observatory_8018151C[];
extern SVECTOR D_neo_ark_observatory_80181524[];
extern SVECTOR D_neo_ark_observatory_80181564[];
extern SVECTOR D_neo_ark_observatory_80181574[];
extern SVECTOR D_neo_ark_observatory_8018157C[];
extern s16     D_neo_ark_observatory_80187A3C;

void Room_Draw13(SVECTOR* v, s32 arg1, s32 arg2);
void func_neo_ark_observatory_80180534(SVECTOR* v, s32 arg1, s16 arg2, s32 arg3);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FBE8);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FCA0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FCE0);

INCLUDE_ASM("rooms/nonmatchings/neo_ark_observatory/neo_ark_observatory_3", func_neo_ark_observatory_8017FD7C);
