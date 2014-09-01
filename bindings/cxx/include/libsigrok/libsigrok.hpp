/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013-2014 Martin Ling <martin-sigrok@earth.li>
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

/**

@mainpage API Reference

Introduction
------------

The sigrok++ API provides an object-oriented C++ interface to the functionality
in libsigrok, including automatic memory and resource management.

It is built on top of the public libsigrok C API, and is designed to be used as
a standalone alternative API. Programs should not mix usage of the C and C++
APIs; the C++ interface code needs to have full control of all C API calls for
resources to be managed correctly.

Memory management
-----------------

All runtime objects created through the C++ API are passed and accessed via
shared pointers, using the C++11 std::shared_ptr implementation. This means
that a reference count is kept for each object.

Shared pointers can be copied and assigned in a user's program, automatically
updating their reference count and deleting objects when they are no longer in
use. The C++ interface code also keeps track of internal dependencies between
libsigrok resources, and ensures that objects are not prematurely deleted when
their resources are in use by other objects.

This means that management of sigrok++ objects and their underlying libsigrok
resources can be treated as fully automatic. As long as all shared pointers to
objects are deleted or reassigned when no longer in use, all underlying
resources will be released at the right time.

Getting started
---------------

Usage of the C++ API needs to begin with a call to sigrok::Context::create().
This will create the global libsigrok context and returns a shared pointer to
the sigrok::Context object. Methods on this object provide access to the
hardware drivers, input and output formats supported by the library, as well
as means of creating other objects such as sessions and triggers.

Error handling
--------------

When any libsigrok C API call returns an error, a sigrok::Error exception is
raised, which provides access to the error code and description.

*/

#ifndef LIBSIGROK_HPP
#define LIBSIGROK_HPP

#include "libsigrok/libsigrok.h"
#include <glibmm-2.4/glibmm.h>

#include <stdexcept>
#include <memory>
#include <vector>
#include <map>

namespace sigrok
{

using namespace std;

/* Forward declarations */
class SR_API Error;
class SR_API Context;
class SR_API Driver;
class SR_API Device;
class SR_API HardwareDevice;
class SR_API Channel;
class SR_API EventSource;
class SR_API Session;
class SR_API ConfigKey;
class SR_API InputFormat;
class SR_API OutputFormat;
class SR_API LogLevel;
class SR_API ChannelGroup;
class SR_API Trigger;
class SR_API TriggerStage;
class SR_API TriggerMatch;
class SR_API TriggerMatchType;
class SR_API ChannelType;
class SR_API Packet;
class SR_API PacketPayload;
class SR_API PacketType;
class SR_API Quantity;
class SR_API Unit;
class SR_API QuantityFlag;
class SR_API Input;
class SR_API InputDevice;
class SR_API Output;
class SR_API DataType;
class SR_API Option;

/** Exception thrown when an error code is returned by any libsigrok call. */
class SR_API Error: public exception
{
public:
	Error(int result);
	~Error() throw();
	const int result;
	const char *what() const throw();
};

/* Base template for most classes which wrap a struct type from libsigrok. */
template <class Parent, typename Struct> class SR_API StructureWrapper :
	public enable_shared_from_this<StructureWrapper<Parent, Struct> >
{
protected:
	/*  Parent object which owns this child object's underlying structure.

		This shared pointer will be null when this child is unused, but
		will be assigned to point to the parent before any shared pointer
		to this child is handed out to the user.

		When the reference count of this child falls to zero, this shared
		pointer to its parent is reset by a custom deleter on the child's
		shared pointer.

		This strategy ensures that the destructors for both the child and
		the parent are called at the correct time, i.e. only when all
		references to both the parent and all its children are gone. */
	shared_ptr<Parent> parent;

public:
	shared_ptr<StructureWrapper<Parent, Struct> >
	get_shared_pointer(Parent *parent)
	{
		if (!parent)
			throw Error(SR_ERR_BUG);
		this->parent = static_pointer_cast<Parent>(parent->shared_from_this());
		return shared_ptr<StructureWrapper<Parent, Struct> >(
			this, reset_parent);
	}
	shared_ptr<StructureWrapper<Parent, Struct> >
	get_shared_pointer(shared_ptr<Parent> parent)
	{
		if (!parent)
			throw Error(SR_ERR_BUG);
		this->parent = parent;
		return shared_ptr<StructureWrapper<Parent, Struct> >(
			this, reset_parent);
	}
protected:
	static void reset_parent(StructureWrapper<Parent, Struct> *object)
	{
		if (!object->parent)
			throw Error(SR_ERR_BUG);
		object->parent.reset();
	}

	Struct *structure;

	StructureWrapper<Parent, Struct>(Struct *structure) :
		structure(structure)
	{
	}
};

/** Type of log callback */
typedef function<void(const LogLevel *, string message)> LogCallbackFunction;

/** The global libsigrok context */
class SR_API Context : public enable_shared_from_this<Context>
{
public:
	/** Create new context */
	static shared_ptr<Context> create();
	/** libsigrok package version. */
	string get_package_version();
	/** libsigrok library version. */
	string get_lib_version();
	/** Available hardware drivers, indexed by name. */
	map<string, shared_ptr<Driver> > get_drivers();
	/** Available input formats, indexed by name. */
	map<string, shared_ptr<InputFormat> > get_input_formats();
	/** Available output formats, indexed by name. */
	map<string, shared_ptr<OutputFormat> > get_output_formats();
	/** Current log level. */
	const LogLevel *get_log_level();
	/** Set the log level.
	 * @param level LogLevel to use. */
	void set_log_level(const LogLevel *level);
	/** Current log domain. */
	string get_log_domain();
	/** Set the log domain.
	 * @param value Log domain prefix string. */
	void set_log_domain(string value);
	/** Set the log callback.
	 * @param callback Callback of the form callback(LogLevel, string). */
	void set_log_callback(LogCallbackFunction callback);
	/** Set the log callback to the default handler. */
	void set_log_callback_default();
	/** Create a new session. */
	shared_ptr<Session> create_session();
	/** Load a saved session.
	 * @param filename File name string. */
	shared_ptr<Session> load_session(string filename);
	/** Create a new trigger.
	 * @param name Name string for new trigger. */
	shared_ptr<Trigger> create_trigger(string name);
	/** Open an input file.
	 * @param filename File name string. */
	shared_ptr<Input> open_file(string filename);
	/** Open an input stream based on header data.
	 * @param header Initial data from stream. */
	shared_ptr<Input> open_stream(string header);
protected:
	struct sr_context *structure;
	map<string, Driver *> drivers;
	map<string, InputFormat *> input_formats;
	map<string, OutputFormat *> output_formats;
	Session *session;
	LogCallbackFunction log_callback;
	Context();
	~Context();
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Context *context) { delete context; }
	};
	friend class Deleter;
	friend class Session;
	friend class Driver;
};

