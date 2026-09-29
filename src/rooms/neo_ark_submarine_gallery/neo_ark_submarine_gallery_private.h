#ifndef SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H
#define SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H

#include "types.h"

#include "gameplay/direction.h"
#include "gameplay/message.h"

#include "main/task_types.h"

extern GpMsgEntry D_neo_ark_submarine_gallery_80181884[5];

extern TaskDesc D_neo_ark_submarine_gallery_801818AC;

// Callbacks referenced by the overlay's shared data tables.
void func_neo_ark_submarine_gallery_8017D678(Task*);

void func_neo_ark_submarine_gallery_8017E2CC(Task*);

void func_neo_ark_submarine_gallery_8017E86C(Task*);

s32 func_neo_ark_submarine_gallery_8017EA04(Task*, s32, GpMessageArg, GpMessageArg);

s32 func_neo_ark_submarine_gallery_8017EA0C(Task*, s32, GpSaveLoc*, GpSaveLoc*);

s32 func_neo_ark_submarine_gallery_8017EABC(Task*, s32, s32, GpMessageArg);

s32 func_neo_ark_submarine_gallery_8017EB48(Task*, s32, GpMessageArg, GpMessageArg);

void func_neo_ark_submarine_gallery_8017EF94(Task*);

#endif // SRC_ROOMS_NEO_ARK_SUBMARINE_GALLERY_NEO_ARK_SUBMARINE_GALLERY_PRIVATE_H
