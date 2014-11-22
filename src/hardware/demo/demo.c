/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2010 Uwe Hermann <uwe@hermann-uwe.de>
 * Copyright (C) 2011 Olivier Fauchon <olivier@aixmarseille.com>
 * Copyright (C) 2012 Alexandru Gagniuc <mr.nuke.me@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#endif
#include "libsigrok.h"
#include "libsigrok-internal.h"

#define LOG_PREFIX "demo"

#define DEFAULT_NUM_LOGIC_CHANNELS     8
#define DEFAULT_NUM_ANALOG_CHANNELS    4

/* The size in bytes of chunks to send through the session bus. */
#define LOGIC_BUFSIZE        4096
/* Size of the analog pattern space per channel. */
#define ANALOG_BUFSIZE       4096

#define DEFAULT_ANALOG_AMPLITUDE 25
#define ANALOG_SAMPLES_PER_PERIOD 20

/* Logic patterns we can generate. */
enum {
	/**
	 * Spells "sigrok" across 8 channels using '0's (with '1's as
	 * "background") when displayed using the 'bits' output format.
	 * The pattern is repeated every 8 channels, shifted to the right
	 * in time by one bit.
	 */
	PATTERN_SIGROK,

	/** Pseudo-random values on all channels. */
	PATTERN_RANDOM,

	/**
	 * Incrementing number across 8 channels. The pattern is repeated
	 * every 8 channels, shifted to the right in time by one bit.
	 */
	PATTERN_INC,

	/** All channels have a low logic state. */
	PATTERN_ALL_LOW,

	/** All channels have a high logic state. */
	PATTERN_ALL_HIGH,
};

/* Analog patterns we can generate. */
enum {
	/**
	 * Square wave.
	 */
	PATTERN_SQUARE,
	PATTERN_SINE,
	PATTERN_TRIANGLE,
	PATTERN_SAWTOOTH,
};

static const char *logic_pattern_str[] = {
	"sigrok",
	"random",
	"incremental",
	"all-low",
	"all-high",
};

static const char *analog_pattern_str[] = {
	"square",
	"sine",
	"triangle",
	"sawtooth",
};

struct analog_gen {
	int pattern;
	float amplitude;
	float pattern_data[ANALOG_BUFSIZE];
	unsigned int num_samples;
	struct sr_datafeed_analog packet;
};

/* Private, per-device-instance driver context. */
struct dev_context {
	int pipe_fds[2];
	GIOChannel *channel;
	uint64_t cur_samplerate;
	gboolean continuous;
	uint64_t limit_samples;
	uint64_t limit_msec;
	uint64_t logic_counter;
	uint64_t analog_counter;
	int64_t starttime;
	uint64_t step;
	/* Logic */
	int32_t num_logic_channels;
	unsigned int logic_unitsize;
	/* There is only ever one logic channel group, so its pattern goes here. */
	uint8_t logic_pattern;
	unsigned char logic_data[LOGIC_BUFSIZE];
	/* Analog */
	int32_t num_analog_channels;
	GHashTable *ch_ag;
};

static const uint32_t drvopts[] = {
	SR_CONF_DEMO_DEV,
	SR_CONF_LOGIC_ANALYZER,
	SR_CONF_OSCILLOSCOPE,
};

static const uint32_t scanopts[] = {
	SR_CONF_NUM_LOGIC_CHANNELS,
	SR_CONF_NUM_ANALOG_CHANNELS,
};

