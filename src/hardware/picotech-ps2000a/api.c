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

#include "protocol.h"

static const int32_t hwcaps[] = {
        SR_CONF_OSCILLOSCOPE,
        SR_CONF_LOGIC_ANALYZER,
        SR_CONF_SAMPLERATE,
        // TODO
};

static const int32_t analog_hwcaps[] = {
        SR_CONF_NUM_VDIV,
        SR_CONF_VDIV,
        SR_CONF_COUPLING,
};

static const int32_t digital_hwcaps[] = {
        SR_CONF_VOLTAGE_THRESHOLD,
};

static const char *coupling[] = {
        "AC",
        "DC",
};


SR_PRIV struct sr_dev_driver picotech_ps2000a_driver_info;
static struct sr_dev_driver *di = &picotech_ps2000a_driver_info;

static int init(struct sr_context *sr_ctx)
{
        return std_init(sr_ctx, di, LOG_PREFIX);
}

static void clear_helper(void *priv)
{
        UNIT *devc;

        devc = priv;
        g_free(devc->analog_groups);
        g_free(devc->digital_groups);
        g_free(devc);
}

static int dev_clear(void)
{
        return std_dev_clear(di, clear_helper);
}

static int cleanup(void)
{
        return dev_clear();
}

static GSList *scan(GSList *options)
{
        PICO_STATUS status;
	int16_t handle;

        struct sr_dev_inst *sdi;
        struct sr_channel *ch;
        struct drv_context *drvc;
        UNIT *devc;
        GSList *devices,*iter;
        int i,j;
        gchar *channel_name;

        (void)options;
        drvc = di->priv;
        devices = NULL;

	do
	{
	        status = ps2000aOpenUnit(&handle, NULL);
		if(status == PICO_OK)
		{
                        if (!(devc = g_try_malloc0(sizeof(UNIT)))) {
                                sr_err("Device context malloc failed.");
                                goto end;
                        }
                        devc->handle = handle;
                        set_info(devc);

                        /* Register the device with libsigrok. */
                        sdi = sr_dev_inst_user_new("Picotech", devc->modelString, devc->serial);
                        if (!sdi) {
                                sr_err("Failed to create device instance.");
                                g_free(devc);          
                                goto end;
                        }
                        sdi->driver = di;
                        sdi->priv = devc;

                        devc->analog_groups = g_malloc0(sizeof(struct sr_channel_group*) *
                                        devc->channelCount);
                        for (i = 0; i < devc->channelCount; i++) {
                                if (!(channel_name = g_strdup_printf("%c", 'A'+i)))
                                        return NULL; // TODO: better error handling
                                ch = sr_channel_new(i, SR_CHANNEL_ANALOG, TRUE, channel_name);
                                if (!ch)
                                        return NULL; // TODO: better error handling
                                sdi->channels = g_slist_append(sdi->channels, ch);
                                // add the analog channel as a device channel group 
                                devc->analog_groups[i] = g_malloc0(sizeof(struct sr_channel_group));
                                devc->analog_groups[i]->name = channel_name;
                                devc->analog_groups[i]->channels = g_slist_append(NULL, ch);
                                sdi->channel_groups = g_slist_append(sdi->channel_groups,
                                        devc->analog_groups[i]);
                        }

                        if (devc->digitalPorts) {
                                devc->digital_groups = g_malloc0(sizeof(struct sr_channel_group*) *
                                        devc->digitalPorts);
                                for (i = 0; i < devc->digitalPorts; i++) {
                                        devc->digital_groups[i] = g_malloc0(sizeof(struct sr_channel_group));
                                        devc->digital_groups[i]->name = g_strdup_printf("Port %d",i);
                                        for (j = 0; j < 8; j++) {
                                                if (!(channel_name = g_strdup_printf("D%02d", i*8+j)))
                                                        return NULL; // TODO: better error handling
                                                ch = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, channel_name);
                                                if (!ch)
                                                        return NULL; // TODO: better error checking
                                                sdi->channels = g_slist_append(sdi->channels, ch);
                                                devc->digital_groups[i]->channels = g_slist_append(
                                                        devc->digital_groups[i]->channels, ch);
                                        }
                                        // add the 8 channels in the digital port as a device channel group 
                                        sdi->channel_groups = g_slist_append(sdi->channel_groups,
                                                devc->digital_groups[i]);
                                }
                        }

                        devices = g_slist_append(devices, sdi);
                        drvc->instances = g_slist_append(drvc->instances, sdi);
		}
	} while(status != PICO_NOT_FOUND);

        /* Close devices. We'll reopen them again when we need it. */
        for(iter = devices; iter != NULL; iter = iter->next){
           sdi  = (struct sr_dev_inst *)(iter->data);
           devc = sdi->priv;
           ps2000aCloseUnit(devc->handle);
        }

