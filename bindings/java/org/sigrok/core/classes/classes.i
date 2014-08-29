%module classes

/* Automatically load JNI library. */
%pragma(java) jniclasscode=%{
  static {
    System.loadLibrary("sigrok_java_core_classes");
  }
%}

/* Documentation & importing interfaces. */
%pragma(java) jniclassimports=%{
/**
 * @mainpage API Reference
 * 
 * Introduction
 * ------------
 * 
 * The sigrok-java API provides an object-oriented Java interface to the
 * functionality in libsigrok. It is built on top of the sigrok++ C++ API.
 * 
 * Getting started
 * ---------------
 * 
 * Usage of the sigrok-java API needs to begin with a call to Context.create().
 * This will create the global libsigrok context and returns a Context object.
 * Methods on this object provide access to the hardware drivers, input and
 * output formats supported by the library, as well as means of creating other
 * objects such as sessions and triggers.
 * 
 * Error handling
 * --------------
 * 
 * When any libsigrok C API call returns an error, an Error exception is raised,
 * which provides access to the error code and description.
 */

import org.sigrok.core.interfaces.LogCallback;
import org.sigrok.core.interfaces.DatafeedCallback;
import org.sigrok.core.interfaces.SourceCallback;
%}

/* Map Java FileDescriptor objects to int fds */
%typemap(jni) int fd "jobject"
%typemap(jtype) int fd "java.io.FileDescriptor"
%typemap(jstype) int fd "java.io.FileDescriptor"
%typemap(javain) int fd "$javainput"

%typemap(in) int fd {
  jclass FileDescriptor = jenv->FindClass("java/io/FileDescriptor");
  jfieldID fd = jenv->GetFieldID(FileDescriptor, "fd", "I");
  $1 = jenv->GetIntField($input, fd);
}

/* Map Glib::VariantBase to a Variant class in Java */
%rename(Variant) VariantBase;
namespace Glib {
  class VariantBase {};
}

/* Map between std::vector and java.util.Vector */
%define VECTOR(CValue, JValue)

%typemap(jni) std::vector< CValue > "jobject"
%typemap(jtype) std::vector< CValue > "java.util.Vector<JValue>"
%typemap(jstype) std::vector< CValue > "java.util.Vector<JValue>"

%typemap(javain,
    pre="  $javaclassname temp$javainput = $javaclassname.convertVector($javainput);",
    pgcppname="temp$javainput")
  std::vector< CValue > "$javaclassname.getCPtr(temp$javainput)"

%typemap(javacode) std::vector< CValue > %{
  static $javaclassname convertVector(java.util.Vector<JValue> in)
  {
    $javaclassname out = new $javaclassname();
    for (JValue value : in)
      out.add(value);
    return out;
  }
%}

%typemap(javaout) std::vector< CValue > {
  return (java.util.Vector<JValue>)$jnicall;
}

