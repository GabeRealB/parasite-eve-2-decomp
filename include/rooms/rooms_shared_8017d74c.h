#ifndef ROOMS_SHARED_8017D74C_H
#define ROOMS_SHARED_8017D74C_H

#include "main/task.h"

/// Runs the accepted room event request, waits for its sounds, then loads the
/// destination recorded in RoomsShared8017d638Msg.
void RoomsShared8017d74c(Task* task);

#endif
