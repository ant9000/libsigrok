/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012 Martin Ling <martin-git@earth.li>
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2013 Mathias Grimmberger <mgri@zaphod.sax.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"
#include "protocol.h"

/*
defs here
*/

SR_PRIV struct sr_dev_driver picotech_ps2000a_driver_info;
static struct sr_dev_driver *di = &picotech_ps2000a_driver_info;

static int init(struct sr_context *sr_ctx)
{
        return std_init(sr_ctx, di, LOG_PREFIX);
}

static void clear_helper(void *priv)
{
        // TODO
}

static int dev_clear(void)
{
        return std_dev_clear(di, clear_helper);
}

static int cleanup(void)
{
        return dev_clear();
}

static struct sr_dev_inst *probe_device(struct sr_scpi_dev_inst *scpi)
{
        // TODO
        return NULL;
}

static GSList *scan(GSList *options)
{
        return sr_scpi_scan(di->priv, options, probe_device);
}


static GSList *dev_list(void)
{
        return ((struct drv_context *)(di->priv))->instances;
}

static int dev_open(struct sr_dev_inst *sdi)
{
        // TODO
        return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
        // TODO
        return SR_OK;
}

static int config_get(int id, GVariant **data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        // TODO
        return SR_OK;
}

static int config_set(int id, GVariant *data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        // TODO
        return SR_OK;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        // TODO
        return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
        // TODO
        return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
        // TODO
        return SR_OK;
}


SR_PRIV struct sr_dev_driver picotech_ps2000a_driver_info = {
        .name = "picotech-ps2000a",
        .longname = "Picotech PicoScope 2000A",
        .api_version = 1,
        .init = init,
        .cleanup = cleanup,
        .scan = scan,
        .dev_list = dev_list,
        .dev_clear = dev_clear,
        .config_get = config_get,
        .config_set = config_set,
        .config_list = config_list,
        .dev_open = dev_open,
        .dev_close = dev_close,
        .dev_acquisition_start = dev_acquisition_start,
        .dev_acquisition_stop = dev_acquisition_stop,
        .priv = NULL,
};