%typemap(out) std::vector< CValue > {
  jclass Vector = jenv->FindClass("java/util/Vector");
  jmethodID Vector_init = jenv->GetMethodID(Vector, "<init>", "()V");
  jmethodID Vector_add = jenv->GetMethodID(Vector, "add",
    "(Ljava/lang/Object;)Z");
  jclass Value = jenv->FindClass("org/sigrok/core/classes/" #JValue);
  jmethodID Value_init = jenv->GetMethodID(Value, "<init>", "(JZ)V");
  $result = jenv->NewObject(Vector, Vector_init);
  jlong value;
  for (auto entry : $1)
  {
    *(CValue **) &value = new CValue(entry);
    jenv->CallObjectMethod($result, Vector_add,
      jenv->NewObject(Value, Value_init, value, true));
  }
}

%enddef

VECTOR(std::shared_ptr<sigrok::Channel>, Channel)
VECTOR(std::shared_ptr<sigrok::HardwareDevice>, HardwareDevice)

/* Common macro for mapping between std::map and java.util.Map */

%define MAP_COMMON(CKey, CValue, JKey, JValue)

%typemap(jstype) std::map< CKey, CValue >
  "java.util.Map<JKey, JValue>"

%typemap(javain,
    pre="  $javaclassname temp$javainput = $javaclassname.convertMap($javainput);",
    pgcppname="temp$javainput")
  std::map< CKey, CValue > "$javaclassname.getCPtr(temp$javainput)"

%typemap(javacode) std::map< CKey, CValue > %{
  static $javaclassname convertMap(java.util.Map<JKey,JValue> in)
  {
    $javaclassname out = new $javaclassname();
    for (java.util.Map.Entry<JKey, JValue> entry : in.entrySet())
      out.set(entry.getKey(), entry.getValue());
    return out;
  }
%}

%typemap(javaout) std::map< CKey, CValue > {
  return (java.util.Map<JKey, JValue>)$jnicall;
}

%enddef

/* Specialisation for string->string maps. */

MAP_COMMON(std::string, std::string, String, String)

%typemap(out) std::map<std::string, std::string> {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  $result = jenv->NewObject(HashMap, init);
  for (auto entry : $1)
    jenv->CallObjectMethod($result, put,
      jenv->NewStringUTF(entry.first.c_str()),
      jenv->NewStringUTF(entry.second.c_str()));
}

/* Specialisation macro for string->shared_ptr maps. */

%define STRING_TO_SHARED_PTR_MAP(ClassName)

%typemap(jni) std::map<std::string, std::shared_ptr<sigrok::ClassName> >
  "jobject"
%typemap(jtype) std::map<std::string, std::shared_ptr<sigrok::ClassName> >
  "java.util.Map<String,ClassName>"

MAP_COMMON(std::string, std::shared_ptr<sigrok::ClassName>, String, ClassName)

%typemap(out) std::map<std::string, std::shared_ptr<sigrok::ClassName> > {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID HashMap_init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID HashMap_put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  jclass Value = jenv->FindClass("org/sigrok/core/classes/" #ClassName);
  jmethodID Value_init = jenv->GetMethodID(Value, "<init>", "(JZ)V");
  $result = jenv->NewObject(HashMap, HashMap_init);
  jlong value;
  for (auto entry : $1)
  {
    *(std::shared_ptr< sigrok::ClassName > **)&value =
      new std::shared_ptr< sigrok::ClassName>(entry.second);
    jenv->CallObjectMethod($result, HashMap_put,
      jenv->NewStringUTF(entry.first.c_str()),
      jenv->NewObject(Value, Value_init, value, true));
  }
}

%enddef

STRING_TO_SHARED_PTR_MAP(Driver)
STRING_TO_SHARED_PTR_MAP(InputFormat)
STRING_TO_SHARED_PTR_MAP(OutputFormat)

/* Specialisation for ConfigKey->Variant maps */

MAP_COMMON(const sigrok::ConfigKey *, Glib::VariantBase, ConfigKey, Variant)

%typemap(jni) std::map<const sigrok::ConfigKey, Glib::VariantBase> "jobject"
%typemap(jtype) std::map<const sigrok::ConfigKey, Glib::VariantBase>
  "java.util.Map<ConfigKey,Variant>"

%typemap(out) std::map<const sigrok::ConfigKey *, Glib::VariantBase> {
  jclass HashMap = jenv->FindClass("java/util/HashMap");
  jmethodID HashMap_init = jenv->GetMethodID(HashMap, "<init>", "()V");
  jmethodID HashMap_put = jenv->GetMethodID(HashMap, "put",
    "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  jclass ConfigKey = jenv->FindClass("org/sigrok/core/classes/ConfigKey");
  jmethodID ConfigKey_init = jenv->GetMethodID(ConfigKey, "<init>", "(JZ)V");
  jclass Variant = jenv->FindClass("org/sigrok/core/classes/Variant");
  jmethodID Variant_init = jenv->GetMethodID(Variant, "<init>", "(JZ)V");
  $result = jenv->NewObject(HashMap, HashMap_init);
  jlong key;
  jlong value;
  for (auto entry : $1)
  {
    *(const sigrok::ConfigKey **) &key = entry.first;
    *(Glib::VariantBase **) &value = new Glib::VariantBase(entry.second);
    jenv->CallObjectMethod($result, HashMap_put,
      jenv->NewObject(ConfigKey, ConfigKey_init, key, false));
      jenv->NewObject(Variant, Variant_init, value, true));
  }
}

/* Support Driver.scan() with no arguments. */
%extend sigrok::Driver {
  std::vector<std::shared_ptr<sigrok::HardwareDevice> > scan()
  {
    std::map<const sigrok::ConfigKey *, Glib::VariantBase> options;
    return $self->scan(options);
  }
}

/* Support InputFormat.create_input() with no options. */
%extend sigrok::InputFormat {
  std::shared_ptr<sigrok::Input> create_input()
  {
    std::map<std::string, Glib::VariantBase> options;
    return $self->create_input(options);
  }
}

/* Support OutputFormat.create_output(device) with no options. */
%extend sigrok::OutputFormat {
  std::shared_ptr<sigrok::Output> create_output(
    std::shared_ptr<sigrok::Device> device)
  {
    std::map<std::string, Glib::VariantBase> options;
    return $self->create_output(device, options);
  }
}

/* Pass JNIEnv parameter to C++ extension methods requiring it. */

%typemap(in, numinputs=0) JNIEnv * %{
   $1 = jenv;
%} 

/* Support Java log callbacks. */

%typemap(javaimports) sigrok::Context
  "import org.sigrok.core.interfaces.LogCallback;"

%inline {
typedef jobject jlogcallback;
}

%typemap(jni) jlogcallback "jlogcallback"
%typemap(jtype) jlogcallback "LogCallback"
%typemap(jstype) jlogcallback "LogCallback"
%typemap(javain) jlogcallback "$javainput"

%extend sigrok::Context
{
  void add_log_callback(JNIEnv *env, jlogcallback obj)
  {
    jclass obj_class = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(obj_class, "run",
      "(Lorg/sigrok/core/classes/LogLevel;Ljava/lang/String;)V");
    jclass LogLevel = (jclass) env->NewGlobalRef(
        env->FindClass("org/sigrok/core/classes/LogLevel"));
    jmethodID LogLevel_init = env->GetMethodID(LogLevel, "<init>", "(JZ)V");
    jobject obj_ref = env->NewGlobalRef(obj);

    $self->set_log_callback([=] (
      const sigrok::LogLevel *loglevel,
      std::string message)
    {
      jlong loglevel_addr;
      *(const sigrok::LogLevel **) &loglevel_addr = loglevel;
      jobject loglevel_obj = env->NewObject(
        LogLevel, LogLevel_init, loglevel_addr, false);
      jobject message_obj = env->NewStringUTF(message.c_str());
      env->CallVoidMethod(obj_ref, method, loglevel_obj, message_obj);
      if (env->ExceptionCheck())
        throw sigrok::Error(SR_ERR);
    });
  }
}

/* Support Java datafeed callbacks. */

%typemap(javaimports) sigrok::Session
  "import org.sigrok.core.interfaces.DatafeedCallback;"

%inline {
typedef jobject jdatafeedcallback;
}

%typemap(jni) jdatafeedcallback "jdatafeedcallback"
%typemap(jtype) jdatafeedcallback "DatafeedCallback"
%typemap(jstype) jdatafeedcallback "DatafeedCallback"
%typemap(javain) jdatafeedcallback "$javainput"

%extend sigrok::Session
{
  void add_datafeed_callback(JNIEnv *env, jdatafeedcallback obj)
  {
    jclass obj_class = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(obj_class, "run",
      "(Lorg/sigrok/core/classes/Device;Lorg/sigrok/core/classes/Packet;)V");
    jclass Device = (jclass) env->NewGlobalRef(
        env->FindClass("org/sigrok/core/classes/Device"));
    jmethodID Device_init = env->GetMethodID(Device, "<init>", "(JZ)V");
    jclass Packet = (jclass) env->NewGlobalRef(
       env->FindClass("org/sigrok/core/classes/Packet"));
    jmethodID Packet_init = env->GetMethodID(Packet, "<init>", "(JZ)V");
    jobject obj_ref = env->NewGlobalRef(obj);

    $self->add_datafeed_callback([=] (
      std::shared_ptr<sigrok::Device> device,
      std::shared_ptr<sigrok::Packet> packet)
    {
      jlong device_addr;
      jlong packet_addr;
      *(std::shared_ptr<sigrok::Device> **) &device_addr =
        new std::shared_ptr<sigrok::Device>(device);
      *(std::shared_ptr<sigrok::Packet> **) &packet_addr =
        new std::shared_ptr<sigrok::Packet>(packet);
      jobject device_obj = env->NewObject(
        Device, Device_init, device_addr, true);
      jobject packet_obj = env->NewObject(
        Packet, Packet_init, packet_addr, true);
      env->CallVoidMethod(obj_ref, method, device_obj, packet_obj);
      if (env->ExceptionCheck())
        throw sigrok::Error(SR_ERR);
    });
  }
}

/* Support Java event source callbacks. */

%typemap(javaimports) sigrok::EventSource
  "import org.sigrok.core.interfaces.SourceCallback;"

%inline {
typedef jobject jsourcecallback;
}

%typemap(jni) jsourcecallback "jsourcecallback"
%typemap(jtype) jsourcecallback "SourceCallback"
%typemap(jstype) jsourcecallback "SourceCallback"
%typemap(javain) jsourcecallback "$javainput"

%extend sigrok::EventSource
{
  std::shared_ptr<sigrok::EventSource> create(
    int fd, Glib::IOCondition events, int timeout,
    JNIEnv *env, jsourcecallback obj)
  {
    (void) $self;
    jclass obj_class = env->GetObjectClass(obj);
    jmethodID method = env->GetMethodID(obj_class, "run", "(I)V");
    jobject obj_ref = env->NewGlobalRef(obj);

    return sigrok::EventSource::create(fd, events, timeout, [=] (int revents)
    {
      bool result = env->CallBooleanMethod(obj_ref, method, revents);
      if (env->ExceptionCheck())
        throw sigrok::Error(SR_ERR);
      return result;
    });
  }
}

/* Currently broken due to some std::map typemap issues. */
%ignore sigrok::Meta::get_config;

%include "doc.i"
%include "bindings/swig/classes.i"