/** A hardware driver provided by the library */
class SR_API Driver : public StructureWrapper<Context, struct sr_dev_driver>
{
public:
	/** Name of this driver. */
	string get_name();
	/** Long name for this driver. */
	string get_long_name();
	/** Scan for devices and return a list of devices found.
	 * @param options Mapping of (ConfigKey, value) pairs. */
	vector<shared_ptr<HardwareDevice> > scan(
		map<const ConfigKey *, Glib::VariantBase> options = {});
protected:
	bool initialized;
	vector<HardwareDevice *> devices;
	Driver(struct sr_dev_driver *structure);
	~Driver();
	friend class Context;
	friend class HardwareDevice;
	friend class ChannelGroup;
};

/** An object that can be configured. */
class SR_API Configurable
{
public:
	/** Read configuration for the given key.
	 * @param key ConfigKey to read. */
	Glib::VariantBase config_get(const ConfigKey *key);
	/** Set configuration for the given key to a specified value.
	 * @param key ConfigKey to set.
	 * @param value Value to set. */
	void config_set(const ConfigKey *key, Glib::VariantBase value);
	/** Enumerate available values for the given configuration key.
	 * @param key ConfigKey to enumerate values for. */
	Glib::VariantContainerBase config_list(const ConfigKey *key);
protected:
	Configurable(
		struct sr_dev_driver *driver,
		struct sr_dev_inst *sdi,
		struct sr_channel_group *channel_group);
	virtual ~Configurable();
	struct sr_dev_driver *config_driver;
	struct sr_dev_inst *config_sdi;
	struct sr_channel_group *config_channel_group;
};

