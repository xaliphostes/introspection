#include <any>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

// Forward declarations
class TypeInfo;
class MemberInfo;
class MethodInfo;

// Base class for introspectable objects
class Introspectable {
  public:
    virtual ~Introspectable() = default;
    virtual const TypeInfo &getTypeInfo() const = 0;
};

// Holds information about a data member
class MemberInfo {
  public:
    std::string name;
    std::string type_name;
    std::function<std::any(const void *)> getter;
    std::function<void(void *, const std::any &)> setter;

    MemberInfo(const std::string &n, const std::string &t,
               std::function<std::any(const void *)> g,
               std::function<void(void *, const std::any &)> s)
        : name(n), type_name(t), getter(g), setter(s) {}
};

// Holds information about a method
class MethodInfo {
  public:
    std::string name;
    std::string return_type;
    std::vector<std::string> parameter_types;
    std::function<std::any(void *, const std::vector<std::any> &)> invoker;

    MethodInfo(
        const std::string &n, const std::string &ret_type,
        const std::vector<std::string> &param_types,
        std::function<std::any(void *, const std::vector<std::any> &)> inv)
        : name(n), return_type(ret_type), parameter_types(param_types),
          invoker(inv) {}
};

// Holds complete type information
class TypeInfo {
  public:
    std::string class_name;
    std::unordered_map<std::string, std::unique_ptr<MemberInfo>> members;
    std::unordered_map<std::string, std::unique_ptr<MethodInfo>> methods;

    explicit TypeInfo(const std::string &name) : class_name(name) {}

    void addMember(std::unique_ptr<MemberInfo> member) {
        members[member->name] = std::move(member);
    }

    void addMethod(std::unique_ptr<MethodInfo> method) {
        methods[method->name] = std::move(method);
    }

    const MemberInfo *getMember(const std::string &name) const {
        auto it = members.find(name);
        return (it != members.end()) ? it->second.get() : nullptr;
    }

    const MethodInfo *getMethod(const std::string &name) const {
        auto it = methods.find(name);
        return (it != methods.end()) ? it->second.get() : nullptr;
    }

    std::vector<std::string> getMemberNames() const {
        std::vector<std::string> names;
        for (const auto &pair : members) {
            names.push_back(pair.first);
        }
        return names;
    }

    std::vector<std::string> getMethodNames() const {
        std::vector<std::string> names;
        for (const auto &pair : methods) {
            names.push_back(pair.first);
        }
        return names;
    }
};

// Template helpers for type name extraction
template <typename T> std::string getTypeName() { return typeid(T).name(); }

// Specialized versions for common types
template <> std::string getTypeName<int>() { return "int"; }
template <> std::string getTypeName<double>() { return "double"; }
template <> std::string getTypeName<float>() { return "float"; }
template <> std::string getTypeName<std::string>() { return "string"; }
template <> std::string getTypeName<bool>() { return "bool"; }
template <> std::string getTypeName<void>() { return "void"; }

