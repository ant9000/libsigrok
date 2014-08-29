/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 poljar (Damir Jelić) <poljarinho@gmail.com>
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

#include <stdlib.h>
#include "protocol.h"

#define SERIALCOMM "115200/8n1/flow=1"

SR_PRIV struct sr_dev_driver hameg_hmo_driver_info;
static struct sr_dev_driver *di = &hameg_hmo_driver_info;

static const char *manufacturers[] = {
	"HAMEG",
};

static const int32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

enum {
	CG_INVALID = -1,
	CG_NONE,
	CG_ANALOG,
	CG_DIGITAL,
};

static int init(struct sr_context *sr_ctx)
{
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static int check_manufacturer(const char *manufacturer)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(manufacturers); ++i)
		if (!strcmp(manufacturer, manufacturers[i]))
			return SR_OK;

	return SR_ERR;
}

static struct sr_dev_inst *hmo_probe_serial_device(struct sr_scpi_dev_inst *scpi)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_scpi_hw_info *hw_info;

	sdi = NULL;
	devc = NULL;
	hw_info = NULL;

	if (sr_scpi_get_hw_id(scpi, &hw_info) != SR_OK) {
		sr_info("Couldn't get IDN response.");
		goto fail;
	}

	if (check_manufacturer(hw_info->manufacturer) != SR_OK)
		goto fail;

	if (!(sdi = sr_dev_inst_new(0, SR_ST_ACTIVE,
				    hw_info->manufacturer, hw_info->model,
				    hw_info->firmware_version))) {
		goto fail;
	}
	sr_scpi_hw_info_free(hw_info);
	hw_info = NULL;

	if (!(devc = g_try_malloc0(sizeof(struct dev_context))))
		goto fail;

	sdi->driver = di;
	sdi->priv = devc;
	sdi->inst_type = SR_INST_SCPI;
	sdi->conn = scpi;

	if (hmo_init_device(sdi) != SR_OK)
		goto fail;

	sr_scpi_close(sdi->conn);

	sdi->status = SR_ST_INACTIVE;

	return sdi;

fail:
	if (hw_info)
		sr_scpi_hw_info_free(hw_info);
	if (sdi)
		sr_dev_inst_free(sdi);
	if (devc)
		g_free(devc);

	return NULL;
}

static GSList *scan(GSList *options)
{
	return sr_scpi_scan(di->priv, options, hmo_probe_serial_device);
}

static GSList *dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static void clear_helper(void *priv)
{
	struct dev_context *devc;

	devc = priv;

	hmo_scope_state_free(devc->model_state);

	g_free(devc->analog_groups);
	g_free(devc->digital_groups);

	g_free(devc);
}

static int dev_clear(void)
{
	return std_dev_clear(di, clear_helper);
}

static int dev_open(struct sr_dev_inst *sdi)
{
	if (sdi->status != SR_ST_ACTIVE && sr_scpi_open(sdi->conn) != SR_OK)
		return SR_ERR;

	if (hmo_scope_state_get(sdi) != SR_OK)
		return SR_ERR;

	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	if (sdi->status == SR_ST_INACTIVE)
		return SR_OK;

	sr_scpi_close(sdi->conn);

	sdi->status = SR_ST_INACTIVE;

	return SR_OK;
}

static int cleanup(void)
{
	dev_clear();

	return SR_OK;
}

static int check_channel_group(struct dev_context *devc,
			     const struct sr_channel_group *cg)
{
	unsigned int i;
	struct scope_config *model;

	model = devc->model_config;

	if (!cg)
		return CG_NONE;

	for (i = 0; i < model->analog_channels; ++i)
		if (cg == devc->analog_groups[i])
			return CG_ANALOG;

	for (i = 0; i < model->digital_pods; ++i)
		if (cg == devc->digital_groups[i])
			return CG_DIGITAL;

	sr_err("Invalid channel group specified.");

	return CG_INVALID;
}