/** A generic device, either hardware or virtual */
class SR_API Device : public Configurable
{
public:
	/** Description identifying this device. */
	string get_description();
	/** Vendor name for this device. */
	string get_vendor();
	/** Model name for this device. */
	string get_model();
	/** Version string for this device. */
	string get_version();
	/** List of the channels available on this device. */
	vector<shared_ptr<Channel> > get_channels();
	/** Channel groups available on this device, indexed by name. */
	map<string, shared_ptr<ChannelGroup> > get_channel_groups();
	/** Open device. */
	void open();
	/** Close device. */
	void close();
protected:
	Device(struct sr_dev_inst *structure);
	~Device();
	virtual shared_ptr<Device> get_shared_from_this() = 0;
	shared_ptr<Channel> get_channel(struct sr_channel *ptr);
	struct sr_dev_inst *structure;
	map<struct sr_channel *, Channel *> channels;
	map<string, ChannelGroup *> channel_groups;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Device *device) { delete device; }
	};
	friend class Deleter;
	friend class Session;
	friend class Channel;
	friend class ChannelGroup;
	friend class Output;
	friend class Analog;
};

/** A real hardware device, connected via a driver */
class SR_API HardwareDevice :
	public StructureWrapper<Context, struct sr_dev_inst>,
	public Device
{
public:
	/** Driver providing this device. */
	shared_ptr<Driver> get_driver();
protected:
	HardwareDevice(Driver *driver, struct sr_dev_inst *structure);
	~HardwareDevice();
	shared_ptr<Device> get_shared_from_this();
	Driver *driver;
	friend class Driver;
	friend class ChannelGroup;
};

/** A channel on a device */
class SR_API Channel : public StructureWrapper<Device, struct sr_channel>
{
public:
	/** Current name of this channel. */
	string get_name();
	/** Set the name of this channel. *
	 * @param name Name string to set. */
	void set_name(string name);
	/** Type of this channel. */
	const ChannelType *get_type();
	/** Enabled status of this channel. */
	bool get_enabled();
	/** Set the enabled status of this channel.
	 * @param value Boolean value to set. */
	void set_enabled(bool value);
	/** Get the index number of this channel. */
	unsigned int get_index();
protected:
	Channel(struct sr_channel *structure);
	~Channel();
	const ChannelType * const type;
	friend class Device;
	friend class ChannelGroup;
	friend class Session;
	friend class TriggerStage;
};

/** A group of channels on a device, which share some configuration */
class SR_API ChannelGroup :
	public StructureWrapper<Device, struct sr_channel_group>,
	public Configurable
{
public:
	/** Name of this channel group. */
	string get_name();
	/** List of the channels in this group. */
	vector<shared_ptr<Channel> > get_channels();
protected:
	ChannelGroup(Device *device, struct sr_channel_group *structure);
	~ChannelGroup();
	vector<Channel *> channels;
	friend class Device;
};

/** A trigger configuration */
class SR_API Trigger : public enable_shared_from_this<Trigger>
{
public:
	/** Name of this trigger configuration. */
	string get_name();
	/** List of the stages in this trigger. */
	vector<shared_ptr<TriggerStage> > get_stages();
	/** Add a new stage to this trigger. */
	shared_ptr<TriggerStage> add_stage();
protected:
	Trigger(shared_ptr<Context> context, string name);
	~Trigger();
	struct sr_trigger *structure;
	shared_ptr<Context> context;
	vector<TriggerStage *> stages;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Trigger *trigger) { delete trigger; }
	};
	friend class Context;
	friend class Session;
};

/** A stage in a trigger configuration */
class SR_API TriggerStage : public StructureWrapper<Trigger, struct sr_trigger_stage>
{
public:
	/** Index number of this stage. */
	int get_number();
	/** List of match conditions on this stage. */
	vector<shared_ptr<TriggerMatch> > get_matches();
	/** Add a new match condition to this stage.
	 * @param channel Channel to match on.
	 * @param type TriggerMatchType to apply. */
	void add_match(shared_ptr<Channel> channel, const TriggerMatchType *type);
	/** Add a new match condition to this stage.
	 * @param channel Channel to match on.
	 * @param type TriggerMatchType to apply.
	 * @param value Threshold value. */
	void add_match(shared_ptr<Channel> channel, const TriggerMatchType *type, float value);
protected:
	vector<TriggerMatch *> matches;
	TriggerStage(struct sr_trigger_stage *structure);
	~TriggerStage();
	friend class Trigger;
};