static const uint32_t devopts[] = {
	SR_CONF_CONTINUOUS | SR_CONF_SET,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_SAMPLERATE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_logic[] = {
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
};

static const uint32_t devopts_cg_analog[] = {
	SR_CONF_PATTERN_MODE | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_AMPLITUDE | SR_CONF_GET | SR_CONF_SET,
};

static const uint64_t samplerates[] = {
	SR_HZ(1),
	SR_GHZ(1),
	SR_HZ(1),
};

static uint8_t pattern_sigrok[] = {
	0x4c, 0x92, 0x92, 0x92, 0x64, 0x00, 0x00, 0x00,
	0x82, 0xfe, 0xfe, 0x82, 0x00, 0x00, 0x00, 0x00,
	0x7c, 0x82, 0x82, 0x92, 0x74, 0x00, 0x00, 0x00,
	0xfe, 0x12, 0x12, 0x32, 0xcc, 0x00, 0x00, 0x00,
	0x7c, 0x82, 0x82, 0x82, 0x7c, 0x00, 0x00, 0x00,
	0xfe, 0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xbe, 0xbe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

SR_PRIV struct sr_dev_driver demo_driver_info;
static struct sr_dev_driver *di = &demo_driver_info;

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data);


static int init(struct sr_context *sr_ctx)
{
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static void generate_analog_pattern(struct analog_gen *ag, uint64_t sample_rate)
{
	double t, frequency;
	float value;
	unsigned int num_samples, i;
	int last_end;

	sr_dbg("Generating %s pattern.", analog_pattern_str[ag->pattern]);

	num_samples = ANALOG_BUFSIZE / sizeof(float);

	switch (ag->pattern) {
	case PATTERN_SQUARE:
		value = ag->amplitude;
		last_end = 0;
		for (i = 0; i < num_samples; i++) {
			if (i % 5 == 0)
				value = -value;
			if (i % 10 == 0)
				last_end = i;
			ag->pattern_data[i] = value;
		}
		ag->num_samples = last_end;
		break;

	case PATTERN_SINE:
		frequency = (double) sample_rate / ANALOG_SAMPLES_PER_PERIOD;

		/* Make sure the number of samples we put out is an integer
		 * multiple of our period size */
		/* FIXME we actually need only one period. A ringbuffer would be
		 * usefull here.*/
		while (num_samples % ANALOG_SAMPLES_PER_PERIOD != 0)
			num_samples--;

		for (i = 0; i < num_samples; i++) {
			t = (double) i / (double) sample_rate;
			ag->pattern_data[i] = ag->amplitude *
						sin(2 * M_PI * frequency * t);
		}

		ag->num_samples = num_samples;
		break;

	case PATTERN_TRIANGLE:
		frequency = (double) sample_rate / ANALOG_SAMPLES_PER_PERIOD;

		while (num_samples % ANALOG_SAMPLES_PER_PERIOD != 0)
			num_samples--;

		for (i = 0; i < num_samples; i++) {
			t = (double) i / (double) sample_rate;
			ag->pattern_data[i] = (2 * ag->amplitude / M_PI) *
						asin(sin(2 * M_PI * frequency * t));
		}

		ag->num_samples = num_samples;
		break;

	case PATTERN_SAWTOOTH:
		frequency = (double) sample_rate / ANALOG_SAMPLES_PER_PERIOD;

		while (num_samples % ANALOG_SAMPLES_PER_PERIOD != 0)
			num_samples--;

		for (i = 0; i < num_samples; i++) {
			t = (double) i / (double) sample_rate;
			ag->pattern_data[i] = 2 * ag->amplitude *
						((t * frequency) - floor(0.5f + t * frequency));
		}

		ag->num_samples = num_samples;
		break;
	}
}

static GSList *scan(GSList *options)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_channel *ch;
	struct sr_channel_group *cg, *acg;
	struct sr_config *src;
	struct analog_gen *ag;
	GSList *devices, *l;
	int num_logic_channels, num_analog_channels, pattern, i;
	char channel_name[16];

	drvc = di->priv;

	num_logic_channels = DEFAULT_NUM_LOGIC_CHANNELS;
	num_analog_channels = DEFAULT_NUM_ANALOG_CHANNELS;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_NUM_LOGIC_CHANNELS:
			num_logic_channels = g_variant_get_int32(src->data);
			break;
		case SR_CONF_NUM_ANALOG_CHANNELS:
			num_analog_channels = g_variant_get_int32(src->data);
			break;
		}
	}

	devices = NULL;

	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_ACTIVE;
	sdi->model = g_strdup("Demo device");
	sdi->driver = di;

	devc = g_malloc(sizeof(struct dev_context));
	devc->cur_samplerate = SR_KHZ(200);
	devc->limit_samples = 0;
	devc->limit_msec = 0;
	devc->step = 0;
	devc->continuous = FALSE;
	devc->num_logic_channels = num_logic_channels;
	devc->logic_unitsize = (devc->num_logic_channels + 7) / 8;
	devc->logic_pattern = PATTERN_SIGROK;
	devc->num_analog_channels = num_analog_channels;

	/* Logic channels, all in one channel group. */
	cg = g_malloc0(sizeof(struct sr_channel_group));
	cg->name = g_strdup("Logic");
	for (i = 0; i < num_logic_channels; i++) {
		sprintf(channel_name, "D%d", i);
		ch = sr_channel_new(i, SR_CHANNEL_LOGIC, TRUE, channel_name);
		sdi->channels = g_slist_append(sdi->channels, ch);
		cg->channels = g_slist_append(cg->channels, ch);
	}
	sdi->channel_groups = g_slist_append(NULL, cg);

	/* Analog channels, channel groups and pattern generators. */
	pattern = 0;
	/* An "Analog" channel group with all analog channels in it. */
	acg = g_malloc0(sizeof(struct sr_channel_group));
	acg->name = g_strdup("Analog");
	sdi->channel_groups = g_slist_append(sdi->channel_groups, acg);

	devc->ch_ag = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (i = 0; i < num_analog_channels; i++) {
		snprintf(channel_name, 16, "A%d", i);
		ch = sr_channel_new(i + num_logic_channels, SR_CHANNEL_ANALOG,
				TRUE, channel_name);
		sdi->channels = g_slist_append(sdi->channels, ch);
		acg->channels = g_slist_append(acg->channels, ch);

		/* Every analog channel gets its own channel group as well. */
		cg = g_malloc0(sizeof(struct sr_channel_group));
		cg->name = g_strdup(channel_name);
		cg->channels = g_slist_append(NULL, ch);
		sdi->channel_groups = g_slist_append(sdi->channel_groups, cg);

		/* Every channel gets a generator struct. */
		ag = g_malloc(sizeof(struct analog_gen));
		ag->amplitude = DEFAULT_ANALOG_AMPLITUDE;
		ag->packet.channels = cg->channels;
		ag->packet.mq = 0;
		ag->packet.mqflags = 0;
		ag->packet.unit = SR_UNIT_VOLT;
		ag->packet.data = ag->pattern_data;
		ag->pattern = pattern;
		g_hash_table_insert(devc->ch_ag, ch, ag);

		if (++pattern == ARRAY_SIZE(analog_pattern_str))
			pattern = 0;
	}

	sdi->priv = devc;
	devices = g_slist_append(devices, sdi);
	drvc->instances = g_slist_append(drvc->instances, sdi);

	return devices;
}

