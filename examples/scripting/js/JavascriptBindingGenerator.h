#pragma once
#include <introspection/introspectable.h>
#include <memory>
#include <napi.h>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

/**
 * @brief Type converters registry - singleton pattern
 */
class TypeConverterRegistry {
  public:
    using CppToJsConverter =
        std::function<Napi::Value(Napi::Env, const std::any &)>;
    using JsToCppConverter = std::function<std::any(const Napi::Value &)>;

    static TypeConverterRegistry &instance();

    void register_converter(const std::string &, CppToJsConverter,
                            JsToCppConverter);
    Napi::Value convert_to_js(Napi::Env, const std::any &,
                              const std::string &) const;
    std::any convert_to_cpp(const Napi::Value &, const std::string &) const;

  private:
    TypeConverterRegistry();
    std::unordered_map<std::string, CppToJsConverter> cpp_to_js_converters;
    std::unordered_map<std::string, JsToCppConverter> js_to_cpp_converters;
};

/**
 * @brief Simple wrapper using Napi::ObjectWrap properly
 */
template <typename T>
class ObjectWrapper : public Napi::ObjectWrap<ObjectWrapper<T>> {
  public:
    static Napi::FunctionReference constructor;
    static Napi::Object Init(Napi::Env, Napi::Object, const std::string &);

    ObjectWrapper(const Napi::CallbackInfo &info);

    T *GetCppObject();

  private:
    std::shared_ptr<T> cpp_obj;

    void SetupBindings();
    void SetupProperty(const std::string &prop_name);
    void SetupMethod(const std::string &method_name);
    void SetupIntrospection();
    static bool IsSimpleGetterSetter(const std::string &, const TypeInfo &);
    static std::string Capitalize(const std::string &str);
};

/**
 * @brief Binding generator
 */
class JavascriptBindingGenerator {
  public:
    JavascriptBindingGenerator(Napi::Env env, Napi::Object exports);

    template <typename T> void bind_class(const std::string &class_name = "");

    template <typename... Classes> void bind_classes() {
        (bind_class<Classes>(), ...);
    }

    void add_utilities();
    void register_type_converter(
        const std::string &type_name,
        typename TypeConverterRegistry::CppToJsConverter to_js,
        typename TypeConverterRegistry::JsToCppConverter to_cpp);

  private:
    Napi::Env env;
    Napi::Object exports;
    std::unordered_set<std::string> bound_classes;
};

#define NAPI_AUTO_BIND_CLASS(generator, ClassName)                             \
    generator.bind_class<ClassName>(#ClassName)

#include "JavascriptBindingGenerator.hxx"