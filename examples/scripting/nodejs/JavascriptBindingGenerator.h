#pragma once
#include <introspection/introspectable.h>
#include <memory>
#include <napi.h>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>

/**
 * @brief Automatic Node.js N-API binding generator for introspectable classes.
 * Binded types are double, int, float, string, bool, vector<int>,
 * vector<double>, vector<string>
 * @example
 * ```cpp
 * // Usage example:
 * Napi::Object Init(Napi::Env env, Napi::Object exports) {
 *     AutoJSBindingGenerator generator(env, exports);
 *
 *     // Automatically bind introspectable classes
 *     generator.bind_class<Person>();
 *     generator.bind_class<GameObject>("GameObj");  // Custom name
 *
 *     // Or bind multiple at once
 *     generator.bind_classes<Person, GameObject, Vehicle>();
 *
 *     // Add utility functions
 *     generator.add_utilities();
 *
 *     return exports;
 * }
 *
 * NODE_API_MODULE(introspection_demo, Init)
 * ```
 *
 * @example
 * ```javascript
 * const introspection = require('./build/Release/introspection_demo');
 *
 * // Create objects using constructors
 * const person = new introspection.Person("Alice", 30, 1.65);
 * const vehicle = new introspection.Vehicle("Honda", "Civic", 2022);
 *
 * // Access properties directly
 * console.log(person.name);    // "Alice"
 * console.log(person.age);     // 30
 * person.name = "Bob";
 * person.age = 25;
 *
 * // Call methods
 * person.introduce();
 * person.celebrateBirthday();
 *
 * vehicle.start();
 * vehicle.drive(100.5);
 * console.log(vehicle.getInfo());
 *
 * // Use introspection utilities
 * console.log(person.getClassName());    // "Person"
 * console.log(person.getMemberNames());  // ["name", "age", "height",
 * "isActive"] console.log(person.getMethodNames());  // ["introduce",
 * "celebrateBirthday", ...]
 *
 * // Dynamic access
 * console.log(person.getMemberValue("age"));
 * person.setMemberValue("height", 1.80);
 * person.callMethod("introduce", []);
 *
 * // JSON export
 * console.log(person.toJSON());
 *
 * // Module utilities
 * console.log(introspection.getAllClasses());
 * const defaultPerson = introspection.createPerson();
 * ```
 */
class JavascriptBindingGenerator {
  public:
    // Type converter function signatures
    using CppToJsConverter =
        std::function<Napi::Value(Napi::Env, const std::any &)>;
    using JsToCppConverter = std::function<std::any(const Napi::Value &)>;

    JavascriptBindingGenerator(Napi::Env env, Napi::Object exports)
        : env(env), exports(exports) {
        register_builtin_converters();
    }

    /**
     * @brief Register a custom type converter at runtime
     */
    void register_type_converter(const std::string &type_name,
                                 CppToJsConverter to_js,
                                 JsToCppConverter to_cpp) {
        cpp_to_js_converters[type_name] = to_js;
        js_to_cpp_converters[type_name] = to_cpp;
    }
    /**
     * @brief Automatically bind an introspectable class to JavaScript
     * @tparam T The introspectable class type
     * @param class_name Optional custom class name (uses introspection name if
     * empty)
     * @return JavaScript constructor function
     */
    template <typename T>
    Napi::Function bind_class(const std::string &class_name = "") {
        static_assert(std::is_base_of_v<Introspectable, T>,
                      "Type must inherit from Introspectable");

        // Get type info from introspection system (use static method to avoid
        // copies)
        const auto &type_info = T::getStaticTypeInfo();

        std::string final_class_name =
            class_name.empty() ? type_info.class_name : class_name;

        // Prevent duplicate bindings
        if (bound_classes.find(final_class_name) != bound_classes.end()) {
            throw std::runtime_error("Class '" + final_class_name +
                                     "' already bound");
        }
        bound_classes.insert(final_class_name);

        // Create JavaScript class using N-API
        auto js_class = create_js_class<T>(final_class_name, type_info);

        // Register constructor in exports
        exports.Set(final_class_name, js_class);
        constructors[final_class_name] = js_class;

        return js_class;
    }

    /**
     * @brief Bind multiple classes at once
     */
    template <typename... Classes> void bind_classes() {
        (bind_class<Classes>(), ...);
    }

