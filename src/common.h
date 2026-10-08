/* SD2Cloud -- declarations shared by the app's source files */
#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <tamtypes.h>

#define APP_NAME    "SD2Cloud"
#define APP_VERSION "1.5.1"
#ifndef APP_COMMIT
#define APP_COMMIT ""       /* the commit it was built from (7 characters), when the build is told: see the Makefile */
#endif

/* each module's own header: this one is for a file that needs most of them */
#include "system.h"
#include "files.h"
#include "json.h"
#include "i18n.h"
#include "config.h"
#include "state.h"
#include "cards.h"
#include "mcfs.h"
#include "stream.h"
#include "google.h"
#include "update.h"
#include "restore.h"
#include "helper.h"

#endif