static GSList *dev_list(void)
{
	return ((struct drv_context *)(di->priv))->instances;
}

static int dev_open(struct sr_dev_inst *sdi)
{
	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
	sdi->status = SR_ST_INACTIVE;

	return SR_OK;
}

static void clear_helper(void *priv)
{
	struct dev_context *devc;
	GHashTableIter iter;
	void *value;

	devc = priv;

	/* Analog generators. */
	g_hash_table_iter_init(&iter, devc->ch_ag);
	while (g_hash_table_iter_next(&iter, NULL, &value))
		g_free(value);
	g_hash_table_unref(devc->ch_ag);
	g_free(devc);
}

static int cleanup(void)
{
	return std_dev_clear(di, clear_helper);
}

static int config_get(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	struct analog_gen *ag;
	int pattern;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;
	switch (key) {
	case SR_CONF_SAMPLERATE:
		*data = g_variant_new_uint64(devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
	case SR_CONF_PATTERN_MODE:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		/* Any channel in the group will do. */
		ch = cg->channels->data;
		if (ch->type == SR_CHANNEL_LOGIC) {
			pattern = devc->logic_pattern;
			*data = g_variant_new_string(logic_pattern_str[pattern]);
		} else if (ch->type == SR_CHANNEL_ANALOG) {
			ag = g_hash_table_lookup(devc->ch_ag, ch);
			pattern = ag->pattern;
			*data = g_variant_new_string(analog_pattern_str[pattern]);
		} else
			return SR_ERR_BUG;
		break;
	case SR_CONF_AMPLITUDE:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		/* Any channel in the group will do. */
		ch = cg->channels->data;
		if (ch->type != SR_CHANNEL_ANALOG)
			return SR_ERR_ARG;
		ag = g_hash_table_lookup(devc->ch_ag, ch);
		*data = g_variant_new_double(ag->amplitude);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_set(uint32_t key, GVariant *data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	struct analog_gen *ag;
	struct sr_channel *ch;
	GSList *l;
	int logic_pattern, analog_pattern, ret;
	unsigned int i;
	const char *stropt;

	devc = sdi->priv;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SAMPLERATE:
		devc->cur_samplerate = g_variant_get_uint64(data);
		sr_dbg("Setting samplerate to %" PRIu64, devc->cur_samplerate);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		devc->limit_msec = 0;
		devc->limit_samples = g_variant_get_uint64(data);
		sr_dbg("Setting sample limit to %" PRIu64, devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		devc->limit_msec = g_variant_get_uint64(data);
		devc->limit_samples = 0;
		sr_dbg("Setting time limit to %" PRIu64"ms", devc->limit_msec);
		break;
	case SR_CONF_PATTERN_MODE:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		stropt = g_variant_get_string(data, NULL);
		logic_pattern = analog_pattern = -1;
		for (i = 0; i < ARRAY_SIZE(logic_pattern_str); i++) {
			if (!strcmp(stropt, logic_pattern_str[i])) {
				logic_pattern = i;
				break;
			}
		}
		for (i = 0; i < ARRAY_SIZE(analog_pattern_str); i++) {
			if (!strcmp(stropt, analog_pattern_str[i])) {
				analog_pattern = i;
				break;
			}
		}
		if (logic_pattern == -1 && analog_pattern == -1)
			return SR_ERR_ARG;
		for (l = cg->channels; l; l = l->next) {
			ch = l->data;
			if (ch->type == SR_CHANNEL_LOGIC) {
				if (logic_pattern == -1)
					return SR_ERR_ARG;
				sr_dbg("Setting logic pattern to %s",
						logic_pattern_str[logic_pattern]);
				devc->logic_pattern = logic_pattern;
				/* Might as well do this now, these are static. */
				if (logic_pattern == PATTERN_ALL_LOW)
					memset(devc->logic_data, 0x00, LOGIC_BUFSIZE);
				else if (logic_pattern == PATTERN_ALL_HIGH)
					memset(devc->logic_data, 0xff, LOGIC_BUFSIZE);
			} else if (ch->type == SR_CHANNEL_ANALOG) {
				if (analog_pattern == -1)
					return SR_ERR_ARG;
				sr_dbg("Setting analog pattern for channel %s to %s",
						ch->name, analog_pattern_str[analog_pattern]);
				ag = g_hash_table_lookup(devc->ch_ag, ch);
				ag->pattern = analog_pattern;
			} else
				return SR_ERR_BUG;
		}
		break;
	case SR_CONF_AMPLITUDE:
		if (!cg)
			return SR_ERR_CHANNEL_GROUP;
		for (l = cg->channels; l; l = l->next) {
			ch = l->data;
			if (ch->type != SR_CHANNEL_ANALOG)
				return SR_ERR_ARG;
			ag = g_hash_table_lookup(devc->ch_ag, ch);
			ag->amplitude = g_variant_get_double(data);
		}
		break;
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data, const struct sr_dev_inst *sdi,
		const struct sr_channel_group *cg)
{
	struct sr_channel *ch;
	GVariant *gvar;
	GVariantBuilder gvb;

	(void)sdi;

	if (key == SR_CONF_SCAN_OPTIONS) {
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
		return SR_OK;
	}

	if (key == SR_CONF_DEVICE_OPTIONS && !sdi) {
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				drvopts, ARRAY_SIZE(drvopts), sizeof(uint32_t));
		return SR_OK;
	}

	if (!sdi)
		return SR_ERR_ARG;

	if (!cg) {
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
					devopts, ARRAY_SIZE(devopts), sizeof(uint32_t));
			break;
		case SR_CONF_SAMPLERATE:
			g_variant_builder_init(&gvb, G_VARIANT_TYPE("a{sv}"));
			gvar = g_variant_new_fixed_array(G_VARIANT_TYPE("t"), samplerates,
					ARRAY_SIZE(samplerates), sizeof(uint64_t));
			g_variant_builder_add(&gvb, "{sv}", "samplerate-steps", gvar);
			*data = g_variant_builder_end(&gvb);
			break;
		default:
			return SR_ERR_NA;
		}
	} else {
		/* Any channel in the group will do. */
		ch = cg->channels->data;
		switch (key) {
		case SR_CONF_DEVICE_OPTIONS:
			if (ch->type == SR_CHANNEL_LOGIC)
				*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
						devopts_cg_logic, ARRAY_SIZE(devopts_cg_logic),
						sizeof(uint32_t));
			else if (ch->type == SR_CHANNEL_ANALOG)
				*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
						devopts_cg_analog, ARRAY_SIZE(devopts_cg_analog),
						sizeof(uint32_t));
			else
				return SR_ERR_BUG;
			break;
		case SR_CONF_PATTERN_MODE:
			if (ch->type == SR_CHANNEL_LOGIC)
				*data = g_variant_new_strv(logic_pattern_str,
						ARRAY_SIZE(logic_pattern_str));
			else if (ch->type == SR_CHANNEL_ANALOG)
				*data = g_variant_new_strv(analog_pattern_str,
						ARRAY_SIZE(analog_pattern_str));
			else
				return SR_ERR_BUG;
			break;
		default:
			return SR_ERR_NA;
		}
	}

	return SR_OK;
}

