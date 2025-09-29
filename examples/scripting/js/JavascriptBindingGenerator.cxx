#include "JavascriptBindingGenerator.h"

TypeConverterRegistry &TypeConverterRegistry::instance() {
    static TypeConverterRegistry registry;
    return registry;
}

void TypeConverterRegistry::register_converter(const std::string &type_name,
                                               CppToJsConverter to_js,
                                               JsToCppConverter to_cpp) {
    cpp_to_js_converters[type_name] = to_js;
    js_to_cpp_converters[type_name] = to_cpp;
}

Napi::Value
TypeConverterRegistry::convert_to_js(Napi::Env env, const std::any &value,
                                     const std::string &type_name) const {
    if (!value.has_value() || type_name == "void") {
        return env.Undefined();
    }

    auto it = cpp_to_js_converters.find(type_name);
    if (it != cpp_to_js_converters.end()) {
        return it->second(env, value);
    }

    try {
        if (type_name == "string") {
            return Napi::String::New(env, std::any_cast<std::string>(value));
        } else if (type_name == "int") {
            return Napi::Number::New(env, std::any_cast<int>(value));
        } else if (type_name == "double") {
            return Napi::Number::New(env, std::any_cast<double>(value));
        } else if (type_name == "float") {
            return Napi::Number::New(env, std::any_cast<float>(value));
        } else if (type_name == "bool") {
            return Napi::Boolean::New(env, std::any_cast<bool>(value));
        }
    } catch (const std::bad_any_cast &) {
        // Fall through
    }

    return env.Undefined();
}

std::any
TypeConverterRegistry::convert_to_cpp(const Napi::Value &js_value,
                                      const std::string &type_name) const {
    auto it = js_to_cpp_converters.find(type_name);
    if (it != js_to_cpp_converters.end()) {
        return it->second(js_value);
    }

    if (type_name == "string") {
        return std::make_any<std::string>(
            js_value.IsString() ? js_value.As<Napi::String>().Utf8Value()
                                : js_value.ToString().Utf8Value());
    } else if (type_name == "int") {
        return std::make_any<int>(js_value.As<Napi::Number>().Int32Value());
    } else if (type_name == "double") {
        return std::make_any<double>(js_value.As<Napi::Number>().DoubleValue());
    } else if (type_name == "float") {
        return std::make_any<float>(
            static_cast<float>(js_value.As<Napi::Number>().DoubleValue()));
    } else if (type_name == "bool") {
        return std::make_any<bool>(js_value.As<Napi::Boolean>().Value());
    }

    throw std::runtime_error("Unsupported type: " + type_name);
}

TypeConverterRegistry::TypeConverterRegistry() {
    // Register vector converters
    register_converter(
        "vector<int>",
        [](Napi::Env env, const std::any &value) -> Napi::Value {
            auto vec = std::any_cast<std::vector<int>>(value);
            auto arr = Napi::Array::New(env, vec.size());
            for (size_t i = 0; i < vec.size(); ++i) {
                arr.Set(i, vec[i]);
            }
            return arr;
        },
        [](const Napi::Value &js_val) -> std::any {
            auto arr = js_val.As<Napi::Array>();
            std::vector<int> vec;
            for (uint32_t i = 0; i < arr.Length(); ++i) {
                vec.push_back(arr.Get(i).As<Napi::Number>().Int32Value());
            }
            return vec;
        });

    register_converter(
        "vector<double>",
        [](Napi::Env env, const std::any &value) -> Napi::Value {
            auto vec = std::any_cast<std::vector<double>>(value);
            auto arr = Napi::Array::New(env, vec.size());
            for (size_t i = 0; i < vec.size(); ++i) {
                arr.Set(i, vec[i]);
            }
            return arr;
        },
        [](const Napi::Value &js_val) -> std::any {
            auto arr = js_val.As<Napi::Array>();
            std::vector<double> vec;
            for (uint32_t i = 0; i < arr.Length(); ++i) {
                vec.push_back(arr.Get(i).As<Napi::Number>().DoubleValue());
            }
            return vec;
        });

    register_converter(
        "vector<string>",
        [](Napi::Env env, const std::any &value) -> Napi::Value {
            auto vec = std::any_cast<std::vector<std::string>>(value);
            auto arr = Napi::Array::New(env, vec.size());
            for (size_t i = 0; i < vec.size(); ++i) {
                arr.Set(i, vec[i]);
            }
            return arr;
        },
        [](const Napi::Value &js_val) -> std::any {
            auto arr = js_val.As<Napi::Array>();
            std::vector<std::string> vec;
            for (uint32_t i = 0; i < arr.Length(); ++i) {
                vec.push_back(arr.Get(i).As<Napi::String>().Utf8Value());
            }
            return vec;
        });
}

// ============================================================

JavascriptBindingGenerator::JavascriptBindingGenerator(Napi::Env env,
                                                       Napi::Object exports)
    : env(env), exports(exports) {}

void JavascriptBindingGenerator::add_utilities() {
    exports.Set(
        "getAllClasses",
        Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
            auto arr = Napi::Array::New(env);
            uint32_t idx = 0;
            for (const auto &name : bound_classes) {
                arr.Set(idx++, name);
            }
            return arr;
        }));
}

void JavascriptBindingGenerator::register_type_converter(
    const std::string &type_name,
    typename TypeConverterRegistry::CppToJsConverter to_js,
    typename TypeConverterRegistry::JsToCppConverter to_cpp) {
    TypeConverterRegistry::instance().register_converter(type_name, to_js,
                                                         to_cpp);
}
