#ifndef SRC_SHARED_DRYFIELD_TIME_H
#define SRC_SHARED_DRYFIELD_TIME_H

/// Selects the daytime Dryfield instance of included room code.
///
/// Factory, G & R kitchen, motel room 6 and trailer coach carriers define
/// `DRYFIELD_TIME` as this before including their shared header and retain that
/// binding for its implementation fragments. The value 1 is a preprocessor
/// discriminator; daytime Dryfield's runtime stage number is 2.
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
