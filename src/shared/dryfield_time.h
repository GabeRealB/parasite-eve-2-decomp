#ifndef SRC_SHARED_DRYFIELD_TIME_H
#define SRC_SHARED_DRYFIELD_TIME_H

/// Selects the daytime Dryfield instance of included room code.
///
/// Factory, G & R kitchen, motel room 6 and trailer coach carriers define
/// `DRYFIELD_TIME` as this before including their shared header and retain that
/// binding for its implementation fragments. The value 1 is a preprocessor
/// discriminator; daytime Dryfield's runtime stage number is 2.
#define DRYFIELD_DAY 1

/// Selects the nighttime Dryfield instance of included room code.
///
/// Factory, G & R kitchen, motel room 6 and trailer coach carriers bind
/// `DRYFIELD_TIME` to this before including their shared header and keep that
/// binding through the implementation fragments. The value 2 is a preprocessor
/// discriminator, independent of the runtime stage number.
#define DRYFIELD_NIGHT 2

/// Requires a fixed day/night binding for each included Dryfield room instance.
///
/// Each carrier defines `DRYFIELD_TIME` as `DRYFIELD_DAY` (1) or
/// `DRYFIELD_NIGHT` (2) before its shared room header and keeps the binding
/// unchanged through all implementation fragments. It is a preprocessor
/// discriminator with no runtime storage or time units. A carrier can use the
/// named value before this header defines it; expansion occurs at each use.
#ifndef DRYFIELD_TIME
#error "define DRYFIELD_TIME (DRYFIELD_DAY or DRYFIELD_NIGHT) before including a Dryfield room library"
#elif DRYFIELD_TIME != DRYFIELD_DAY && DRYFIELD_TIME != DRYFIELD_NIGHT
#error "DRYFIELD_TIME must be DRYFIELD_DAY or DRYFIELD_NIGHT"
#endif

/// A sound id in the build's own stage bank: 0x52 for the day town, 0x53 for
/// the night town, in the id's top byte.
#if DRYFIELD_TIME == DRYFIELD_DAY
#define DRYFIELD_STAGE_SOUND(id) (0x52000000 | (id))
#else
#define DRYFIELD_STAGE_SOUND(id) (0x53000000 | (id))
#endif

#endif /* SRC_SHARED_DRYFIELD_TIME_H */