static int config_get(int key, GVariant **data, const struct sr_dev_inst *sdi,
		      const struct sr_channel_group *cg)
{
	int ret, cg_type;
	unsigned int i;
	struct dev_context *devc;
	struct scope_config *model;
	struct scope_state *state;

	if (!sdi || !(devc = sdi->priv))
		return SR_ERR_ARG;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	ret = SR_ERR_NA;
	model = devc->model_config;
	state = devc->model_state;

	switch (key) {
	case SR_CONF_NUM_TIMEBASE:
		*data = g_variant_new_int32(model->num_xdivs);
		ret = SR_OK;
		break;
	case SR_CONF_TIMEBASE:
		*data = g_variant_new("(tt)", (*model->timebases)[state->timebase][0],
				      (*model->timebases)[state->timebase][1]);
		ret = SR_OK;
		break;
	case SR_CONF_NUM_VDIV:
		if (cg_type == CG_NONE) {
			sr_err("No channel group specified.");
			return SR_ERR_CHANNEL_GROUP;
		} else if (cg_type == CG_ANALOG) {
			for (i = 0; i < model->analog_channels; ++i) {
				if (cg != devc->analog_groups[i])
					continue;
				*data = g_variant_new_int32(model->num_ydivs);
				ret = SR_OK;
				break;
			}

		} else {
			ret = SR_ERR_NA;
		}
		break;
	case SR_CONF_VDIV:
		if (cg_type == CG_NONE) {
			sr_err("No channel group specified.");
			return SR_ERR_CHANNEL_GROUP;
		} else if (cg_type == CG_ANALOG) {
			for (i = 0; i < model->analog_channels; ++i) {
				if (cg != devc->analog_groups[i])
					continue;
				*data = g_variant_new("(tt)",
						      (*model->vdivs)[state->analog_channels[i].vdiv][0],
						      (*model->vdivs)[state->analog_channels[i].vdiv][1]);
				ret = SR_OK;
				break;
			}

		} else {
			ret = SR_ERR_NA;
		}
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_string((*model->trigger_sources)[state->trigger_source]);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_string((*model->trigger_slopes)[state->trigger_slope]);
		ret = SR_OK;
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		*data = g_variant_new_double(state->horiz_triggerpos);
		ret = SR_OK;
		break;
	case SR_CONF_COUPLING:
		if (cg_type == CG_NONE) {
			sr_err("No channel group specified.");
			return SR_ERR_CHANNEL_GROUP;
		} else if (cg_type == CG_ANALOG) {
			for (i = 0; i < model->analog_channels; ++i) {
				if (cg != devc->analog_groups[i])
					continue;
				*data = g_variant_new_string((*model->coupling_options)[state->analog_channels[i].coupling]);
				ret = SR_OK;
				break;
			}

		} else {
			ret = SR_ERR_NA;
		}
		break;
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(state->sample_rate);
		ret = SR_OK;
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static GVariant *build_tuples(const uint64_t (*array)[][2], unsigned int n)
{
	unsigned int i;
	GVariant *rational[2];
	GVariantBuilder gvb;

	g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);

	for (i = 0; i < n; i++) {
		rational[0] = g_variant_new_uint64((*array)[i][0]);
		rational[1] = g_variant_new_uint64((*array)[i][1]);

		/* FIXME: Valgrind reports a memory leak here. */
		g_variant_builder_add_value(&gvb, g_variant_new_tuple(rational, 2));
	}

	return g_variant_builder_end(&gvb);
}

static int config_set(int key, GVariant *data, const struct sr_dev_inst *sdi,
		      const struct sr_channel_group *cg)
{
	int ret, cg_type;
	unsigned int i, j;
	char command[MAX_COMMAND_SIZE], float_str[30];
	struct dev_context *devc;
	struct scope_config *model;
	struct scope_state *state;
	const char *tmp;
	uint64_t p, q;
	double tmp_d;
	gboolean update_sample_rate;

	if (!sdi || !(devc = sdi->priv))
		return SR_ERR_ARG;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	model = devc->model_config;
	state = devc->model_state;
	update_sample_rate = FALSE;

	ret = SR_ERR_NA;

	switch (key) {
	case SR_CONF_LIMIT_FRAMES:
		devc->frame_limit = g_variant_get_uint64(data);
		ret = SR_OK;
		break;
	case SR_CONF_TRIGGER_SOURCE:
		tmp = g_variant_get_string(data, NULL);
		for (i = 0; (*model->trigger_sources)[i]; i++) {
			if (g_strcmp0(tmp, (*model->trigger_sources)[i]) != 0)
				continue;
			state->trigger_source = i;
			g_snprintf(command, sizeof(command),
				   (*model->scpi_dialect)[SCPI_CMD_SET_TRIGGER_SOURCE],
				   (*model->trigger_sources)[i]);

			ret = sr_scpi_send(sdi->conn, command);
			break;
		}
		break;
	case SR_CONF_VDIV:
		if (cg_type == CG_NONE) {
			sr_err("No channel group specified.");
			return SR_ERR_CHANNEL_GROUP;
		}

		g_variant_get(data, "(tt)", &p, &q);

		for (i = 0; i < model->num_vdivs; i++) {
			if (p != (*model->vdivs)[i][0] ||
			    q != (*model->vdivs)[i][1])
				continue;
			for (j = 1; j <= model->analog_channels; ++j) {
				if (cg != devc->analog_groups[j - 1])
					continue;
				state->analog_channels[j - 1].vdiv = i;
				g_ascii_formatd(float_str, sizeof(float_str), "%E", (float) p / q);
				g_snprintf(command, sizeof(command),
					   (*model->scpi_dialect)[SCPI_CMD_SET_VERTICAL_DIV],
					   j, float_str);

				if (sr_scpi_send(sdi->conn, command) != SR_OK ||
				    sr_scpi_get_opc(sdi->conn) != SR_OK)
					return SR_ERR;

				break;
			}

			ret = SR_OK;
			break;
		}
		break;
	case SR_CONF_TIMEBASE:
		g_variant_get(data, "(tt)", &p, &q);

		for (i = 0; i < model->num_timebases; i++) {
			if (p != (*model->timebases)[i][0] ||
			    q != (*model->timebases)[i][1])
				continue;
			state->timebase = i;
			g_ascii_formatd(float_str, sizeof(float_str), "%E", (float) p / q);
			g_snprintf(command, sizeof(command),
				   (*model->scpi_dialect)[SCPI_CMD_SET_TIMEBASE],
				   float_str);

			ret = sr_scpi_send(sdi->conn, command);
			update_sample_rate = TRUE;
			break;
		}
		break;
	case SR_CONF_HORIZ_TRIGGERPOS:
		tmp_d = g_variant_get_double(data);

		if (tmp_d < 0.0 || tmp_d > 1.0)
			return SR_ERR;

		state->horiz_triggerpos = tmp_d;
		tmp_d = -(tmp_d - 0.5) *
			((double) (*model->timebases)[state->timebase][0] /
			(*model->timebases)[state->timebase][1])
			 * model->num_xdivs;

		g_ascii_formatd(float_str, sizeof(float_str), "%E", tmp_d);
		g_snprintf(command, sizeof(command),
			   (*model->scpi_dialect)[SCPI_CMD_SET_HORIZ_TRIGGERPOS],
			   float_str);

		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_TRIGGER_SLOPE:
		tmp = g_variant_get_string(data, NULL);

		if (!tmp || !(tmp[0] == 'f' || tmp[0] == 'r'))
			return SR_ERR_ARG;

		state->trigger_slope = (tmp[0] == 'r') ? 0 : 1;

		g_snprintf(command, sizeof(command),
			   (*model->scpi_dialect)[SCPI_CMD_SET_TRIGGER_SLOPE],
			   (state->trigger_slope == 0) ? "POS" : "NEG");

		ret = sr_scpi_send(sdi->conn, command);
		break;
	case SR_CONF_COUPLING:
		if (cg_type == CG_NONE) {
			sr_err("No channel group specified.");
			return SR_ERR_CHANNEL_GROUP;
		}

		tmp = g_variant_get_string(data, NULL);

		for (i = 0; (*model->coupling_options)[i]; i++) {
			if (strcmp(tmp, (*model->coupling_options)[i]) != 0)
				continue;
			for (j = 1; j <= model->analog_channels; ++j) {
				if (cg != devc->analog_groups[j - 1])
					continue;
				state->analog_channels[j-1].coupling = i;

				g_snprintf(command, sizeof(command),
					   (*model->scpi_dialect)[SCPI_CMD_SET_COUPLING],
					   j, tmp);

				if (sr_scpi_send(sdi->conn, command) != SR_OK ||
				    sr_scpi_get_opc(sdi->conn) != SR_OK)
					return SR_ERR;
				break;
			}

			ret = SR_OK;
			break;
		}
		break;
	default:
		ret = SR_ERR_NA;
		break;
	}

	if (ret == SR_OK)
		ret = sr_scpi_get_opc(sdi->conn);

	if (ret == SR_OK && update_sample_rate)
		ret = hmo_update_sample_rate(sdi);

	return ret;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi,
		       const struct sr_channel_group *cg)
{
	int cg_type;
	struct dev_context *devc;
	struct scope_config *model;

	if (!sdi || !(devc = sdi->priv))
		return SR_ERR_ARG;

	if ((cg_type = check_channel_group(devc, cg)) == CG_INVALID)
		return SR_ERR;

	model = devc->model_config;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				scanopts, ARRAY_SIZE(scanopts), sizeof(int32_t));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		if (cg_type == CG_NONE) {
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				model->hw_caps, model->num_hwcaps,
				sizeof(int32_t));
		} else if (cg_type == CG_ANALOG) {
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				model->analog_hwcaps, model->num_analog_hwcaps,
				sizeof(int32_t));
		} else {
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				NULL, 0, sizeof(int32_t));
		}
		break;
	case SR_CONF_COUPLING:
		if (cg_type == CG_NONE)
			return SR_ERR_CHANNEL_GROUP;
		*data = g_variant_new_strv(*model->coupling_options,
			   g_strv_length((char **)*model->coupling_options));
		break;
	case SR_CONF_TRIGGER_SOURCE:
		*data = g_variant_new_strv(*model->trigger_sources,
			   g_strv_length((char **)*model->trigger_sources));
		break;
	case SR_CONF_TRIGGER_SLOPE:
		*data = g_variant_new_strv(*model->trigger_slopes,
			   g_strv_length((char **)*model->trigger_slopes));
		break;
	case SR_CONF_TIMEBASE:
		*data = build_tuples(model->timebases, model->num_timebases);
		break;
	case SR_CONF_VDIV:
		if (cg_type == CG_NONE)
			return SR_ERR_CHANNEL_GROUP;
		*data = build_tuples(model->vdivs, model->num_vdivs);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