/** A match condition in a trigger configuration  */
class SR_API TriggerMatch : public StructureWrapper<TriggerStage, struct sr_trigger_match>
{
public:
	/** Channel this condition matches on. */
	shared_ptr<Channel> get_channel();
	/** Type of match. */
	const TriggerMatchType *get_type();
	/** Threshold value. */
	float get_value();
protected:
	TriggerMatch(struct sr_trigger_match *structure, shared_ptr<Channel> channel);
	~TriggerMatch();
	shared_ptr<Channel> channel;
	friend class TriggerStage;
};

/** Type of datafeed callback */
typedef function<void(shared_ptr<Device>, shared_ptr<Packet>)>
	DatafeedCallbackFunction;

/* Data required for C callback function to call a C++ datafeed callback */
class SR_PRIV DatafeedCallbackData
{
public:
	void run(const struct sr_dev_inst *sdi,
		const struct sr_datafeed_packet *pkt);
protected:
	DatafeedCallbackFunction callback;
	DatafeedCallbackData(Session *session,
		DatafeedCallbackFunction callback);
	Session *session;
	friend class Session;
};

/** Type of source callback */
typedef function<bool(Glib::IOCondition)>
	SourceCallbackFunction;

/* Data required for C callback function to call a C++ source callback */
class SR_PRIV SourceCallbackData
{
public:
	bool run(int revents);
protected:
	SourceCallbackData(shared_ptr<EventSource> source);
	shared_ptr<EventSource> source;
	friend class Session;
};

/** An I/O event source */
class SR_API EventSource
{
public:
	/** Create an event source from a file descriptor.
	 * @param fd File descriptor.
	 * @param events GLib IOCondition event mask.
	 * @param timeout Timeout in milliseconds.
	 * @param callback Callback of the form callback(events) */
	static shared_ptr<EventSource> create(int fd, Glib::IOCondition events,
		int timeout, SourceCallbackFunction callback);
	/** Create an event source from a GLib PollFD
	 * @param pollfd GLib PollFD
	 * @param timeout Timeout in milliseconds.
	 * @param callback Callback of the form callback(events) */
	static shared_ptr<EventSource> create(Glib::PollFD pollfd, int timeout,
		SourceCallbackFunction callback);
	/** Create an event source from a GLib IOChannel
	 * @param channel GLib IOChannel.
	 * @param events GLib IOCondition event mask.
	 * @param timeout Timeout in milliseconds.
	 * @param callback Callback of the form callback(events) */
	static shared_ptr<EventSource> create(
		Glib::RefPtr<Glib::IOChannel> channel, Glib::IOCondition events,
		int timeout, SourceCallbackFunction callback);
protected:
	EventSource(int timeout, SourceCallbackFunction callback);
	~EventSource();
	enum source_type {
		SOURCE_FD,
		SOURCE_POLLFD,
		SOURCE_IOCHANNEL
	} type;
	int fd;
	Glib::PollFD pollfd;
	Glib::RefPtr<Glib::IOChannel> channel;
	Glib::IOCondition events;
	int timeout;
	SourceCallbackFunction callback;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(EventSource *source) { delete source; }
	};
	friend class Deleter;
	friend class Session;
	friend class SourceCallbackData;
};

/** A sigrok session */
class SR_API Session 
{
public:
	/** Add a device to this session.
	 * @param device Device to add. */
	void add_device(shared_ptr<Device> device);
	/** List devices attached to this session. */
	vector<shared_ptr<Device> > get_devices();
	/** Remove all devices from this session. */
	void remove_devices();
	/** Add a datafeed callback to this session.
	 * @param callback Callback of the form callback(Device, Packet). */
	void add_datafeed_callback(DatafeedCallbackFunction callback);
	/** Remove all datafeed callbacks from this session. */
	void remove_datafeed_callbacks();
	/** Add an I/O event source.
	 * @param source EventSource to add. */
	void add_source(shared_ptr<EventSource> source);
	/** Remove an event source.
	 * @param source EventSource to remove. */
	void remove_source(shared_ptr<EventSource> source);
	/** Start the session. */
	void start();
	/** Run the session event loop. */
	void run();
	/** Stop the session. */
	void stop();
	/** Begin saving session to a file.
	 * @param filename File name string. */
	void begin_save(string filename);
	/** Append a packet to the session file being saved.
	 * @param packet Packet to append. */
	void append(shared_ptr<Packet> packet);
	/** Append raw logic data to the session file being saved. */
	void append(void *data, size_t length, unsigned int unit_size);
	/** Get current trigger setting. */
	shared_ptr<Trigger> get_trigger();
	/** Set trigger setting.
	 * @param trigger Trigger object to use. */
	void set_trigger(shared_ptr<Trigger> trigger);
protected:
	Session(shared_ptr<Context> context);
	Session(shared_ptr<Context> context, string filename);
	~Session();
	struct sr_session *structure;
	const shared_ptr<Context> context;
	map<const struct sr_dev_inst *, shared_ptr<Device> > devices;
	vector<DatafeedCallbackData *> datafeed_callbacks;
	map<shared_ptr<EventSource>, SourceCallbackData *> source_callbacks;
	bool saving;
	bool save_initialized;
	string save_filename;
	uint64_t save_samplerate;
	shared_ptr<Trigger> trigger;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Session *session) { delete session; }
	};
	friend class Deleter;
	friend class Context;
	friend class DatafeedCallbackData;
};

