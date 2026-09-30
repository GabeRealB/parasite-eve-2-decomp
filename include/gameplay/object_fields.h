#ifndef GAMEPLAY_OBJECT_FIELDS_H
#define GAMEPLAY_OBJECT_FIELDS_H

#include "types.h"

#include "gameplay/enemy.h"

struct Enemy;

s32 Gp_LookupIdField(s32 arg0, s32 arg1);

s32 Gp_GetIdParam0(s32 arg0);

s32 Gp_GetIdParam1(s32 arg0);

void Gp_SetObjFlag4(struct Enemy* arg0, s32 arg1, s32 arg2);

s32 Gp_TickObjFlag4(struct Enemy* arg0);

s32 Gp_ObjFlag4Expired(struct Enemy* arg0);

void Gp_SetObjFlag1(struct Enemy* arg0);

void Gp_SetObjFlag2(struct Enemy* arg0, s32 arg1, s32 arg2);

s32 Gp_TickObjFlag2(struct Enemy* arg0);

s32 Gp_GetIdParam2(s32 arg0);

#endif // GAMEPLAY_OBJECT_FIELDS_H
