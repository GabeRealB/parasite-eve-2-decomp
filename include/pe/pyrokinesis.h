#ifndef PE_PYROKINESIS_H
#define PE_PYROKINESIS_H

#include "main/task_types.h"

#include "common.h"

#include "gameplay/actor.h"

#include "main/session_types.h"

/// Collision pair allocated by `func_pyrokinesis_8012EF48` (`memCalloc(0x58)`)
/// and stored in `Task::work`. `obj` is linked on list 1 and carries the
/// packed combo id, `obj2` on list 7 with the 0x4400 flags the cone uses to
/// probe for a wall; both point `field_C` at the one-element `rec` table
/// (terminator `field_0 = 2`).
typedef struct PyroWork {
    /* 0x00 */ GpObj   obj;
    /* 0x20 */ GpObj   obj2;
    /* 0x40 */ GpRec18 rec;
} PyroWork;
STATIC_ASSERT_SIZEOF(PyroWork, 0x58);

/// Pyrokinesis uses the first three attachment rows (indices 1..3).
/// Their dispatch.field_4 values (65, 90, 200) are the flame duration budgets.

void func_pyrokinesis_8012EF48(Task* arg0);

void func_pyrokinesis_80131CE4(Task* arg0);

void func_pyrokinesis_80130C54(Task* arg0);

void func_pyrokinesis_801311B8(Task* arg0);

void func_pyrokinesis_8012FAC8(Task* arg0);

#endif /* PE_PYROKINESIS_H */
