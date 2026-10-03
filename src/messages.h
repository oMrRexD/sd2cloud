/* SD2Cloud -- ids of the texts on screen (the list lives in messages.def) */
#ifndef MESSAGES_H
#define MESSAGES_H

enum {
    T_NONE = 0,
#define X(id, en, pt) id,
#include "messages.def"
#undef X
    T_COUNT
};

#endif
