/* Scripted encounter fragments carried by actor_342400 for the incinerator.
 * Encounter rows select a Mad Chaser, a Slouch or a pair of Sucklercephs.
 * Pair stages borrow the spawned enemies, reveal each with the row's command,
 * and complete the row after the carrier's death/cull check releases both.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_MAD_CHASER_WAVES_H
#define SRC_SHARED_MAD_CHASER_WAVES_H

#include "types.h"

#include "main/task_types.h"

void madChaserWavePairSpawn(Task* arg0);
void madChaserWaveOpen(Task* arg0);

#endif /* SRC_SHARED_MAD_CHASER_WAVES_H */