static void logic_generator(struct sr_dev_inst *sdi, uint64_t size)
{
	struct dev_context *devc;
	uint64_t i, j;
	uint8_t pat;

	devc = sdi->priv;

	switch (devc->logic_pattern) {
	case PATTERN_SIGROK:
		memset(devc->logic_data, 0x00, size);
		for (i = 0; i < size; i += devc->logic_unitsize) {
			for (j = 0; j < devc->logic_unitsize; j++) {
				pat = pattern_sigrok[(devc->step + j) % sizeof(pattern_sigrok)] >> 1;
				devc->logic_data[i + j] = ~pat;
			}
			devc->step++;
		}
		break;
	case PATTERN_RANDOM:
		for (i = 0; i < size; i++)
			devc->logic_data[i] = (uint8_t)(rand() & 0xff);
		break;
	case PATTERN_INC:
		for (i = 0; i < size; i++) {
			for (j = 0; j < devc->logic_unitsize; j++) {
				devc->logic_data[i + j] = devc->step;
			}
			devc->step++;
		}
		break;
	case PATTERN_ALL_LOW:
	case PATTERN_ALL_HIGH:
		/* These were set when the pattern mode was selected. */
		break;
	default:
		sr_err("Unknown pattern: %d.", devc->logic_pattern);
		break;
	}
}

