const DataType *ConfigKey::data_type() const
{
	const struct sr_config_info *info = sr_config_info_get(_id);
	if (!info)
		throw Error(SR_ERR_NA);
	return DataType::get(info->datatype);
}

string ConfigKey::identifier() const
{
	const struct sr_config_info *info = sr_config_info_get(_id);
	if (!info)
		throw Error(SR_ERR_NA);
	return valid_string(info->id);
}

string ConfigKey::description() const
{
	const struct sr_config_info *info = sr_config_info_get(_id);
	if (!info)
		throw Error(SR_ERR_NA);
	return valid_string(info->name);
}

const ConfigKey *ConfigKey::get_by_identifier(string identifier)
{
	const struct sr_config_info *info = sr_config_info_name_get(identifier.c_str());
	if (!info)
		throw Error(SR_ERR_ARG);
	return get(info->key);
}

#include "config.h"

#ifndef HAVE_STOI_STOD

/* Fallback implementation of stoi and stod */

#include <cstdlib>
#include <cerrno>
#include <stdexcept>
#include <limits>

static inline int stoi( const std::string& str )
{
	char *endptr;
	errno = 0;
	const long ret = std::strtol(str.c_str(), &endptr, 10);
	if (endptr == str.c_str())
		throw std::invalid_argument("stoi");
	else if (errno == ERANGE ||
		 ret < std::numeric_limits<int>::min() ||
		 ret > std::numeric_limits<int>::max())
		throw std::out_of_range("stoi");
	else
		return ret;
}

static inline double stod( const std::string& str )
{
	char *endptr;
	errno = 0;
	const double ret = std::strtod(str.c_str(), &endptr);
	if (endptr == str.c_str())
		throw std::invalid_argument("stod");
	else if (errno == ERANGE)
		throw std::out_of_range("stod");
	else
		return ret;
}
#endif

Glib::VariantBase ConfigKey::parse_string(string value) const
{
	GVariant *variant;
	uint64_t p, q;

	switch (data_type()->id())
	{
		case SR_T_UINT64:
			check(sr_parse_sizestring(value.c_str(), &p));
			variant = g_variant_new_uint64(p);
			break;
		case SR_T_STRING:
			variant = g_variant_new_string(value.c_str());
			break;
		case SR_T_BOOL:
			variant = g_variant_new_boolean(sr_parse_boolstring(value.c_str()));
			break;
		case SR_T_FLOAT:
			variant = g_variant_new_double(stod(value));
			break;
		case SR_T_RATIONAL_PERIOD:
			check(sr_parse_period(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case SR_T_RATIONAL_VOLT:
			check(sr_parse_voltage(value.c_str(), &p, &q));
			variant = g_variant_new("(tt)", p, q);
			break;
		case SR_T_INT32:
			variant = g_variant_new_int32(stoi(value));
			break;
		default:
			throw Error(SR_ERR_BUG);
	}

	return Glib::VariantBase(variant, false);
}

