
template <typename T>
inline Napi::Object ObjectWrapper<T>::Init(Napi::Env env, Napi::Object exports,
                                           const std::string &class_name) {
    Napi::Function func =
        ObjectWrapper::DefineClass(env, class_name.c_str(),
                                   {
                                       // These are just placeholders - real
                                       // methods are added in constructor
                                   });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    exports.Set(class_name, func);
    return exports;
}

template <typename T>
inline ObjectWrapper<T>::ObjectWrapper(const Napi::CallbackInfo &info)
    : Napi::ObjectWrap<ObjectWrapper<T>>(info) {

    // Create the C++ object
    cpp_obj = std::make_shared<T>();

    // Setup all bindings
    SetupBindings();
}

template <typename T> inline T *ObjectWrapper<T>::GetCppObject() {
    return cpp_obj.get();
}

template <typename T> inline void ObjectWrapper<T>::SetupBindings() {
    auto env = this->Env();
    auto obj = this->Value();
    const auto &type_info = cpp_obj->getTypeInfo();

    // Bind properties
    for (const auto &member_name : type_info.getMemberNames()) {
        SetupProperty(member_name);
    }

    // Bind methods
    for (const auto &method_name : type_info.getMethodNames()) {
        // Skip ONLY if it's a simple getter/setter for an existing member
        if (!IsSimpleGetterSetter(method_name, type_info)) {
            SetupMethod(method_name);
        }
    }

    // Bind introspection
    SetupIntrospection();
}

