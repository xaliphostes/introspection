template <typename T>
inline std::string getTypeName()
{
    // Remove const, volatile, and reference qualifiers
    using BaseType = std::remove_cv_t<std::remove_reference_t<T>>;

    if constexpr (std::is_same_v<BaseType, std::string>)
    {
        return "string";
    }
    else if constexpr (std::is_same_v<BaseType, char>)
    {
        return "char";
    }
    else if constexpr (std::is_same_v<BaseType, unsigned char>)
    {
        return "unsigned char";
    }
    else if constexpr (std::is_same_v<BaseType, char *>)
    {
        return "char*";
    }
    else if constexpr (std::is_same_v<BaseType, const char *>)
    {
        return "const char*";
    }

    else if constexpr (std::is_pointer_v<BaseType>)
    {
        return getTypeName<std::remove_pointer_t<BaseType>>() + "*";
    }
    else
    {
        // Fallback to mangled name for unknown types
        return typeid(T).name();
    }
}

// Specialized versions for common types
template <>
inline std::string getTypeName<short>() { return "short"; }

template <>
inline std::string getTypeName<unsigned short>() { return "unsigned short"; }

template <>
inline std::string getTypeName<long>() { return "long"; }

template <>
inline std::string getTypeName<unsigned long>() { return "unsigned long"; }

template <>
inline std::string getTypeName<long long>() { return "long long"; }

template <>
inline std::string getTypeName<unsigned int>() { return "unsigned int"; }

template <>
inline std::string getTypeName<size_t>() { return "size_t"; }

template <>
inline std::string getTypeName<int>() { return "int"; }

template <>
inline std::string getTypeName<double>() { return "double"; }

template <>
inline std::string getTypeName<float>() { return "float"; }

template <>
inline std::string getTypeName<bool>() { return "bool"; }

template <>
inline std::string getTypeName<void>() { return "void"; }

template <typename Class>
template <typename MemberType>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::member(const std::string &name, MemberType Class::*member_ptr)
{
    info.addMember(std::make_unique<MemberInfo>(
        name,
        getTypeName<MemberType>(),
        [member_ptr](const void *obj) -> std::any
        {
            const auto *typed_obj = static_cast<const Class *>(obj);
            return std::any{typed_obj->*member_ptr};
        },
        [member_ptr](void *obj, const std::any &value)
        {
            auto *typed_obj = static_cast<Class *>(obj);
            typed_obj->*member_ptr = std::any_cast<MemberType>(value);
        }));
    return *this;
}

template <typename Class>
template <typename ReturnType>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name, ReturnType (Class::*method_ptr)())
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        std::vector<std::string>(),
        [method_ptr](void *obj, const std::vector<std::any> &) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);
            if constexpr (std::is_void_v<ReturnType>)
            {
                (typed_obj->*method_ptr)();
                return std::any{};
            }
            else
            {
                return std::any{(typed_obj->*method_ptr)()};
            }
        }));
    return *this;
}

template <typename Class>
template <typename ReturnType>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name, ReturnType (Class::*method_ptr)() const)
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        std::vector<std::string>(),
        [method_ptr](void *obj, const std::vector<std::any> &) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);
            if constexpr (std::is_void_v<ReturnType>)
            {
                (typed_obj->*method_ptr)();
                return std::any{};
            }
            else
            {
                return std::any{(typed_obj->*method_ptr)()};
            }
        }));
    return *this;
}

template <typename Class>
template <typename ReturnType, typename Param1>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name, ReturnType (Class::*method_ptr)(Param1))
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        std::vector<std::string>{getTypeName<Param1>()},
        [method_ptr](void *obj, const std::vector<std::any> &args) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);
            auto param1 = std::any_cast<Param1>(args[0]);
            if constexpr (std::is_void_v<ReturnType>)
            {
                (typed_obj->*method_ptr)(param1);
                return std::any{};
            }
            else
            {
                return std::any{(typed_obj->*method_ptr)(param1)};
            }
        }));
    return *this;
}

template <typename Class>
template <typename ReturnType, typename Param1, typename Param2>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name, ReturnType (Class::*method_ptr)(Param1, Param2))
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        std::vector<std::string>{getTypeName<Param1>(), getTypeName<Param2>()},
        [method_ptr](void *obj, const std::vector<std::any> &args) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);
            auto param1 = std::any_cast<Param1>(args[0]);
            auto param2 = std::any_cast<Param2>(args[1]);
            if constexpr (std::is_void_v<ReturnType>)
            {
                (typed_obj->*method_ptr)(param1, param2);
                return std::any{};
            }
            else
            {
                return std::any{(typed_obj->*method_ptr)(param1, param2)};
            }
        }));
    return *this;
}

template <typename Class>
template <typename ReturnType, typename Param1, typename Param2, typename Param3>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name, ReturnType (Class::*method_ptr)(Param1, Param2, Param3))
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        std::vector<std::string>{getTypeName<Param1>(), getTypeName<Param2>(), getTypeName<Param3>()},
        [method_ptr](void *obj, const std::vector<std::any> &args) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);
            auto param1 = std::any_cast<Param1>(args[0]);
            auto param2 = std::any_cast<Param2>(args[1]);
            auto param3 = std::any_cast<Param3>(args[2]);
            if constexpr (std::is_void_v<ReturnType>)
            {
                (typed_obj->*method_ptr)(param1, param2, param3);
                return std::any{};
            }
            else
            {
                return std::any{(typed_obj->*method_ptr)(param1, param2, param3)};
            }
        }));
    return *this;
}
