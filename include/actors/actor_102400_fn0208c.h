#ifndef ACTOR_102400_FN0208C_H
#define ACTOR_102400_FN0208C_H

#include "common.h"

#include "actors/actors_shared_80135b58.h"
#include "gameplay/1BC.h"
#include "main/task.h"

/// 0x40-byte `G_SCRATCH_HEAD` block used by `Actor02400_Fn0208C`: an identity
/// `mat` scaled per axis by `scale`, plus a copy `t` of the coordinate's
/// translation, restored after the multiply.
typedef struct Actor02400ScaleScratch {
    /* 0x00 */ ActorShared80135b58Mat mat;
    /* 0x20 */ VECTOR                 scale;
    /* 0x30 */ VECTOR                 t;
} Actor02400ScaleScratch;
STATIC_ASSERT_SIZEOF(Actor02400ScaleScratch, 0x40);

/// Work block behind the actor's 0x1C slot. `field_100` is the unscaled
/// rotation the model coordinate is rebuilt from each frame and
/// `field_128..field_12C` the per-axis scale; `field_52` and `field_D4` are
/// derived from the Y and Z scale. `ActorsShared80133d94` shrinks the
/// scale toward its floor, releases the `field_130` task (state 4) and counts
/// `field_140` up, re-arming it at a random 30..61 once it passes 360.
typedef struct Actor02400ScaleWork {
    /* 0x000 */ byte   pad_0[0x52];
    /* 0x052 */ s16    field_52;
    /* 0x054 */ byte   pad_54[0x80];
    /* 0x0D4 */ s16    field_D4;
    /* 0x0D6 */ byte   pad_D6[8];
    /* 0x0DE */ u16    field_DE;
    /* 0x0E0 */ byte   pad_E0[0x20];
    /* 0x100 */ MATRIX field_100;
    /* 0x120 */ byte   pad_120[8];
    /* 0x128 */ s16    field_128;
    /* 0x12A */ s16    field_12A;
    /* 0x12C */ s16    field_12C;
    /* 0x12E */ byte   pad_12E[2];
    /* 0x130 */ Task** field_130;
    /* 0x134 */ byte   pad_134[4];
    /* 0x138 */ s16    field_138;
    /* 0x13A */ s16    field_13A;
    /* 0x13C */ s16    field_13C;
    /* 0x13E */ s16    field_13E;
    /* 0x140 */ s16    field_140;
    /* 0x142 */ s16    field_142;
    /* 0x144 */ byte   pad_144[2];
    /* 0x146 */ s16    field_146;
    /* 0x148 */ byte   pad_148[6];
    /* 0x14E */ s16    field_14E;
} Actor02400ScaleWork;

typedef struct Actor02400Scale {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ Actor02400ScaleWork*      field_1C;
    /* 0x20 */ GpEnemy*                  field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ ActorShared80135b58Obj2C* field_2C;
} Actor02400Scale;

void Actor02400_Fn0208C(Actor02400Scale* arg0);
void ActorsShared80133d94(Actor02400Scale* arg0);

#endif