end:
        return devices;
}


static GSList *dev_list(void)
{
        return ((struct drv_context *)(di->priv))->instances;
}

static int dev_open(struct sr_dev_inst *sdi)
{
        UNIT *devc;
        PICO_STATUS status;
        devc = sdi->priv;
	status = ps2000aOpenUnit(&devc->handle, NULL);
        if (status != PICO_OK) {
                return SR_ERR;
        }
        sdi->status = SR_ST_ACTIVE;
        return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
        UNIT *devc;
        PICO_STATUS status;
        devc = sdi->priv;
        if (sdi->status == SR_ST_ACTIVE) {
                status = ps2000aCloseUnit(devc->handle);
                if (status != PICO_OK) {
                        return SR_ERR;
                }
        }
        sdi->status = SR_ST_INACTIVE;
        return SR_OK;

}

static int config_get(uint32_t id, GVariant **data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        UNIT *devc = NULL;
        int i;

        if (sdi)
                devc = sdi->priv;

        switch (id) {
        case SR_CONF_DEVICE_OPTIONS:
                if (!cg) {
                        sr_err("No channel group specified.");
                        return SR_ERR_CHANNEL_GROUP;
                }
                for (i = 0; i < devc->channelCount; i++) {
                        if (cg == devc->analog_groups[i]) {
                                // TODO
                                return SR_OK;
                        }
                }
                for (i = 0; i < devc->digitalPorts; i++) {
                        if (cg == devc->digital_groups[i]) {
                                // TODO
                                return SR_OK;
                        }
                }
                return SR_ERR_NA;
                break;
        case SR_CONF_COUPLING:
                if (!cg) {
                        sr_err("No channel group specified.");
                        return SR_ERR_CHANNEL_GROUP;
                }
                // TODO
                return SR_OK;
                break;

        // TODO

        default:
                return SR_ERR_NA;
        }

        return SR_OK;

}

static int config_set(uint32_t id, GVariant *data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        // TODO
        return SR_OK;
}

static int config_list(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
                const struct sr_channel_group *cg)
{
        UNIT *devc = NULL;
        int i;

        if (sdi)
                devc = sdi->priv;

        if (key == SR_CONF_DEVICE_OPTIONS && cg == NULL) {
                *data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
                        hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
                return SR_OK;
        }

        /* Other options require a valid device instance. */
        if (!sdi || !(devc = sdi->priv))
                return SR_ERR_ARG;

        /* If a channel group is specified, it must be a valid one. */
        if (cg && !g_slist_find(sdi->channel_groups, cg)) {
                sr_err("Invalid channel group specified.");
                return SR_ERR;
        }


        switch (key) {
        case SR_CONF_DEVICE_OPTIONS:
                if (!cg) {
                        sr_err("No channel group specified.");
                        return SR_ERR_CHANNEL_GROUP;
                }
                for (i = 0; i < devc->channelCount; i++) {
                        if (cg == devc->analog_groups[i]) {
                                *data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
                                        analog_hwcaps, ARRAY_SIZE(analog_hwcaps), sizeof(int32_t));
                                return SR_OK;
                        }
                }
                for (i = 0; i < devc->digitalPorts; i++) {
                        if (cg == devc->digital_groups[i]) {
                                *data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
                                        digital_hwcaps, ARRAY_SIZE(digital_hwcaps), sizeof(int32_t));
                                return SR_OK;
                        }
                }
                return SR_ERR_NA;
                break;
        case SR_CONF_COUPLING:
                if (!cg) {
                        sr_err("No channel group specified.");
                        return SR_ERR_CHANNEL_GROUP;
                }
                *data = g_variant_new_strv(coupling, ARRAY_SIZE(coupling));
                break;

        // TODO

        default:
                return SR_ERR_NA;
        }

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
