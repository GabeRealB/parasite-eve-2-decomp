/* The scripted enemy waves of the Shelter B3 dumping hole. A controller walks
 * a 17-slot encounter table and keeps up to three slots live. Each slot spawns
 * a spawner task for one enemy of either kind or for a hopper pair, numbering
 * each enemy through a shared counter. After 60 frames a spawner reveals its
 * enemy with a command message (0x2A00, 0x2C00 or 0x2E00) carrying the slot's
 * spawn argument, then watches it until it dies or leaves the cull zone and
 * marks the slot done.
 *
 * Include this header in the prologue and each fragment at its function's
 * position.
 */

#ifndef SRC_SHARED_HOPPER_WAVES_H
#define SRC_SHARED_HOPPER_WAVES_H

#include "types.h"

#include "main/task_types.h"

void hopperWavePairSpawn(Task* arg0);
void hopperWaveOpen(Task* arg0);
void hopperWaveRevealSecond(Task* arg0);
void hopperWavePairRevealFirst(Task* arg0);
void hopperWavePairRevealSecond(Task* arg0);
void hopperWavePairWatch(Task* arg0);
void hopperWavePairDropDead(Task* arg0);

/* Defined by each package. */
void hopperWaveSpawnSlot(s16 arg0, s16 arg1, s16 arg2);
void hopperWavePairCull(Task* arg0);

#endif /* SRC_SHARED_HOPPER_WAVES_H */