SR_PRIV int hmo_request_data(const struct sr_dev_inst *sdi)
{
	char command[MAX_COMMAND_SIZE];
	struct sr_channel *ch;
	struct dev_context *devc;
	struct scope_config *model;

	devc = sdi->priv;
	model = devc->model_config;

	ch = devc->current_channel->data;

	switch (ch->type) {
	case SR_CHANNEL_ANALOG:
		g_snprintf(command, sizeof(command),
			   (*model->scpi_dialect)[SCPI_CMD_GET_ANALOG_DATA],
			   ch->index + 1);
		break;
	case SR_CHANNEL_LOGIC:
		g_snprintf(command, sizeof(command),
			   (*model->scpi_dialect)[SCPI_CMD_GET_DIG_DATA],
			   ch->index < 8 ? 1 : 2);
		break;
	default:
		sr_err("Invalid channel type.");
		break;
	}

	return sr_scpi_send(sdi->conn, command);
}

static int hmo_check_channels(GSList *channels)
{
	GSList *l;
	struct sr_channel *ch;
	gboolean enabled_pod1, enabled_pod2, enabled_chan3, enabled_chan4;

	enabled_pod1 = enabled_pod2 = enabled_chan3 = enabled_chan4 = FALSE;

	for (l = channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			if (ch->index == 2)
				enabled_chan3 = TRUE;
			else if (ch->index == 3)
				enabled_chan4 = TRUE;
			break;
		case SR_CHANNEL_LOGIC:
			if (ch->index < 8)
				enabled_pod1 = TRUE;
			else
				enabled_pod2 = TRUE;
			break;
		default:
			return SR_ERR;
		}
	}

	if ((enabled_pod1 && enabled_chan3) ||
	    (enabled_pod2 && enabled_chan4))
		return SR_ERR;

	return SR_OK;
}