template <typename T>
inline void ObjectWrapper<T>::SetupProperty(const std::string &prop_name) {
    auto env = this->Env();
    auto obj = this->Value();

    // Get Object.defineProperty
    auto global = env.Global();
    auto object_ctor = global.Get("Object").template As<Napi::Object>();
    auto define_prop =
        object_ctor.Get("defineProperty").template As<Napi::Function>();

    // Create descriptor
    auto descriptor = Napi::Object::New(env);
    descriptor.Set("enumerable", true);
    descriptor.Set("configurable", true);

    // Getter
    descriptor.Set(
        "get",
        Napi::Function::New(
            env,
            [this, prop_name](const Napi::CallbackInfo &info) -> Napi::Value {
                try {
                    auto val = cpp_obj->getMemberValue(prop_name);
                    const auto *mem =
                        cpp_obj->getTypeInfo().getMember(prop_name);
                    return TypeConverterRegistry::instance().convert_to_js(
                        info.Env(), val, mem->type_name);
                } catch (const std::exception &e) {
                    Napi::Error::New(info.Env(), e.what())
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
            }));

    // Setter
    descriptor.Set(
        "set",
        Napi::Function::New(
            env,
            [this, prop_name](const Napi::CallbackInfo &info) -> Napi::Value {
                try {
                    if (info.Length() < 1) {
                        Napi::TypeError::New(info.Env(), "Expected 1 argument")
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                    }
                    const auto *mem =
                        cpp_obj->getTypeInfo().getMember(prop_name);
                    auto cpp_val =
                        TypeConverterRegistry::instance().convert_to_cpp(
                            info[0], mem->type_name);
                    cpp_obj->setMemberValue(prop_name, cpp_val);
                    return info.Env().Undefined();
                } catch (const std::exception &e) {
                    Napi::Error::New(info.Env(), e.what())
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
            }));

    // Define the property
    define_prop.Call({obj, Napi::String::New(env, prop_name), descriptor});

    // Also add explicit getter/setter methods
    std::string getter = "get" + Capitalize(prop_name);
    std::string setter = "set" + Capitalize(prop_name);

    obj.Set(getter,
            Napi::Function::New(env, [this, prop_name](
                                         const Napi::CallbackInfo &info) {
                auto val = cpp_obj->getMemberValue(prop_name);
                const auto *mem = cpp_obj->getTypeInfo().getMember(prop_name);
                return TypeConverterRegistry::instance().convert_to_js(
                    info.Env(), val, mem->type_name);
            }));

    obj.Set(
        setter, Napi::Function::New(env, [this, prop_name](
                                             const Napi::CallbackInfo &info) {
            if (info.Length() >= 1) {
                const auto *mem = cpp_obj->getTypeInfo().getMember(prop_name);
                auto cpp_val = TypeConverterRegistry::instance().convert_to_cpp(
                    info[0], mem->type_name);
                cpp_obj->setMemberValue(prop_name, cpp_val);
            }
            return info.Env().Undefined();
        }));
}

template <typename T>
inline void ObjectWrapper<T>::SetupMethod(const std::string &method_name) {
    auto env = this->Env();
    auto obj = this->Value();

    obj.Set(method_name,
            Napi::Function::New(env, [this, method_name](
                                         const Napi::CallbackInfo &info) {
                try {
                    const auto *meth =
                        cpp_obj->getTypeInfo().getMethod(method_name);

                    if (info.Length() != meth->parameter_types.size()) {
                        std::string err =
                            "Expected " +
                            std::to_string(meth->parameter_types.size()) +
                            " arguments, got " + std::to_string(info.Length());
                        Napi::TypeError::New(info.Env(), err)
                            .ThrowAsJavaScriptException();
                        return info.Env().Undefined();
                    }

                    std::vector<std::any> args;
                    for (size_t i = 0; i < info.Length(); ++i) {
                        args.push_back(
                            TypeConverterRegistry::instance().convert_to_cpp(
                                info[i], meth->parameter_types[i]));
                    }

                    auto result = cpp_obj->callMethod(method_name, args);
                    return TypeConverterRegistry::instance().convert_to_js(
                        info.Env(), result, meth->return_type);

                } catch (const std::exception &e) {
                    Napi::Error::New(info.Env(), e.what())
                        .ThrowAsJavaScriptException();
                    return info.Env().Undefined();
                }
            }));
}

template <typename T> inline void ObjectWrapper<T>::SetupIntrospection() {
    auto env = this->Env();
    auto obj = this->Value();

    obj.Set("getClassName",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                return Napi::String::New(info.Env(), cpp_obj->getClassName());
            }));

    obj.Set("getMemberNames",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                auto names = cpp_obj->getMemberNames();
                auto arr = Napi::Array::New(info.Env());
                for (size_t i = 0; i < names.size(); ++i) {
                    arr.Set(i, names[i]);
                }
                return arr;
            }));

    obj.Set("getMethodNames",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                auto names = cpp_obj->getMethodNames();
                auto arr = Napi::Array::New(info.Env());
                for (size_t i = 0; i < names.size(); ++i) {
                    arr.Set(i, names[i]);
                }
                return arr;
            }));

    obj.Set("hasMember",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                if (info.Length() > 0 && info[0].IsString()) {
                    std::string name =
                        info[0].template As<Napi::String>().Utf8Value();
                    return Napi::Boolean::New(info.Env(),
                                              cpp_obj->hasMember(name));
                }
                return Napi::Boolean::New(info.Env(), false);
            }));

    obj.Set("hasMethod",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                if (info.Length() > 0 && info[0].IsString()) {
                    std::string name =
                        info[0].template As<Napi::String>().Utf8Value();
                    return Napi::Boolean::New(info.Env(),
                                              cpp_obj->hasMethod(name));
                }
                return Napi::Boolean::New(info.Env(), false);
            }));

    obj.Set("toJSON",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                return Napi::String::New(info.Env(), cpp_obj->toJSON());
            }));

    obj.Set("getMemberValue",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                if (info.Length() > 0 && info[0].IsString()) {
                    std::string name =
                        info[0].template As<Napi::String>().Utf8Value();
                    auto val = cpp_obj->getMemberValue(name);
                    const auto *mem = cpp_obj->getTypeInfo().getMember(name);
                    return TypeConverterRegistry::instance().convert_to_js(
                        info.Env(), val, mem ? mem->type_name : "unknown");
                }
                return info.Env().Undefined();
            }));

    obj.Set("setMemberValue",
            Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
                if (info.Length() >= 2 && info[0].IsString()) {
                    std::string name =
                        info[0].template As<Napi::String>().Utf8Value();
                    const auto *mem = cpp_obj->getTypeInfo().getMember(name);
                    if (mem) {
                        auto cpp_val =
                            TypeConverterRegistry::instance().convert_to_cpp(
                                info[1], mem->type_name);
                        cpp_obj->setMemberValue(name, cpp_val);
                    }
                }
                return info.Env().Undefined();
            }));

    obj.Set(
        "callMethod",
        Napi::Function::New(env, [this](const Napi::CallbackInfo &info) {
            if (info.Length() > 0 && info[0].IsString()) {
                std::string name =
                    info[0].template As<Napi::String>().Utf8Value();
                const auto *meth = cpp_obj->getTypeInfo().getMethod(name);
                if (meth) {
                    std::vector<std::any> args;
                    if (info.Length() > 1 && info[1].IsArray()) {
                        auto arr = info[1].template As<Napi::Array>();
                        for (uint32_t i = 0; i < arr.Length() &&
                                             i < meth->parameter_types.size();
                             ++i) {
                            args.push_back(
                                TypeConverterRegistry::instance()
                                    .convert_to_cpp(arr.Get(i),
                                                    meth->parameter_types[i]));
                        }
                    }
                    auto result = cpp_obj->callMethod(name, args);
                    return TypeConverterRegistry::instance().convert_to_js(
                        info.Env(), result, meth->return_type);
                }
            }
            return info.Env().Undefined();
        }));
}

