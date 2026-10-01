/* Which of Dryfield's two builds a room package is: the day town (stage 2) or
 * the night town (stage 3). Most Dryfield rooms exist in both, compiled from
 * one source; a package defines DRYFIELD_TIME before including a library that
 * differs between the two.
 */

#ifndef SRC_SHARED_DRYFIELD_TIME_H
#define SRC_SHARED_DRYFIELD_TIME_H

#define DRYFIELD_DAY   1
#define DRYFIELD_NIGHT 2

#ifndef DRYFIELD_TIME
#error "define DRYFIELD_TIME (DRYFIELD_DAY or DRYFIELD_NIGHT) before including a Dryfield room library"
#endif

/// A sound id in the build's own stage bank: 0x52 for the day town, 0x53 for
/// the night town, in the id's top byte.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define DRYFIELD_STAGE_SOUND(id) (0x52000000 | (id))
#else
#define DRYFIELD_STAGE_SOUND(id) (0x53000000 | (id))
#endif

#endif /* SRC_SHARED_DRYFIELD_TIME_H */
