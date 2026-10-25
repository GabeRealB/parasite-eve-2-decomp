#ifndef INCLUDE_ACTORS_GENERATOR_H
#define INCLUDE_ACTORS_GENERATOR_H

/// Descriptor indices shared by the Beta and Proto Generator spawn tables.
///
/// Both tables contain exactly two recipes and have no terminator.
enum {
    GENERATOR_BODY_TASK_INDEX         = 0,
    GENERATOR_LIFE_SUPPORT_TASK_INDEX = 1,
    GENERATOR_TASK_DESC_COUNT         = 2
};

#endif // INCLUDE_ACTORS_GENERATOR_H
