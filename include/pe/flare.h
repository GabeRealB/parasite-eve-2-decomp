#ifndef PE_FLARE_H
#define PE_FLARE_H

#include "main/task_types.h"

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

void flareSparkTask(Task* arg0);

void flareEffectTask(Task* arg0);

#endif /* PE_FLARE_H */
