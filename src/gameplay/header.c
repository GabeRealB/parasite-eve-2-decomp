#include "common.h"
#include "gameplay/gameplay.h"

void Gp_PlayClockState2(Task* arg0);
void Gp_PlayClockState3(Task* arg0);
void Gp_RestartSessionTask(Task* arg0);

/// Printed when the 2D display body a task spawns with cannot be allocated.
const char gGpStrNewDisp2dNull[] = "new_disp_2d ----> NULL\n";
/// Neutral grey (128,128,128) material colour.
///
/// The pre-transformed primitives that carry no colour of their own are shaded
/// with it, and one whose object is dimmed by `lightLevel` decays toward it as
/// the level falls, so it is both the flat material colour and the unlit end
/// of the shading range.
const CVECTOR gGpColorGrey   = { 0x80, 0x80, 0x80, 0 };
const CVECTOR Gp_ColorOrange = { 0xFF, 0xA0, 0x60, 0 };
/// The base colour a lit primitive is computed from when the lighting alone
/// should decide its colour: white, the identity of the GTE's colour multiply.
///
/// A `CVECTOR`, so the whole colour is loaded into the GTE at once. Colour
/// computations start from a copy of it and overwrite the channels they derive.
const CVECTOR gGpColorWhite      = { 0xFF, 0xFF, 0xFF, 0 };
const char    Gp_StrColon[]      = ":";
const char    Gp_StrApostrophe[] = "'";

const TaskFuncTable6 Gp_PlayClockStates = { {
    Gp_InitPlayClock,
    Gp_TickPlayClock,
    Gp_PlayClockState2,
    Gp_PlayClockState3,
    Gp_RestartSessionTask,
    taskKill,
} };

const char Gp_StrBattleResult[] = "Battle Result";
const char Gp_StrTotal[]        = "Total";
const char Gp_StrHP[]           = "HP";
const char Gp_StrMP[]           = "MP";
const char Gp_StrBP[]           = "BP";
const char Gp_StrEXP[]          = "EXP";
const char Gp_StrItem[]         = "Item";