    /**
     * @brief Add utility functions to the module
     */
    void add_utilities() {
        // Add function to get all available classes
        exports.Set(
            "getAllClasses",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                Napi::Array classes = Napi::Array::New(env);
                uint32_t index = 0;
                for (const auto &class_name : bound_classes) {
                    classes.Set(index++, Napi::String::New(env, class_name));
                }
                return classes;
            }));

        // Add factory functions for default construction
        for (const auto &[class_name, constructor] : constructors) {
            std::string factory_name = "create" + class_name;
            exports.Set(factory_name,
                        Napi::Function::New(
                            env, [constructor](const Napi::CallbackInfo &info) {
                                return constructor.New({});
                            }));
        }
    }

  private:
    Napi::Env env;
    Napi::Object exports;
    std::unordered_set<std::string> bound_classes;
    std::unordered_map<std::string, Napi::Function> constructors;
    std::unordered_map<std::string, CppToJsConverter> cpp_to_js_converters;
    std::unordered_map<std::string, JsToCppConverter> js_to_cpp_converters;

    void register_builtin_converters() {
        // Array converters
        register_type_converter(
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

        // Add more array types as needed
        register_type_converter(
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

        register_type_converter(
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

    template <typename T>
    Napi::Function create_js_class(const std::string &class_name,
                                   const TypeInfo &type_info) {
        // // Define the JavaScript class
        // auto js_class = DefineClass(
        //     env, class_name.c_str(),
        //     {
        //         // Constructor
        //         InstanceMethod<&JSWrapper<T>::constructor_wrapper>(
        //             "constructor"),

        //         // Create instance methods and properties dynamically
        //     });

        // // We need to add properties and methods dynamically after class
        // // creation This is a limitation of N-API - we'll use a wrapper
        // approach return Napi::Function::New(
        //     env, [this, type_info](const Napi::CallbackInfo &info) {
        //         return create_instance<T>(info, type_info);
        //     });

        // Create a JavaScript constructor function that returns our custom
        // object
        // This approach works better with dynamic introspection
        return Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
            // Get type_info inside the lambda from the static method
            const auto &type_info = T::getStaticTypeInfo();
            return create_instance<T>(info, type_info);
        });
    }

    template <typename T>
    Napi::Object create_instance(const Napi::CallbackInfo &info,
                                 const TypeInfo &type_info) {
        // Create the C++ object
        std::shared_ptr<T> cpp_obj;

        if (info.Length() == 0) {
            cpp_obj = std::make_shared<T>();
        } else {
            // For now, support default constructor only
            // Advanced constructor support would require extending
            // introspection
            cpp_obj = std::make_shared<T>();
        }

        // Create JavaScript object
        Napi::Object js_obj = Napi::Object::New(env);

        // Store C++ object reference
        js_obj.Set("__cpp_object", Napi::External<T>::New(env, cpp_obj.get()));
        js_obj.Set("__cpp_shared_ptr",
                   Napi::External<std::shared_ptr<T>>::New(env, &cpp_obj));

        // Add properties for all members
        bind_properties<T>(js_obj, type_info);

        // Add methods
        bind_methods<T>(js_obj, type_info);

        // Add introspection utilities
        bind_introspection_utilities<T>(js_obj);

        return js_obj;
    }

    template <typename T>
    void bind_properties(Napi::Object &js_obj, const TypeInfo &type_info) {
        for (const auto &member_name : type_info.getMemberNames()) {
            const auto *member = type_info.getMember(member_name);
            if (!member)
                continue;

            // Create getter/setter methods with capitalized names
            std::string getter_name = "get" + capitalize(member_name);
            std::string setter_name = "set" + capitalize(member_name);

            js_obj.Set(
                getter_name,
                Napi::Function::New(
                    env, [this, member_name](const Napi::CallbackInfo &info) {
                        auto cpp_obj =
                            get_cpp_object<T>(info.This().As<Napi::Object>());
                        auto value = cpp_obj->getMemberValue(member_name);
                        const auto *member_info =
                            cpp_obj->getTypeInfo().getMember(member_name);
                        return convert_any_to_js(info.Env(), value,
                                                 member_info->type_name);
                    }));

            js_obj.Set(
                setter_name,
                Napi::Function::New(env, [this, member_name](
                                             const Napi::CallbackInfo &info) {
                    if (info.Length() < 1) {
                        Napi::TypeError::New(info.Env(), "Expected 1 argument")
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                    }
                    auto cpp_obj =
                        get_cpp_object<T>(info.This().As<Napi::Object>());
                    const auto *member_info =
                        cpp_obj->getTypeInfo().getMember(member_name);
                    auto cpp_value =
                        convert_js_to_any(info[0], member_info->type_name);
                    cpp_obj->setMemberValue(member_name, cpp_value);
                    return info.Env().Undefined();
                }));

            // Create direct property access using Object.defineProperty
            create_property_accessor<T>(js_obj, member_name);
        }
    }

    template <typename T>
    void create_property_accessor(Napi::Object &js_obj,
                                  const std::string &member_name) {
        // Use Object.defineProperty to create natural property access
        Napi::Object object_constructor =
            env.Global().Get("Object").As<Napi::Object>();
        Napi::Function define_property =
            object_constructor.Get("defineProperty").As<Napi::Function>();

        Napi::Object descriptor = Napi::Object::New(env);
        descriptor.Set("enumerable", true);
        descriptor.Set("configurable", true);

        // Getter function - capture member_name by value
        auto getter = Napi::Function::New(
            env,
            [this, member_name](const Napi::CallbackInfo &info) -> Napi::Value {
                try {
                    auto cpp_obj =
                        get_cpp_object<T>(info.This().As<Napi::Object>());
                    auto value = cpp_obj->getMemberValue(member_name);
                    const auto *member_info =
                        cpp_obj->getTypeInfo().getMember(member_name);
                    return convert_any_to_js(info.Env(), value,
                                             member_info->type_name);
                } catch (const std::exception &e) {
                    Napi::Error::New(info.Env(), "Failed to get '" +
                                                     member_name +
                                                     "': " + e.what())
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
            });

        // Setter function - capture member_name by value
        auto setter = Napi::Function::New(
            env,
            [this, member_name](const Napi::CallbackInfo &info) -> Napi::Value {
                try {
                    if (info.Length() < 1) {
                        Napi::TypeError::New(info.Env(), "Expected 1 argument")
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                    }

                    auto cpp_obj =
                        get_cpp_object<T>(info.This().As<Napi::Object>());
                    const auto *member_info =
                        cpp_obj->getTypeInfo().getMember(member_name);
                    auto cpp_value =
                        convert_js_to_any(info[0], member_info->type_name);
                    cpp_obj->setMemberValue(member_name, cpp_value);
                    return info.Env().Undefined();
                } catch (const std::exception &e) {
                    Napi::Error::New(info.Env(), "Failed to set '" +
                                                     member_name +
                                                     "': " + e.what())
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
            });

        descriptor.Set("get", getter);
        descriptor.Set("set", setter);

        define_property.Call(
            {js_obj, Napi::String::New(env, member_name), descriptor});
    }

    template <typename T>
    void bind_methods(Napi::Object &js_obj, const TypeInfo &type_info) {
        for (const auto &method_name : type_info.getMethodNames()) {
            const auto *method = type_info.getMethod(method_name);
            if (!method)
                continue;

            // Skip getter/setter methods that are already bound as properties
            if (is_getter_setter_method(method_name))
                continue;

            js_obj.Set(
                method_name,
                Napi::Function::New(env, [&](const Napi::CallbackInfo &info) {
                    auto cpp_obj =
                        get_cpp_object<T>(info.This().As<Napi::Object>());

                    try {
                        // Convert JavaScript arguments to std::any vector
                        std::vector<std::any> cpp_args;
                        const auto *method_info =
                            cpp_obj->getTypeInfo().getMethod(method_name);

                        if (info.Length() !=
                            method_info->parameter_types.size()) {
                            std::string error =
                                "Method '" + method_name + "' expects " +
                                std::to_string(
                                    method_info->parameter_types.size()) +
                                " arguments, got " +
                                std::to_string(info.Length());
                            Napi::TypeError::New(info.Env(), error)
                                .ThrowAsJavaScriptException();
                            return info.Env().Undefined();
                        }

                        for (size_t i = 0; i < info.Length(); ++i) {
                            cpp_args.push_back(convert_js_to_any(
                                info[i], method_info->parameter_types[i]));
                        }

                        // Call method through introspection
                        auto result =
                            cpp_obj->callMethod(method_name, cpp_args);

                        // Convert result back to JavaScript
                        return convert_any_to_js(info.Env(), result,
                                                 method_info->return_type);

                    } catch (const std::exception &e) {
                        Napi::Error::New(info.Env(), "Failed to call method '" +
                                                         method_name +
                                                         "': " + e.what())
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                    }
                }));
        }
    }

    template <typename T>
    void bind_introspection_utilities(Napi::Object &js_obj) {
        // Expose introspection utilities to JavaScript
        js_obj.Set("getClassName",
                   Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                       auto cpp_obj =
                           get_cpp_object<T>(info.This().As<Napi::Object>());
                       return Napi::String::New(info.Env(),
                                                cpp_obj->getClassName());
                   }));

        js_obj.Set("getMemberNames",
                   Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                       auto cpp_obj =
                           get_cpp_object<T>(info.This().As<Napi::Object>());
                       auto names = cpp_obj->getMemberNames();
                       Napi::Array js_array = Napi::Array::New(info.Env());
                       for (size_t i = 0; i < names.size(); ++i) {
                           js_array.Set(i, names[i]);
                       }
                       return js_array;
                   }));

        js_obj.Set("getMethodNames",
                   Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                       auto cpp_obj =
                           get_cpp_object<T>(info.This().As<Napi::Object>());
                       auto names = cpp_obj->getMethodNames();
                       Napi::Array js_array = Napi::Array::New(info.Env());
                       for (size_t i = 0; i < names.size(); ++i) {
                           js_array.Set(i, names[i]);
                       }
                       return js_array;
                   }));

        js_obj.Set(
            "hasMember",
            Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                if (info.Length() < 1 || !info[0].IsString()) {
                    return Napi::Boolean::New(info.Env(), false);
                }
                auto cpp_obj =
                    get_cpp_object<T>(info.This().As<Napi::Object>());
                std::string name = info[0].As<Napi::String>().Utf8Value();
                return Napi::Boolean::New(info.Env(), cpp_obj->hasMember(name));
            }));

        js_obj.Set(
            "hasMethod",
            Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                if (info.Length() < 1 || !info[0].IsString()) {
                    return Napi::Boolean::New(info.Env(), false);
                }
                auto cpp_obj =
                    get_cpp_object<T>(info.This().As<Napi::Object>());
                std::string name = info[0].As<Napi::String>().Utf8Value();
                return Napi::Boolean::New(info.Env(), cpp_obj->hasMethod(name));
            }));

        js_obj.Set("toJSON",
                   Napi::Function::New(env, [](const Napi::CallbackInfo &info) {
                       auto cpp_obj =
                           get_cpp_object<T>(info.This().As<Napi::Object>());
                       return Napi::String::New(info.Env(), cpp_obj->toJSON());
                   }));

        // Dynamic member/method access
        js_obj.Set(
            "getMemberValue",
            Napi::Function::New(env, [&](const Napi::CallbackInfo &info) {
                if (info.Length() < 1 || !info[0].IsString()) {
                    Napi::TypeError::New(info.Env(), "Expected string argument")
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
                auto cpp_obj =
                    get_cpp_object<T>(info.This().As<Napi::Object>());
                std::string name = info[0].As<Napi::String>().Utf8Value();
                auto value = cpp_obj->getMemberValue(name);
                const auto *member = cpp_obj->getTypeInfo().getMember(name);
                return convert_any_to_js(
                    info.Env(), value, member ? member->type_name : "unknown");
            }));

        js_obj.Set(
            "setMemberValue",
            Napi::Function::New(env, [&](const Napi::CallbackInfo &info) {
                if (info.Length() < 2 || !info[0].IsString()) {
                    Napi::TypeError::New(info.Env(),
                                         "Expected (string, value) arguments")
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
                auto cpp_obj =
                    get_cpp_object<T>(info.This().As<Napi::Object>());
                std::string name = info[0].As<Napi::String>().Utf8Value();
                const auto *member = cpp_obj->getTypeInfo().getMember(name);
                if (!member) {
                    Napi::Error::New(info.Env(), "Member not found: " + name)
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
                auto cpp_value = convert_js_to_any(info[1], member->type_name);
                cpp_obj->setMemberValue(name, cpp_value);
                return info.Env().Undefined();
            }));

        js_obj.Set(
            "callMethod",
            Napi::Function::New(env, [&](const Napi::CallbackInfo &info) {
                if (info.Length() < 1 || !info[0].IsString()) {
                    Napi::TypeError::New(info.Env(), "Expected method name")
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }

                auto cpp_obj =
                    get_cpp_object<T>(info.This().As<Napi::Object>());
                std::string name = info[0].As<Napi::String>().Utf8Value();
                const auto *method = cpp_obj->getTypeInfo().getMethod(name);
                if (!method) {
                    Napi::Error::New(info.Env(), "Method not found: " + name)
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }

                std::vector<std::any> cpp_args;
                if (info.Length() > 1 && info[1].IsArray()) {
                    Napi::Array args_array = info[1].As<Napi::Array>();
                    for (uint32_t i = 0; i < args_array.Length(); ++i) {
                        if (i < method->parameter_types.size()) {
                            cpp_args.push_back(convert_js_to_any(
                                args_array.Get(i), method->parameter_types[i]));
                        }
                    }
                }

                auto result = cpp_obj->callMethod(name, cpp_args);
                return convert_any_to_js(info.Env(), result,
                                         method->return_type);
            }));
    }

    // Helper functions
    template <typename T> static T *get_cpp_object(const Napi::Object &js_obj) {
        return js_obj.Get("__cpp_object").As<Napi::External<T>>().Data();
    }

    bool is_getter_setter_method(const std::string &method_name) const {
        return method_name.starts_with("get") ||
               method_name.starts_with("set") || method_name.starts_with("is");
    }

    std::string capitalize(const std::string &str) const {
        if (str.empty())
            return str;
        std::string result = str;
        result[0] = std::toupper(result[0]);
        return result;
    }

    // Convert std::any to JavaScript value based on type name
    Napi::Value convert_any_to_js(Napi::Env env, const std::any &value,
                                  const std::string &type_name) const {
        if (!value.has_value() || type_name == "void") {
            return env.Undefined();
        }

        // CHECK CUSTOM CONVERTERS FIRST
        auto it = cpp_to_js_converters.find(type_name);
        if (it != cpp_to_js_converters.end()) {
            try {
                return it->second(env, value);
            } catch (const std::exception &e) {
                Napi::TypeError::New(env, "Custom converter failed for '" +
                                              type_name + "': " + e.what())
                    .ThrowAsJavaScriptException();
                return env.Undefined();
            }
        }

        // FALLBACK TO BUILT-IN TYPES
        try {
            if (type_name == "string") {
                return Napi::String::New(env,
                                         std::any_cast<std::string>(value));
            } else if (type_name == "int") {
                return Napi::Number::New(env, std::any_cast<int>(value));
            } else if (type_name == "double") {
                return Napi::Number::New(env, std::any_cast<double>(value));
            } else if (type_name == "float") {
                return Napi::Number::New(env, std::any_cast<float>(value));
            } else if (type_name == "bool") {
                return Napi::Boolean::New(env, std::any_cast<bool>(value));
            } else {
                return env.Undefined();
            }
        } catch (const std::bad_any_cast &e) {
            Napi::TypeError::New(env, "Failed to convert type '" + type_name +
                                          "' to JavaScript: " + e.what())
                .ThrowAsJavaScriptException();
            return env.Undefined();
        }
    }

    // Convert JavaScript value to std::any based on expected type
    std::any convert_js_to_any(const Napi::Value &js_value,
                               const std::string &type_name) const {
        // CHECK CUSTOM CONVERTERS FIRST
        auto it = js_to_cpp_converters.find(type_name);
        if (it != js_to_cpp_converters.end()) {
            try {
                return it->second(js_value);
            } catch (const std::exception &e) {
                throw std::runtime_error("Custom converter failed for '" +
                                         type_name + "': " + e.what());
            }
        }

        // FALLBACK TO BUILT-IN TYPES
        try {
            if (type_name == "string") {
                if (js_value.IsString()) {
                    return std::make_any<std::string>(
                        js_value.As<Napi::String>().Utf8Value());
                } else {
                    return std::make_any<std::string>(
                        js_value.ToString().Utf8Value());
                }
            } else if (type_name == "int") {
                return std::make_any<int>(
                    js_value.As<Napi::Number>().Int32Value());
            } else if (type_name == "double") {
                return std::make_any<double>(
                    js_value.As<Napi::Number>().DoubleValue());
            } else if (type_name == "float") {
                return std::make_any<float>(static_cast<float>(
                    js_value.As<Napi::Number>().DoubleValue()));
            } else if (type_name == "bool") {
                return std::make_any<bool>(
                    js_value.As<Napi::Boolean>().Value());
            } else {
                throw std::runtime_error("Unsupported type conversion for: " +
                                         type_name);
            }
        } catch (const std::exception &e) {
            throw std::runtime_error("Failed to convert JavaScript value to '" +
                                     type_name + "': " + e.what());
        }
    }

    // // Wrapper class for handling JavaScript class instances
    // template <typename T> class JSWrapper {
    //   public:
    //     static Napi::Value constructor_wrapper(const Napi::CallbackInfo
    //     &info) {
    //         // This would be used if we were creating proper N-API classes
    //         // For now, we use the function-based approach above
    //         return info.Env().Undefined();
    //     }
    // };
};

// Convenience macro for auto-binding
#define NAPI_AUTO_BIND_CLASS(generator, ClassName)                             \
    generator.bind_class<ClassName>(#ClassName)