static int hmo_setup_channels(const struct sr_dev_inst *sdi)
{
	GSList *l;
	unsigned int i;
	gboolean *pod_enabled, setup_changed;
	char command[MAX_COMMAND_SIZE];
	struct scope_state *state;
	struct scope_config *model;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	devc = sdi->priv;
	scpi = sdi->conn;
	state = devc->model_state;
	model = devc->model_config;
	setup_changed = FALSE;

	pod_enabled = g_try_malloc0(sizeof(gboolean) * model->digital_pods);

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		switch (ch->type) {
		case SR_CHANNEL_ANALOG:
			if (ch->enabled == state->analog_channels[ch->index].state)
				break;
			g_snprintf(command, sizeof(command),
				   (*model->scpi_dialect)[SCPI_CMD_SET_ANALOG_CHAN_STATE],
				   ch->index + 1, ch->enabled);

			if (sr_scpi_send(scpi, command) != SR_OK)
				return SR_ERR;
			state->analog_channels[ch->index].state = ch->enabled;
			setup_changed = TRUE;
			break;
		case SR_CHANNEL_LOGIC:
			/*
			 * A digital POD needs to be enabled for every group of
			 * 8 channels.
			 */
			if (ch->enabled)
				pod_enabled[ch->index < 8 ? 0 : 1] = TRUE;

			if (ch->enabled == state->digital_channels[ch->index])
				break;
			g_snprintf(command, sizeof(command),
				   (*model->scpi_dialect)[SCPI_CMD_SET_DIG_CHAN_STATE],
				   ch->index, ch->enabled);

			if (sr_scpi_send(scpi, command) != SR_OK)
				return SR_ERR;

			state->digital_channels[ch->index] = ch->enabled;
			setup_changed = TRUE;
			break;
		default:
			return SR_ERR;
		}
	}

	for (i = 1; i <= model->digital_pods; ++i) {
		if (state->digital_pods[i - 1] == pod_enabled[i - 1])
			continue;
		g_snprintf(command, sizeof(command),
			   (*model->scpi_dialect)[SCPI_CMD_SET_DIG_POD_STATE],
			   i, pod_enabled[i - 1]);
		if (sr_scpi_send(scpi, command) != SR_OK)
			return SR_ERR;
		state->digital_pods[i - 1] = pod_enabled[i - 1];
		setup_changed = TRUE;
	}

	g_free(pod_enabled);

	if (setup_changed && hmo_update_sample_rate(sdi) != SR_OK)
		return SR_ERR;

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	GSList *l;
	gboolean digital_added;
	struct sr_channel *ch;
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	scpi = sdi->conn;
	devc = sdi->priv;
	digital_added = FALSE;

	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;

	for (l = sdi->channels; l; l = l->next) {
		ch = l->data;
		if (!ch->enabled)
			continue;
		/* Only add a single digital channel. */
		if (ch->type != SR_CHANNEL_LOGIC || !digital_added) {
			devc->enabled_channels = g_slist_append(
					devc->enabled_channels, ch);
			if (ch->type == SR_CHANNEL_LOGIC)
				digital_added = TRUE;
		}
	}

	if (!devc->enabled_channels)
		return SR_ERR;

	if (hmo_check_channels(devc->enabled_channels) != SR_OK) {
		sr_err("Invalid channel configuration specified!");
		return SR_ERR_NA;
	}

	if (hmo_setup_channels(sdi) != SR_OK) {
		sr_err("Failed to setup channel configuration!");
		return SR_ERR;
	}

	sr_scpi_source_add(sdi->session, scpi, G_IO_IN, 50,
			hmo_receive_data, (void *)sdi);

	/* Send header packet to the session bus. */
	std_session_send_df_header(cb_data, LOG_PREFIX);

	devc->current_channel = devc->enabled_channels;

	return hmo_request_data(sdi);
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	struct sr_scpi_dev_inst *scpi;
	struct sr_datafeed_packet packet;

	(void)cb_data;

	packet.type = SR_DF_END;
	packet.payload = NULL;
	sr_session_send(sdi, &packet);

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;

	devc->num_frames = 0;
	g_slist_free(devc->enabled_channels);
	devc->enabled_channels = NULL;
	scpi = sdi->conn;
	sr_scpi_source_remove(sdi->session, scpi);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver hameg_hmo_driver_info = {
	.name = "hameg-hmo",
	.longname = "Hameg HMO",
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