/* Callback handling data */
static int prepare_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	struct analog_gen *ag;
	int ag_pattern_pos;
	GHashTableIter iter;
	void *value;
	uint64_t logic_todo, analog_todo, expected_samplenum, analog_sent, sending_now;
	int64_t time, elapsed;

	(void)fd;
	(void)revents;

	sdi = cb_data;
	devc = sdi->priv;
	logic_todo = analog_todo = 0;

	/* How many samples should we have sent by now? */
	time = g_get_monotonic_time();
	elapsed = time - devc->starttime;
	expected_samplenum = elapsed * devc->cur_samplerate / 1000000;

	/* But never more than the limit, if there is one. */
	if (!devc->continuous)
		expected_samplenum = MIN(expected_samplenum, devc->limit_samples);

	/* Of those, how many do we still have to send? */
	if (devc->num_logic_channels)
		logic_todo = expected_samplenum - devc->logic_counter;
	if (devc->num_analog_channels)
		analog_todo = expected_samplenum - devc->analog_counter;

	while (logic_todo || analog_todo) {
		/* Logic */
		if (logic_todo > 0) {
			sending_now = MIN(logic_todo, LOGIC_BUFSIZE / devc->logic_unitsize);
			logic_generator(sdi, sending_now * devc->logic_unitsize);
			packet.type = SR_DF_LOGIC;
			packet.payload = &logic;
			logic.length = sending_now * devc->logic_unitsize;
			logic.unitsize = devc->logic_unitsize;
			logic.data = devc->logic_data;
			sr_session_send(sdi, &packet);
			logic_todo -= sending_now;
			devc->logic_counter += sending_now;
		}

		/* Analog, one channel at a time */
		if (analog_todo > 0) {
			analog_sent = 0;

			g_hash_table_iter_init(&iter, devc->ch_ag);
			while (g_hash_table_iter_next(&iter, NULL, &value)) {
				ag = value;
				packet.type = SR_DF_ANALOG;
				packet.payload = &ag->packet;
				ag_pattern_pos = devc->analog_counter % ag->num_samples;
				sending_now = MIN(analog_todo, ag->num_samples-ag_pattern_pos);
				ag->packet.data = ag->pattern_data + ag_pattern_pos;
				ag->packet.num_samples = sending_now;
				sr_session_send(sdi, &packet);

				/* Whichever channel group gets there first. */
				analog_sent = MAX(analog_sent, sending_now);
			}
			analog_todo -= analog_sent;
			devc->analog_counter += analog_sent;
		}
	}

	if (!devc->continuous
			&& (!devc->num_logic_channels || devc->logic_counter >= devc->limit_samples)
			&& (!devc->num_analog_channels || devc->analog_counter >= devc->limit_samples)) {
		sr_dbg("Requested number of samples reached.");
		dev_acquisition_stop(sdi, cb_data);
		return TRUE;
	}

	return TRUE;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	GHashTableIter iter;
	void *value;

	(void)cb_data;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;
	devc->continuous = !devc->limit_samples;
	devc->logic_counter = devc->analog_counter = 0;

	/*
	 * Setting two channels connected by a pipe is a remnant from when the
	 * demo driver generated data in a thread, and collected and sent the
	 * data in the main program loop.
	 * They are kept here because it provides a convenient way of setting
	 * up a timeout-based polling mechanism.
	 */
	if (pipe(devc->pipe_fds)) {
		sr_err("%s: pipe() failed", __func__);
		return SR_ERR;
	}

	g_hash_table_iter_init(&iter, devc->ch_ag);
	while (g_hash_table_iter_next(&iter, NULL, &value))
		generate_analog_pattern(value, devc->cur_samplerate);

	devc->channel = g_io_channel_unix_new(devc->pipe_fds[0]);
	g_io_channel_set_flags(devc->channel, G_IO_FLAG_NONBLOCK, NULL);

	/* Set channel encoding to binary (default is UTF-8). */
	g_io_channel_set_encoding(devc->channel, NULL, NULL);

	/* Make channels unbuffered. */
	g_io_channel_set_buffered(devc->channel, FALSE);

	sr_session_source_add_channel(sdi->session, devc->channel,
			G_IO_IN | G_IO_ERR, 40, prepare_data, (void *)sdi);

	/* Send header packet to the session bus. */
	std_session_send_df_header(sdi, LOG_PREFIX);

	/* We use this timestamp to decide how many more samples to send. */
	devc->starttime = g_get_monotonic_time();

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;

	(void)cb_data;

	devc = sdi->priv;
	sr_dbg("Stopping acquisition.");

	sr_session_source_remove_channel(sdi->session, devc->channel);
	g_io_channel_shutdown(devc->channel, FALSE, NULL);
	g_io_channel_unref(devc->channel);
	devc->channel = NULL;

	/* Send last packet. */
	packet.type = SR_DF_END;
	sr_session_send(sdi, &packet);

	return SR_OK;
}

SR_PRIV struct sr_dev_driver demo_driver_info = {
	.name = "demo",
	.longname = "Demo driver and pattern generator",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = NULL,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.priv = NULL,
};
