/*
   SSSD

   sss_groupdel

   Copyright (C) Jakub Hrozek <jhrozek@redhat.com>        2009

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <talloc.h>
#include <popt.h>
#include <grp.h>
#include <unistd.h>

#include "db/sysdb.h"
#include "util/util.h"
#include "tools/tools_util.h"
#include "tools/sss_sync_ops.h"

int main(int argc, const char **argv)
{
    int ret = EXIT_SUCCESS;
    int pc_debug = 0;
    struct group *grp_info;
    const char *pc_groupname = NULL;
    struct tools_ctx *tctx = NULL;

    poptContext pc = NULL;
    struct poptOption long_options[] = {
        POPT_AUTOHELP
        { "debug", '\0', POPT_ARG_INT | POPT_ARGFLAG_DOC_HIDDEN, &pc_debug,
                    0, _("The debug level to run with"), NULL },
        POPT_TABLEEND
    };

    debug_prg_name = argv[0];

    ret = set_locale();
    if (ret != EOK) {
        DEBUG(1, ("set_locale failed (%d): %s\n", ret, strerror(ret)));
        ERROR("Error setting the locale\n");
        ret = EXIT_FAILURE;
        goto fini;
    }

    /* parse ops_ctx */
    pc = poptGetContext(NULL, argc, argv, long_options, 0);
    poptSetOtherOptionHelp(pc, "GROUPNAME");
    if ((ret = poptGetNextOpt(pc)) < -1) {
        usage(pc, poptStrerror(ret));
        ret = EXIT_FAILURE;
        goto fini;
    }

    debug_level = pc_debug;

    pc_groupname = poptGetArg(pc);
    if (pc_groupname == NULL) {
        usage(pc, _("Specify group to delete\n"));
        ret = EXIT_FAILURE;
        goto fini;
    }

    CHECK_ROOT(ret, debug_prg_name);

    ret = init_sss_tools(&tctx);
    if (ret != EOK) {
        DEBUG(1, ("init_sss_tools failed (%d): %s\n", ret, strerror(ret)));
        ERROR("Error initializing the tools\n");
        ret = EXIT_FAILURE;
        goto fini;
    }

    /* if the domain was not given as part of FQDN, default to local domain */
    ret = parse_name_domain(tctx, pc_groupname);
    if (ret != EOK) {
        ERROR("Invalid domain specified in FQDN\n");
        ret = EXIT_FAILURE;
        goto fini;
    }

    /* arguments processed, go on to actual work */
    grp_info = getgrnam(tctx->octx->name);
    if (grp_info) {
        tctx->octx->gid = grp_info->gr_gid;
    }

    /* arguments processed, go on to actual work */
    if (id_in_range(tctx->octx->uid, tctx->octx->domain) != EOK) {
        ERROR("The selected GID is outside the allowed range\n");
        ret = EXIT_FAILURE;
        goto fini;
    }

    start_transaction(tctx);
    if (tctx->error != EOK) {
        goto done;
    }

    /* groupdel */
    ret = groupdel(tctx, tctx->ev, tctx->sysdb, tctx->handle, tctx->octx);
    if (ret != EOK) {
        tctx->error = ret;

        /* cancel transaction */
        talloc_zfree(tctx->handle);
        goto done;
    }

    end_transaction(tctx);

done:
    if (tctx->error) {
        ret = tctx->error;
        DEBUG(1, ("sysdb operation failed (%d)[%s]\n", ret, strerror(ret)));
        switch (ret) {
            case ENOENT:
                ERROR("No such group\n");
                break;

            default:
                ERROR("Internal error. Could not remove group.\n");
                break;
        }
        ret = EXIT_FAILURE;
        goto fini;
    }

    ret = EXIT_SUCCESS;

fini:
    talloc_free(tctx);
    poptFreeContext(pc);
    exit(ret);
}

