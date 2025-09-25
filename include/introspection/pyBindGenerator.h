#pragma once
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <introspection/introspectable.h>
#include <memory>
#include <typeinfo>
#include <unordered_set>

namespace py = pybind11;

/**
 * @brief Automatic pybind11 binding generator for introspectable classes
 * @example
 * ```c++
 * // Usage example:
 * PYBIND11_MODULE(my_module, m) {
 *     AutoBindingGenerator generator(m);
 * 
 *     // Automatically bind introspectable classes
 *     generator.bind_class<Person>();
 *     generator.bind_class<GameObject>("GameObj");  // Custom name
 * 
 *     // Or bind multiple at once
 *     generator.bind_classes<Person, GameObject, Vehicle>();
 * 
 *     // Or use the macro
 *     PYBIND11_AUTO_BIND_CLASS(m, Person);
 * }
 * ```
 * 
 * @example
 * ```python
 * import introspection as I
 * 
 * # Create objects using constructors
 * person = I.Person("Alice", 30, 1.65)
 * vehicle = I.Vehicle("Honda", "Civic", 2022)
 * 
 * # Call methods
 * person.introduce()
 * person.celebrate_birthday() # Note: pybind11 converts camelCase to snake_case
 * 
 * vehicle.start()
 * vehicle.drive(100.5)
 * print(vehicle.get_info())
 * ```
 */
class AutoBindingGenerator
{
private:
    py::module_ &module;
    std::unordered_set<std::string> bound_classes;

public:
    explicit AutoBindingGenerator(py::module_ &m) : module(m) {}

    /**
     * @brief Automatically bind an introspectable class to Python
     * @tparam T The introspectable class type
     * @param class_name Optional custom class name (uses introspection name if empty)
     * @return pybind11 class binding for further customization
     */
    template <typename T>
    auto bind_class(const std::string &class_name = "") -> py::class_<T>
    {
        static_assert(std::is_base_of_v<Introspectable, T>,
                      "Type must inherit from Introspectable");

        // Get type info from introspection system
        T dummy_instance;
        const auto &type_info = dummy_instance.getTypeInfo();

        std::string final_class_name = class_name.empty() ? type_info.class_name : class_name;

        // Prevent duplicate bindings
        if (bound_classes.find(final_class_name) != bound_classes.end())
        {
            throw std::runtime_error("Class '" + final_class_name + "' already bound");
        }
        bound_classes.insert(final_class_name);

        // Create pybind11 class
        auto py_class = py::class_<T>(module, final_class_name.c_str());

        // Bind constructors (you may want to customize this)
        bind_constructors<T>(py_class, type_info);

        // Bind all members as properties
        bind_members<T>(py_class, type_info);

        // Bind all methods
        bind_methods<T>(py_class, type_info);

        // Add introspection utilities to Python
        bind_introspection_utilities<T>(py_class);

        return py_class;
    }

    /**
     * @brief Bind multiple classes at once
     */
    template <typename... Classes>
    void bind_classes()
    {
        (bind_class<Classes>(), ...);
    }

private:
    template <typename T>
    void bind_constructors(py::class_<T> &py_class, const TypeInfo &type_info)
    {
        // Default constructor
        py_class.def(py::init<>(), "Default constructor");

        // Note: For more sophisticated constructor binding, you'd need to extend
        // the introspection system to include constructor information
        // For now, we rely on the default constructor and property setters
    }

    template <typename T>
    void bind_members(py::class_<T> &py_class, const TypeInfo &type_info)
    {
        for (const auto &member_name : type_info.getMemberNames())
        {
            const auto *member = type_info.getMember(member_name);
            if (!member)
                continue;

            // Create Python property using introspection getter/setter
            py_class.def_property(
                member_name.c_str(),
                // Getter
                [member_name](const T &obj) -> py::object
                {
                    try
                    {
                        auto value = obj.getMemberValue(member_name);
                        return convert_any_to_python(value, obj.getTypeInfo().getMember(member_name)->type_name);
                    }
                    catch (const std::exception &e)
                    {
                        throw py::value_error("Failed to get member '" + member_name + "': " + e.what());
                    }
                },
                // Setter
                [member_name](T &obj, py::object py_value)
                {
                    try
                    {
                        const auto *member_info = obj.getTypeInfo().getMember(member_name);
                        auto cpp_value = convert_python_to_any(py_value, member_info->type_name);
                        obj.setMemberValue(member_name, cpp_value);
                    }
                    catch (const std::exception &e)
                    {
                        throw py::value_error("Failed to set member '" + member_name + "': " + e.what());
                    }
                },
                ("Access to " + member_name + " member").c_str());
        }
    }

