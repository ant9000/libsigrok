/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2014 Matthias Heidbrink <m-sigrok@heidbrink.biz>
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

/** @file
 *  <em>Conrad DIGI 35 CPU</em> power supply driver
 *  @internal
 */

#include "protocol.h"

#define SERIALCOMM "9600/8n1"

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

static const uint32_t devopts[] = {
	SR_CONF_POWER_SUPPLY,
	SR_CONF_OUTPUT_VOLTAGE | SR_CONF_SET,
	SR_CONF_OUTPUT_CURRENT | SR_CONF_SET,
	SR_CONF_OVER_CURRENT_PROTECTION_ENABLED | SR_CONF_SET,
};

SR_PRIV struct sr_dev_driver conrad_digi_35_cpu_driver_info;
static struct sr_dev_driver *di = &conrad_digi_35_cpu_driver_info;

static int init(struct sr_context *sr_ctx)
{
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *scan(GSList *options)
{
	struct sr_dev_inst *sdi;
	struct drv_context *drvc;
	struct sr_config *src;
	struct sr_channel *ch;
	struct sr_serial_dev_inst *serial;
	GSList *l, *devices;
	const char *conn, *serialcomm;

	devices = NULL;
	drvc = di->priv;
	drvc->instances = NULL;
	conn = serialcomm = NULL;

	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = SERIALCOMM;

	/*
	 * We cannot scan for this device because it is write-only.
	 * So just check that the port parameters are valid and assume that
	 * the device is there.
	 */

	if (!(serial = sr_serial_dev_inst_new(conn, serialcomm)))
		return NULL;

	if (serial_open(serial, SERIAL_RDWR) != SR_OK)
		return NULL;

	serial_flush(serial);
	serial_close(serial);

	sr_spew("Conrad DIGI 35 CPU assumed at %s.", conn);

	if (!(sdi = sr_dev_inst_new(SR_ST_ACTIVE, "Conrad", "DIGI 35 CPU", NULL)))
		return NULL;

	sdi->conn = serial;
	sdi->priv = NULL;
	sdi->driver = di;
	if (!(ch = sr_channel_new(0, SR_CHANNEL_ANALOG, TRUE, "CH1")))
		return NULL;
	sdi->channels = g_slist_append(sdi->channels, ch);

	drvc->instances = g_slist_append(drvc->instances, sdi);
	devices = g_slist_append(devices, sdi);

	return devices;
}

static GSList *dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static int cleanup(void)
{
	return std_dev_clear(di, NULL);
}

static int config_set(uint32_t key, GVariant *data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	int ret;
	double dblval;

	(void)cg;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_OUTPUT_VOLTAGE:
		dblval = g_variant_get_double(data);
		if ((dblval < 0.0) || (dblval > 35.0)) {
			sr_err("Voltage out of range (0 - 35.0)!");
			return SR_ERR_ARG;
		}
		ret = send_msg1(sdi, 'V', (int) (dblval * 10 + 0.5));
		break;
	case SR_CONF_OUTPUT_CURRENT:
		dblval = g_variant_get_double(data);
		if ((dblval < 0.01) || (dblval > 2.55)) {
			sr_err("Current out of range (0 - 2.55)!");
			return SR_ERR_ARG;
		}
		ret = send_msg1(sdi, 'C', (int) (dblval * 100 + 0.5));
		break;
	case SR_CONF_OVER_CURRENT_PROTECTION_ENABLED:
		if (g_variant_get_boolean(data))
			ret = send_msg1(sdi, 'V', 900);
		else /* Constant current mode */
			ret = send_msg1(sdi, 'V', 901);
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)cg;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				devopts, ARRAY_SIZE(devopts), sizeof(uint32_t));
		break;
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	return SR_OK;
}

SR_PRIV struct sr_dev_driver conrad_digi_35_cpu_driver_info = {
	.name = "conrad-digi-35-cpu",
	.longname = "Conrad DIGI 35 CPU",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = NULL,
	.config_get = NULL,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