/** A packet on the session datafeed */
class SR_API Packet : public enable_shared_from_this<Packet>
{
public:
	/** Type of this packet. */
	const PacketType *get_type();
	/** Payload of this packet. */
	shared_ptr<PacketPayload> get_payload();
protected:
	Packet(shared_ptr<Device> device,
		const struct sr_datafeed_packet *structure);
	~Packet();
	const struct sr_datafeed_packet *structure;
	shared_ptr<Device> device;
	PacketPayload *payload;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Packet *packet) { delete packet; }
	};
	friend class Deleter;
	friend class Session;
	friend class Output;
	friend class DatafeedCallbackData;
	friend class Header;
	friend class Meta;
	friend class Logic;
	friend class Analog;
};

/** Abstract base class for datafeed packet payloads */
class SR_API PacketPayload
{
protected:
	PacketPayload();
	virtual ~PacketPayload() = 0;
	virtual shared_ptr<PacketPayload> get_shared_pointer(Packet *parent) = 0;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(PacketPayload *payload) { delete payload; }
	};
	friend class Deleter;
	friend class Packet;
	friend class Output;
};

/** Payload of a datafeed header packet */
class SR_API Header :
	public StructureWrapper<Packet, const struct sr_datafeed_header>,
	public PacketPayload
{
public:
	/* Feed version number. */
	int get_feed_version();
	/* Start time of this session. */
	Glib::TimeVal get_start_time();
protected:
	Header(const struct sr_datafeed_header *structure);
	~Header();
	shared_ptr<PacketPayload> get_shared_pointer(Packet *parent);
	friend class Packet;
};

/** Payload of a datafeed metadata packet */
class SR_API Meta :
	public StructureWrapper<Packet, const struct sr_datafeed_meta>,
	public PacketPayload
{
public:
	/* Mapping of (ConfigKey, value) pairs. */
	map<const ConfigKey *, Glib::VariantBase> get_config();
protected:
	Meta(const struct sr_datafeed_meta *structure);
	~Meta();
	shared_ptr<PacketPayload> get_shared_pointer(Packet *parent);
	map<const ConfigKey *, Glib::VariantBase> config;
	friend class Packet;
};

/** Payload of a datafeed packet with logic data */
class SR_API Logic :
	public StructureWrapper<Packet, const struct sr_datafeed_logic>,
	public PacketPayload
{
public:
	/* Pointer to data. */
	void *get_data_pointer();
	/* Data length in bytes. */
	size_t get_data_length();
	/* Size of each sample in bytes. */
	unsigned int get_unit_size();
protected:
	Logic(const struct sr_datafeed_logic *structure);
	~Logic();
	shared_ptr<PacketPayload> get_shared_pointer(Packet *parent);
	friend class Packet;
};

/** Payload of a datafeed packet with analog data */
class SR_API Analog :
	public StructureWrapper<Packet, const struct sr_datafeed_analog>,
	public PacketPayload
{
public:
	/** Pointer to data. */
	float *get_data_pointer();
	/** Number of samples in this packet. */
	unsigned int get_num_samples();
	/** Channels for which this packet contains data. */
	vector<shared_ptr<Channel> > get_channels();
	/** Measured quantity of the samples in this packet. */
	const Quantity *get_mq();
	/** Unit of the samples in this packet. */
	const Unit *get_unit();
	/** Measurement flags associated with the samples in this packet. */
	vector<const QuantityFlag *> get_mq_flags();
protected:
	Analog(const struct sr_datafeed_analog *structure);
	~Analog();
	shared_ptr<PacketPayload> get_shared_pointer(Packet *parent);
	friend class Packet;
};