    template <typename T>
    void bind_methods(py::class_<T> &py_class, const TypeInfo &type_info)
    {
        for (const auto &method_name : type_info.getMethodNames())
        {
            const auto *method = type_info.getMethod(method_name);
            if (!method)
                continue;

            // Skip getter/setter methods that are already bound as properties
            if (is_getter_setter_method(method_name))
                continue;

            // Create Python method using introspection
            py_class.def(
                method_name.c_str(),
                [method_name](T &obj, py::args args) -> py::object
                {
                    try
                    {
                        // Convert Python arguments to std::any vector
                        std::vector<std::any> cpp_args;
                        const auto *method_info = obj.getTypeInfo().getMethod(method_name);

                        if (args.size() != method_info->parameter_types.size())
                        {
                            throw py::value_error("Method '" + method_name + "' expects " +
                                                  std::to_string(method_info->parameter_types.size()) +
                                                  " arguments, got " + std::to_string(args.size()));
                        }

                        for (size_t i = 0; i < args.size(); ++i)
                        {
                            cpp_args.push_back(convert_python_to_any(args[i], method_info->parameter_types[i]));
                        }

                        // Call method through introspection
                        auto result = obj.callMethod(method_name, cpp_args);

                        // Convert result back to Python
                        return convert_any_to_python(result, method_info->return_type);
                    }
                    catch (const std::exception &e)
                    {
                        throw py::runtime_error("Failed to call method '" + method_name + "': " + e.what());
                    }
                },
                ("Call " + method_name + " method").c_str());
        }
    }

    template <typename T>
    void bind_introspection_utilities(py::class_<T> &py_class)
    {
        // Expose introspection utilities to Python
        py_class.def("get_class_name", &T::getClassName, "Get the class name");
        py_class.def("get_member_names", &T::getMemberNames, "Get all member names");
        py_class.def("get_method_names", &T::getMethodNames, "Get all method names");
        py_class.def("has_member", &T::hasMember, "Check if member exists");
        py_class.def("has_method", &T::hasMethod, "Check if method exists");
        py_class.def("to_json", &T::toJSON, "Export object to JSON string");

        // Dynamic member/method access
        py_class.def("get_member_value", [](const T &obj, const std::string &name) -> py::object
                     {
                auto value = obj.getMemberValue(name);
                const auto *member = obj.getTypeInfo().getMember(name);
                return convert_any_to_python(value, member ? member->type_name : "unknown"); }, "Get member value by name");

        py_class.def("set_member_value", [](T &obj, const std::string &name, py::object value)
                     {
                const auto *member = obj.getTypeInfo().getMember(name);
                if (!member) throw py::value_error("Member not found: " + name);
                auto cpp_value = convert_python_to_any(value, member->type_name);
                obj.setMemberValue(name, cpp_value); }, "Set member value by name");

        py_class.def("call_method", [](T &obj, const std::string &name, py::list args) -> py::object
                     {
                std::vector<std::any> cpp_args;
                const auto *method = obj.getTypeInfo().getMethod(name);
                if (!method) throw py::value_error("Method not found: " + name);
                
                for (size_t i = 0; i < args.size(); ++i) {
                    if (i < method->parameter_types.size()) {
                        cpp_args.push_back(convert_python_to_any(args[i], method->parameter_types[i]));
                    }
                }
                
                auto result = obj.callMethod(name, cpp_args);
                return convert_any_to_python(result, method->return_type); }, "Call method by name with arguments");
    }

    // Helper function to check if a method is a getter/setter
    bool is_getter_setter_method(const std::string &method_name) const
    {
        return method_name.starts_with("get") ||
               method_name.starts_with("set") ||
               method_name.starts_with("is");
    }

    // Convert std::any to Python object based on type name
    py::object convert_any_to_python(const std::any &value, const std::string &type_name) const
    {
        if (value.has_value() == false || type_name == "void")
        {
            return py::none();
        }

        try
        {
            if (type_name == "string")
            {
                return py::cast(std::any_cast<std::string>(value));
            }
            else if (type_name == "int")
            {
                return py::cast(std::any_cast<int>(value));
            }
            else if (type_name == "double")
            {
                return py::cast(std::any_cast<double>(value));
            }
            else if (type_name == "float")
            {
                return py::cast(std::any_cast<float>(value));
            }
            else if (type_name == "bool")
            {
                return py::cast(std::any_cast<bool>(value));
            }
            else
            {
                // For custom types, try generic casting
                // You may need to extend this for your custom types
                return py::cast(value);
            }
        }
        catch (const std::bad_any_cast &e)
        {
            throw py::type_error("Failed to convert type '" + type_name + "' to Python: " + e.what());
        }
    }

    // Convert Python object to std::any based on expected type
    std::any convert_python_to_any(py::object py_value, const std::string &type_name) const
    {
        try
        {
            if (type_name == "string")
            {
                return std::make_any<std::string>(py_value.cast<std::string>());
            }
            else if (type_name == "int")
            {
                return std::make_any<int>(py_value.cast<int>());
            }
            else if (type_name == "double")
            {
                return std::make_any<double>(py_value.cast<double>());
            }
            else if (type_name == "float")
            {
                return std::make_any<float>(py_value.cast<float>());
            }
            else if (type_name == "bool")
            {
                return std::make_any<bool>(py_value.cast<bool>());
            }
            else
            {
                // For custom types, this is more complex and would require
                // additional type information or registration
                throw py::type_error("Unsupported type conversion for: " + type_name);
            }
        }
        catch (const py::cast_error &e)
        {
            throw py::type_error("Failed to convert Python object to '" + type_name + "': " + e.what());
        }
    }
};

// Convenience macro for auto-binding
#define PYBIND11_AUTO_BIND_CLASS(module, ClassName) \
    AutoBindingGenerator(module).bind_class<ClassName>(#ClassName)
