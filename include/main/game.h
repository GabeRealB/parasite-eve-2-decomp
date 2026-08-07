#ifndef GAME_H
#define GAME_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libspu.h>

/// Size of the game heap.
#define G_HEAP_SIZE 0xFF80

// Scratchpad temporary arena (grows downward from head).
#define G_SCRATCH_HEAD         PSX_SCRATCH_ADDR(0x3FC)
#define GameResetScratchHead() *(void**)G_SCRATCH_HEAD = G_SCRATCH_HEAD

// Module headers (types + public APIs). Including game.h pulls in the full
// type set for TUs that historically only included this file.
#include "main/task.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/mc.h"
#include "main/boot.h"
#include "main/gamemain.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/cdaudio.h"
#include "main/cdstream.h"
#include "main/stream.h"
#include "main/ui.h"
#include "main/text.h"
#include "main/tmd.h"
#include "main/fs.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/wipsys.h"
#include "main/gameflow.h"
#include "main/gameflag.h"

#endif // GAME_H
