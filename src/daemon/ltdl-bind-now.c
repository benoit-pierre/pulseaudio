/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif

#ifdef HAVE_SYS_DL_H
#include <sys/dl.h>
#endif

#include <string.h>

#include <ltdl.h>

#include <pulse/i18n.h>

#include <pulsecore/macro.h>
#include <pulsecore/mutex.h>
#include <pulsecore/thread.h>
#include <pulsecore/log.h>

#include "ltdl-bind-now.h"

#ifdef RTLD_NOW
#define PA_BIND_NOW RTLD_NOW
#elif defined(DL_NOW)
#define PA_BIND_NOW DL_NOW
#else
#undef PA_BIND_NOW
#endif

#ifdef PA_BIND_NOW

/*
  To avoid lazy relocations during runtime in our RT threads we add
  our own shared object loader with uses RTLD_NOW if it is
  available. The standard ltdl loader prefers RTLD_LAZY.

  Please note that this loader doesn't have any influence on
  relocations on any libraries that are already loaded into our
  process, i.e. because the pulseaudio binary links directly to
  them. To disable lazy relocations for those libraries it is possible
  to set $LT_BIND_NOW before starting the pulsaudio binary.
*/

static lt_module bind_now_open(lt_user_data d, const char *fname, lt_dladvise advise)
{
    lt_module m;

    pa_assert(fname);

    if (!(m = dlopen(fname, PA_BIND_NOW))) {
#ifdef HAVE_LT_DLMUTEX_REGISTER
        libtool_set_error(dlerror());
#endif
        return NULL;
    }

    return m;
}

static int bind_now_close(lt_user_data d, lt_module m) {

    pa_assert(m);

    if (dlclose(m) != 0){
#ifdef HAVE_LT_DLMUTEX_REGISTER
        libtool_set_error(dlerror());
#endif
        return 1;
    }

    return 0;
}

static lt_ptr bind_now_find_sym(lt_user_data d, lt_module m, const char *symbol) {
    lt_ptr ptr;

    pa_assert(m);
    pa_assert(symbol);

    if (!(ptr = dlsym(m, symbol))) {
#ifdef HAVE_LT_DLMUTEX_REGISTER
        libtool_set_error(dlerror());
#endif
        return NULL;
    }

    return ptr;
}

#endif

void pa_ltdl_init(void) {

#ifdef PA_BIND_NOW
    static const lt_dlvtable *dlopen_loader;
    static lt_dlvtable bindnow_loader;
#endif

    pa_assert_se(lt_dlinit() == 0);

#ifdef PA_BIND_NOW
    /* Already initialised */
    if (dlopen_loader)
        return;

    if (!(dlopen_loader = lt_dlloader_find("dlopen"))) {
        pa_log_warn(_("Failed to find original dlopen loader."));
        return;
    }

    memcpy(&bindnow_loader, dlopen_loader, sizeof(bindnow_loader));
    bindnow_loader.name = "bind-now-loader";
    bindnow_loader.module_open = bind_now_open;
    bindnow_loader.module_close = bind_now_close;
    bindnow_loader.find_sym = bind_now_find_sym;
    bindnow_loader.priority = LT_DLLOADER_PREPEND;

    /* Add our BIND_NOW loader as the default module loader. */
    if (lt_dlloader_add(&bindnow_loader) != 0)
        pa_log_warn(_("Failed to add bind-now-loader."));
#endif
}

void pa_ltdl_done(void) {
    pa_assert_se(lt_dlexit() == 0);
}