// Macros for easy registration
#define INTROSPECTABLE_CLASS(ClassName)                                        \
  public:                                                                      \
    static TypeInfo &getStaticTypeInfo() {                                     \
        static TypeInfo info(#ClassName);                                      \
        static bool initialized = false;                                       \
        if (!initialized) {                                                    \
            registerMembers(info);                                             \
            registerMethods(info);                                             \
            initialized = true;                                                \
        }                                                                      \
        return info;                                                           \
    }                                                                          \
    const TypeInfo &getTypeInfo() const override {                             \
        return getStaticTypeInfo();                                            \
    }                                                                          \
                                                                               \
  private:                                                                     \
    static void registerMembers(TypeInfo &info);                               \
    static void registerMethods(TypeInfo &info);                               \
                                                                               \
  public:

#define REGISTER_MEMBER(ClassName, MemberName, MemberType)                     \
    info.addMember(std::make_unique<MemberInfo>(                               \
        #MemberName, getTypeName<MemberType>(),                                \
        [](const void *obj) -> std::any {                                      \
            const auto *typed_obj = static_cast<const ClassName *>(obj);       \
            return std::any(typed_obj->MemberName);                            \
        },                                                                     \
        [](void *obj, const std::any &value) {                                 \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            typed_obj->MemberName = std::any_cast<MemberType>(value);          \
        }));

#define REGISTER_METHOD_0(ClassName, MethodName, ReturnType)                   \
    info.addMethod(std::make_unique<MethodInfo>(                               \
        #MethodName, getTypeName<ReturnType>(), std::vector<std::string>(),    \
        [](void *obj, const std::vector<std::any> &) -> std::any {             \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            if constexpr (std::is_void_v<ReturnType>) {                        \
                typed_obj->MethodName();                                       \
                return std::any{};                                             \
            } else {                                                           \
                return std::any{typed_obj->MethodName()};                      \
            }                                                                  \
        }));

#define REGISTER_METHOD_1(ClassName, MethodName, ReturnType, Param1Type)       \
    info.addMethod(std::make_unique<MethodInfo>(                               \
        #MethodName, getTypeName<ReturnType>(),                                \
        std::vector<std::string>{getTypeName<Param1Type>()},                   \
        [](void *obj, const std::vector<std::any> &args) -> std::any {         \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            auto param1 = std::any_cast<Param1Type>(args[0]);                  \
            if constexpr (std::is_void_v<ReturnType>) {                        \
                typed_obj->MethodName(param1);                                 \
                return std::any{};                                             \
            } else {                                                           \
                return std::any{typed_obj->MethodName()};                      \
            }                                                                  \
        }));

// Specialized macros for void-returning methods
#define REGISTER_VOID_METHOD_0(ClassName, MethodName)                          \
    info.addMethod(std::make_unique<MethodInfo>(                               \
        #MethodName, getTypeName<void>(), std::vector<std::string>(),          \
        [](void *obj, const std::vector<std::any> &) -> std::any {             \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            typed_obj->MethodName();                                           \
            return std::any{};                                                 \
        }));

#define REGISTER_VOID_METHOD_1(ClassName, MethodName, Param1Type)              \
    info.addMethod(std::make_unique<MethodInfo>(                               \
        #MethodName, getTypeName<void>(),                                      \
        std::vector<std::string>{getTypeName<Param1Type>()},                   \
        [](void *obj, const std::vector<std::any> &args) -> std::any {         \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            auto param1 = std::any_cast<Param1Type>(args[0]);                  \
            typed_obj->MethodName(param1);                                     \
            return std::any{};                                                 \
        }));

#define REGISTER_VOID_METHOD_2(ClassName, MethodName, Param1Type, Param2Type)  \
    info.addMethod(std::make_unique<MethodInfo>(                               \
        #MethodName, getTypeName<void>(),                                      \
        std::vector<std::string>{getTypeName<Param1Type>(),                    \
                                 getTypeName<Param2Type>()},                   \
        [](void *obj, const std::vector<std::any> &args) -> std::any {         \
            auto *typed_obj = static_cast<ClassName *>(obj);                   \
            auto param1 = std::any_cast<Param1Type>(args[0]);                  \
            auto param2 = std::any_cast<Param2Type>(args[1]);                  \
            typed_obj->MethodName(param1, param2);                             \
            return std::any{};                                                 \
        }));

// Example class using the introspection system
class Person : public Introspectable {
    INTROSPECTABLE_CLASS(Person)

  private:
    std::string name;
    int age;
    double height;

  public:
    Person() : name(""), age(0), height(0.0) {}
    Person(const std::string &n, int a, double h)
        : name(n), age(a), height(h) {}

    // Regular methods
    void introduce() {
        std::cout << "Hello, I'm " << name << ", " << age << " years old, "
                  << height << "m tall." << std::endl;
    }

    std::string getName() const { return name; }
    void setName(const std::string &n) { name = n; }

    int getAge() const { return age; }
    void setAge(int a) { age = a; }

    double getHeight() const { return height; }
    void setHeight(double h) { height = h; }
    void setHeightAndAge(double h, int a) {
        setHeight(h);
        setAge(a);
    }

    std::string getDescription() const {
        return name + " (" + std::to_string(age) + " years, " +
               std::to_string(height) + "m)";
    }
};

// Registration implementation
void Person::registerMembers(TypeInfo &info) {
    REGISTER_MEMBER(Person, name, std::string);
    REGISTER_MEMBER(Person, age, int);
    REGISTER_MEMBER(Person, height, double);
}

void Person::registerMethods(TypeInfo &info) {
    // Void methods use specialized macros
    REGISTER_VOID_METHOD_0(Person, introduce);
    REGISTER_VOID_METHOD_1(Person, setName, std::string);
    REGISTER_VOID_METHOD_1(Person, setAge, int);
    REGISTER_VOID_METHOD_1(Person, setHeight, double);
    REGISTER_VOID_METHOD_2(Person, setHeightAndAge, double, int);

    // Non-void methods use regular macros
    REGISTER_METHOD_0(Person, getName, std::string);
    REGISTER_METHOD_0(Person, getAge, int);
    REGISTER_METHOD_0(Person, getHeight, double);
    REGISTER_METHOD_0(Person, getDescription, std::string);
}

// Utility functions for introspection
class IntrospectionUtils {
  public:
    template <typename T>
    static void printMemberValue(const T &obj, const std::string &member_name) {
        const auto &type_info = obj.getTypeInfo();
        const auto *member = type_info.getMember(member_name);
        if (member) {
            auto value = member->getter(&obj);
            std::cout << member_name << " (" << member->type_name << "): ";

            // Print based on type
            if (member->type_name == "string") {
                std::cout << std::any_cast<std::string>(value);
            } else if (member->type_name == "int") {
                std::cout << std::any_cast<int>(value);
            } else if (member->type_name == "double") {
                std::cout << std::any_cast<double>(value);
            }
            std::cout << std::endl;
        } else {
            std::cout << "Member '" << member_name << "' not found"
                      << std::endl;
        }
    }

    template <typename T>
    static void setMemberValue(T &obj, const std::string &member_name,
                               const std::any &value) {
        const auto &type_info = obj.getTypeInfo();
        const auto *member = type_info.getMember(member_name);
        if (member) {
            member->setter(&obj, value);
        } else {
            std::cout << "Member '" << member_name << "' not found"
                      << std::endl;
        }
    }

    template <typename T>
    static std::any callMethod(T &obj, const std::string &method_name,
                               const std::vector<std::any> &args = {}) {
        const auto &type_info = obj.getTypeInfo();
        const auto *method = type_info.getMethod(method_name);
        if (method) {
            return method->invoker(&obj, args);
        } else {
            std::cout << "Method '" << method_name << "' not found"
                      << std::endl;
            return std::any();
        }
    }

    template <typename T> static void printClassInfo(const T &obj) {
        const auto &type_info = obj.getTypeInfo();
        std::cout << "Class: " << type_info.class_name << std::endl;

        std::cout << "Members:" << std::endl;
        for (const auto &member_name : type_info.getMemberNames()) {
            const auto *member = type_info.getMember(member_name);
            std::cout << "  " << member_name << " (" << member->type_name << ")"
                      << std::endl;
        }

        std::cout << "Methods:" << std::endl;
        for (const auto &method_name : type_info.getMethodNames()) {
            const auto *method = type_info.getMethod(method_name);
            std::cout << "  " << method_name << " -> " << method->return_type
                      << std::endl;
        }
    }
};

// Example usage
int main() {
    Person person("Alice", 30, 1.65);

    std::cout << "=== Class Introspection Demo ===" << std::endl;

    // Print class information
    IntrospectionUtils::printClassInfo(person);
    std::cout << std::endl;

    // Access members through introspection
    std::cout << "=== Member Access ===" << std::endl;
    IntrospectionUtils::printMemberValue(person, "name");
    IntrospectionUtils::printMemberValue(person, "age");
    IntrospectionUtils::printMemberValue(person, "height");
    std::cout << std::endl;

    // Modify members through introspection
    std::cout << "=== Member Modification ===" << std::endl;
    IntrospectionUtils::setMemberValue(person, "name", std::string("Bob"));
    IntrospectionUtils::setMemberValue(person, "age", 25);
    IntrospectionUtils::printMemberValue(person, "name");
    IntrospectionUtils::printMemberValue(person, "age");
    std::cout << std::endl;

    // Call methods through introspection
    std::cout << "=== Method Invocation ===" << std::endl;
    IntrospectionUtils::callMethod(person, "introduce");

    auto desc = IntrospectionUtils::callMethod(person, "getDescription");
    std::cout << "Description: " << std::any_cast<std::string>(desc)
              << std::endl;

    // Call method with parameters
    IntrospectionUtils::callMethod(
        person, "setName", std::vector<std::any>{std::string("Charlie")});
    IntrospectionUtils::callMethod(person, "introduce");

    return 0;
}