template <typename T>
inline bool
ObjectWrapper<T>::IsSimpleGetterSetter(const std::string &method_name,
                                       const TypeInfo &type_info) {
    // Check if it's a getter: getName, getAge, etc.
    if (method_name.starts_with("get") && method_name.length() > 3) {
        std::string potential_member = method_name.substr(3);
        // Make first char lowercase
        potential_member[0] = std::tolower(potential_member[0]);

        // Only skip if this member actually exists
        if (type_info.getMember(potential_member) != nullptr) {
            return true;
        }
    }

    // Check if it's a setter: setName, setAge, etc.
    if (method_name.starts_with("set") && method_name.length() > 3) {
        std::string potential_member = method_name.substr(3);
        // Make first char lowercase
        potential_member[0] = std::tolower(potential_member[0]);

        // Only skip if this member actually exists
        if (type_info.getMember(potential_member) != nullptr) {
            return true;
        }
    }

    // Check if it's an "is" getter: isActive, isRunning, etc.
    if (method_name.starts_with("is") && method_name.length() > 2) {
        std::string potential_member = method_name.substr(2);
        // Make first char lowercase
        potential_member[0] = std::tolower(potential_member[0]);

        // Only skip if this member actually exists
        if (type_info.getMember(potential_member) != nullptr) {
            return true;
        }
    }

    return false;
}

template <typename T>
inline std::string ObjectWrapper<T>::Capitalize(const std::string &str) {
    if (str.empty())
        return str;
    std::string result = str;
    result[0] = std::toupper(result[0]);
    return result;
}

// ================================================

// Static member definition
template <typename T> Napi::FunctionReference ObjectWrapper<T>::constructor;

// ================================================

template <typename T>
inline void
JavascriptBindingGenerator::bind_class(const std::string &class_name) {
    static_assert(std::is_base_of_v<Introspectable, T>,
                  "Type must inherit from Introspectable");

    const auto &type_info = T::getStaticTypeInfo();
    std::string final_name =
        class_name.empty() ? type_info.class_name : class_name;

    if (bound_classes.find(final_name) != bound_classes.end()) {
        throw std::runtime_error("Class already bound: " + final_name);
    }
    bound_classes.insert(final_name);

    ObjectWrapper<T>::Init(env, exports, final_name);
}

// ================================================