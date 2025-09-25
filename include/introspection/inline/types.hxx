#include <stdexcept>

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

// Helper function to create parameter type vector from parameter pack
template <typename... Args>
std::vector<std::string> createParameterTypeVector()
{
    if constexpr (sizeof...(Args) == 0)
    {
        return std::vector<std::string>{};
    }
    else
    {
        return std::vector<std::string>{getTypeName<Args>()...};
    }
}

// Helper function to cast arguments from std::any vector to the correct types
// This uses index_sequence to unpack the arguments at compile time
template <typename Class, typename ReturnType, typename... Args, std::size_t... I>
inline std::any callMethodImpl(Class *obj, ReturnType (Class::*method_ptr)(Args...),
                               const std::vector<std::any> &args, std::index_sequence<I...>)
{
    if constexpr (std::is_void_v<ReturnType>)
    {
        (obj->*method_ptr)(std::any_cast<Args>(args[I])...);
        return std::any{};
    }
    else
    {
        return std::any{(obj->*method_ptr)(std::any_cast<Args>(args[I])...)};
    }
}

// Same for const methods
template <typename Class, typename ReturnType, typename... Args, std::size_t... I>
inline std::any callConstMethodImpl(Class *obj, ReturnType (Class::*method_ptr)(Args...) const,
                                    const std::vector<std::any> &args, std::index_sequence<I...>)
{
    if constexpr (std::is_void_v<ReturnType>)
    {
        (obj->*method_ptr)(std::any_cast<Args>(args[I])...);
        return std::any{};
    }
    else
    {
        return std::any{(obj->*method_ptr)(std::any_cast<Args>(args[I])...)};
    }
}

// Variadic method registration for non-const methods
template <typename Class>
template <typename ReturnType, typename... Args>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name,
                                                          ReturnType (Class::*method_ptr)(Args...))
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        createParameterTypeVector<Args...>(),
        [method_ptr, name](void *obj, const std::vector<std::any> &args) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);

            // Validate argument count at runtime
            if (args.size() != sizeof...(Args))
            {
                throw std::runtime_error("Incorrect number of arguments for method '" + name +
                                         "'. Expected " + std::to_string(sizeof...(Args)) +
                                         ", got " + std::to_string(args.size()));
            }

            // Use index_sequence to unpack arguments
            return callMethodImpl(typed_obj, method_ptr, args, std::index_sequence_for<Args...>{});
        }));
    return *this;
}

// Variadic method registration for const methods
template <typename Class>
template <typename ReturnType, typename... Args>
inline TypeRegistrar<Class> &TypeRegistrar<Class>::method(const std::string &name,
                                                          ReturnType (Class::*method_ptr)(Args...) const)
{
    info.addMethod(std::make_unique<MethodInfo>(
        name,
        getTypeName<ReturnType>(),
        createParameterTypeVector<Args...>(),
        [method_ptr, name](void *obj, const std::vector<std::any> &args) -> std::any
        {
            auto *typed_obj = static_cast<Class *>(obj);

            // Validate argument count at runtime
            if (args.size() != sizeof...(Args))
            {
                throw std::runtime_error("Incorrect number of arguments for method '" + name +
                                         "'. Expected " + std::to_string(sizeof...(Args)) +
                                         ", got " + std::to_string(args.size()));
            }

            // Use index_sequence to unpack arguments
            return callConstMethodImpl(typed_obj, method_ptr, args, std::index_sequence_for<Args...>{});
        }));
    return *this;
}