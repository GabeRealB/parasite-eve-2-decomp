#ifndef ACTORS_SHARED_80135C4C_H
#define ACTORS_SHARED_80135C4C_H

#include "common.h"

#include "gameplay/1BC.h"

/// Collision object and its single record, allocated by the shared setup body.
typedef struct ActorsShared80135c4cObjWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpRec18 rec;
    /* 0x38 */ byte    pad_38[2];
    /* 0x3A */ s16     field_3A;
    /* 0x3C */ u16     field_3C;
    /* 0x3E */ byte    pad_3E[2];
} ActorsShared80135c4cObjWork;
STATIC_ASSERT_SIZEOF(ActorsShared80135c4cObjWork, 0x40);

/// Sparse view of the parent work block. The setup copies this halfword into
/// the new collision object's trailing metadata.
typedef struct ActorsShared80135c4cParentWork {
    /* 0x000 */ byte pad_0[0x3AC];
    /* 0x3AC */ u16  field_3AC;
} ActorsShared80135c4cParentWork;

extern GpU16Pair ActorsShared80135c4cPair;

void ActorsShared80135c4c(GpEnemy* enemy, Task* task);

#endif
