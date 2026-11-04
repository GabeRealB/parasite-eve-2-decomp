/* What every unit includes first, from core, which a package names in its dependencies. A slot of a package is declared, filled by its declaration, and
 * read; the build decides what answers it. It reads a source twice: a first pass, with PE2_FIRST_PASS defined, leaves the live code with
 * every other macro expanded and those below it left as written, for the build to read. */
#ifndef PE2_H
#define PE2_H

/* What core gives a package beside the macros, its one public file being this one: its types and helpers. */
#include "../src/common.h"

#define PE2_STR_(x) #x
#define PE2_STR(x) PE2_STR_(x)

/* PE2_EDITOR_LABEL("Face"): what is declared after it, slots and options alike, is a section of that name in the editor, to the next label
 * or the end of the file, where the label of what included it goes on; "" closes it. */
#define PE2_EDITOR_LABEL(label)

/* #include PE2_INCLUDE(gameplay, enemy.h): a file of what a slot holds. */
#define PE2_INCLUDE(binding, path) PE2_STR(pkg/binding/path)

/* PE2_OPTION_DEF(WEAPON_INDEX, int), PE2_OPTION_DEF(IDLE_TIMEOUT_TICKS, int, 900), PE2_OPTION_DEF(FINISH, enum, matte, gloss):
 * a value set on a package in the editor. An int, bool or string may give its default after its type, which what the
 * package sets overrides; one without has to be set. In no #if. */
#define PE2_OPTION_DEF(...)
#define PE2_OPTION(name) pe2_option_##name
#define PE2_OPTION_IS(name, member) pe2_option_##name##_is_##member

/* PE2_CONFIG(release), PE2_CONFIG_IS(target, psx): the build in use, by name, release or target. */
#define PE2_CONFIG(key) pe2_config_##key
#define PE2_CONFIG_IS(key, value) pe2_config_##key##_is_##value

#ifdef PE2_LANGUAGE_SERVER
#include "pe2_server.h"
#else
#include "pe2_options.h"
#endif

#ifndef PE2_FIRST_PASS

/* PE2_BIND(body, model): a slot, in the code's own word, of the type the code holds. A problem with it fires here. */
#define PE2_BIND(binding, type) pe2_bind_##binding

/* PE2_BIND_ARG(body, _symbol, gBody): how the value at an address is made. */
#define PE2_BIND_ARG(...)

/* PE2_VALUE(body), PE2_VALUE(body, _buffer): what a slot holds as tokens, whole or at a sub-path, a last `_name` being what the target
 * derives from it. */
#define PE2_PICK(a, b, c, d, e, f, which, ...) which
#define PE2_VALUE(...) PE2_PICK(__VA_ARGS__, PE2_VALUE6, PE2_VALUE5, PE2_VALUE4, PE2_VALUE3, PE2_VALUE2, PE2_VALUE1, unused)(__VA_ARGS__)
#define PE2_VALUE1(binding) pe2_value_##binding
#define PE2_VALUE2(binding, a) pe2_value_##binding##_##a
#define PE2_VALUE3(binding, a, b) pe2_value_##binding##_##a##_##b
#define PE2_VALUE4(binding, a, b, c) pe2_value_##binding##_##a##_##b##_##c
#define PE2_VALUE5(binding, a, b, c, d) pe2_value_##binding##_##a##_##b##_##c##_##d
#define PE2_VALUE6(binding, a, b, c, d, e) pe2_value_##binding##_##a##_##b##_##c##_##d##_##e

#ifndef PE2_LANGUAGE_SERVER
#include "pe2_values.h"
#endif

#endif

#endif
