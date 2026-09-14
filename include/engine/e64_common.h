#ifndef ENGINE64_COMMON_H
#define ENGINE64_COMMON_H

/* How many entries a declared array holds. Only works on the array itself, not
   on a pointer to it: a table and the count handed over with it are written
   together, and this is what keeps the two from drifting apart. */
#define E64_ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

#endif