/** An input format supported by the library */
class SR_API InputFormat :
	public StructureWrapper<Context, const struct sr_input_module>
{
public:
	/** Name of this input format. */
	string get_name();
	/** Description of this input format. */
	string get_description();
	/** Options supported by this input format. */
	map<string, shared_ptr<Option> > get_options();
	/** Create an input using this input format.
	 * @param options Mapping of (option name, value) pairs. */
	shared_ptr<Input> create_input(map<string, Glib::VariantBase> options = {});
protected:
	InputFormat(const struct sr_input_module *structure);
	~InputFormat();
	friend class Context;
	friend class InputDevice;
};

/** An input instance (an input format applied to a file or stream) */
class SR_API Input : public enable_shared_from_this<Input>
{
public:
	/** Virtual device associated with this input. */
	shared_ptr<InputDevice> get_device();
	/** Send next stream data.
	 * @param data Next stream data. */
	void send(string data);
protected:
	Input(shared_ptr<Context> context, const struct sr_input *structure);
	~Input();
	shared_ptr<Device> get_shared_from_this();
	const struct sr_input *structure;
	shared_ptr<Context> context;
	InputDevice *device;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Input *input) { delete input; }
	};
	friend class Deleter;
	friend class Context;
	friend class InputFormat;
};

/** A virtual device associated with an input */
class SR_API InputDevice :
	public StructureWrapper<Input, struct sr_dev_inst>,
	public Device
{
protected:
	InputDevice(shared_ptr<Input> input, struct sr_dev_inst *sdi);
	~InputDevice();
	shared_ptr<Device> get_shared_from_this();
	shared_ptr<Input> input;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(InputDevice *device) { delete device; }
	};
	friend class Deleter;
	friend class Input;
};

/** An option used by an output format */
class SR_API Option
{
public:
	/** Short name of this option suitable for command line usage. */
	string get_id();
	/** Short name of this option suitable for GUI usage. */
	string get_name();
	/** Description of this option in a sentence. */
	string get_description();
	/** Default value for this option. */
	Glib::VariantBase get_default_value();
	/** Possible values for this option, if a limited set. */
	vector<Glib::VariantBase> get_values();
protected:
	Option(const struct sr_option *structure,
		shared_ptr<const struct sr_option *> structure_array);
	~Option();
	const struct sr_option *structure;
	shared_ptr<const struct sr_option *> structure_array;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Option *option) { delete option; }
	};
	friend class Deleter;
	friend class InputFormat;
	friend class OutputFormat;
};

/** An output format supported by the library */
class SR_API OutputFormat :
	public StructureWrapper<Context, const struct sr_output_module>
{
public:
	/** Name of this output format. */
	string get_name();
	/** Description of this output format. */
	string get_description();
	/** Options supported by this output format. */
	map<string, shared_ptr<Option> > get_options();
	/** Create an output using this format.
	 * @param device Device to output for.
	 * @param options Mapping of (option name, value) pairs. */
	shared_ptr<Output> create_output(shared_ptr<Device> device,
		map<string, Glib::VariantBase> options = {});
protected:
	OutputFormat(const struct sr_output_module *structure);
	~OutputFormat();
	friend class Context;
	friend class Output;
};

/** An output instance (an output format applied to a device) */
class SR_API Output
{
public:
	/** Update output with data from the given packet.
	 * @param packet Packet to handle. */
	string receive(shared_ptr<Packet> packet);
protected:
	Output(shared_ptr<OutputFormat> format, shared_ptr<Device> device);
	Output(shared_ptr<OutputFormat> format,
		shared_ptr<Device> device, map<string, Glib::VariantBase> options);
	~Output();
	const struct sr_output *structure;
	const shared_ptr<OutputFormat> format;
	const shared_ptr<Device> device;
	const map<string, Glib::VariantBase> options;
	/** Deleter needed to allow shared_ptr use with protected destructor. */
	class Deleter
	{
	public:
		void operator()(Output *output) { delete output; }
	};
	friend class Deleter;
	friend class OutputFormat;
};

/** Base class for objects which wrap an enumeration value from libsigrok */
template <typename T> class SR_API EnumValue
{
public:
	/** The enum constant associated with this value. */
	T get_id() const { return id; }
	/** The name associated with this value. */
	string get_name() const { return name; }
protected:
	EnumValue(T id, const char name[]) : id(id), name(name) {}
	~EnumValue() {}
	const T id;
	const string name;
};

#include "enums.hpp"

}

#endif // LIBSIGROK_HPP
