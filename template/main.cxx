#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <typeinfo>
#include <any>
#include <memory>

// Forward declarations
class TypeInfo;
class MemberInfo;
class MethodInfo;

// Base class for introspectable objects
class Introspectable {
public:
    virtual ~Introspectable() = default;
    virtual const TypeInfo& getTypeInfo() const = 0;
    
    // Introspection utility methods (declarations only)
    std::any getMemberValue(const std::string& member_name) const;
    void setMemberValue(const std::string& member_name, const std::any& value);
    std::any callMethod(const std::string& method_name, const std::vector<std::any>& args = {});
    std::vector<std::string> getMemberNames() const;
    std::vector<std::string> getMethodNames() const;
    std::string getClassName() const;
    bool hasMember(const std::string& name) const;
    bool hasMethod(const std::string& name) const;
    void printMemberValue(const std::string& member_name) const;
    void printClassInfo() const;
};

// Implementation of Introspectable methods (after TypeInfo is fully defined)
inline std::any Introspectable::getMemberValue(const std::string& member_name) const {
    const auto& type_info = getTypeInfo();
    const auto* member = type_info.getMember(member_name);
    if (member) {
        return member->getter(this);
    }
    throw std::runtime_error("Member '" + member_name + "' not found");
}

inline void Introspectable::setMemberValue(const std::string& member_name, const std::any& value) {
    const auto& type_info = getTypeInfo();
    const auto* member = type_info.getMember(member_name);
    if (member) {
        member->setter(const_cast<void*>(static_cast<const void*>(this)), value);
    } else {
        throw std::runtime_error("Member '" + member_name + "' not found");
    }
}

inline std::any Introspectable::callMethod(const std::string& method_name, const std::vector<std::any>& args) {
    const auto& type_info = getTypeInfo();
    const auto* method = type_info.getMethod(method_name);
    if (method) {
        return method->invoker(const_cast<void*>(static_cast<const void*>(this)), args);
    }
    throw std::runtime_error("Method '" + method_name + "' not found");
}

inline std::vector<std::string> Introspectable::getMemberNames() const {
    return getTypeInfo().getMemberNames();
}

inline std::vector<std::string> Introspectable::getMethodNames() const {
    return getTypeInfo().getMethodNames();
}

inline std::string Introspectable::getClassName() const {
    return getTypeInfo().class_name;
}

inline bool Introspectable::hasMember(const std::string& name) const {
    return getTypeInfo().getMember(name) != nullptr;
}

inline bool Introspectable::hasMethod(const std::string& name) const {
    return getTypeInfo().getMethod(name) != nullptr;
}

inline void Introspectable::printMemberValue(const std::string& member_name) const {
    const auto& type_info = getTypeInfo();
    const auto* member = type_info.getMember(member_name);
    if (member) {
        auto value = member->getter(this);
        std::cout << member_name << " (" << member->type_name << "): ";
        
        // Print based on type
        if (member->type_name == "string") {
            std::cout << std::any_cast<std::string>(value);
        } else if (member->type_name == "int") {
            std::cout << std::any_cast<int>(value);
        } else if (member->type_name == "double") {
            std::cout << std::any_cast<double>(value);
        } else if (member->type_name == "float") {
            std::cout << std::any_cast<float>(value);
        } else if (member->type_name == "bool") {
            std::cout << (std::any_cast<bool>(value) ? "true" : "false");
        } else {
            std::cout << "[" << member->type_name << " value]";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Member '" << member_name << "' not found" << std::endl;
    }
}

inline void Introspectable::printClassInfo() const {
    const auto& type_info = getTypeInfo();
    std::cout << "Class: " << type_info.class_name << std::endl;
    
    std::cout << "Members:" << std::endl;
    for (const auto& member_name : type_info.getMemberNames()) {
        const auto* member = type_info.getMember(member_name);
        std::cout << "  " << member_name << " (" << member->type_name << ")" << std::endl;
    }
    
    std::cout << "Methods:" << std::endl;
    for (const auto& method_name : type_info.getMethodNames()) {
        const auto* method = type_info.getMethod(method_name);
        std::cout << "  " << method_name << " -> " << method->return_type;
        if (!method->parameter_types.empty()) {
            std::cout << " (params: ";
            for (size_t i = 0; i < method->parameter_types.size(); ++i) {
                if (i > 0) std::cout << ", ";
                std::cout << method->parameter_types[i];
            }
            std::cout << ")";
        }
        std::cout << std::endl;
    }
}

// Holds information about a data member
class MemberInfo {
public:
    std::string name;
    std::string type_name;
    std::function<std::any(const void*)> getter;
    std::function<void(void*, const std::any&)> setter;
    
    MemberInfo(const std::string& n, const std::string& t, 
               std::function<std::any(const void*)> g,
               std::function<void(void*, const std::any&)> s)
        : name(n), type_name(t), getter(g), setter(s) {}
};

// Holds information about a method
class MethodInfo {
public:
    std::string name;
    std::string return_type;
    std::vector<std::string> parameter_types;
    std::function<std::any(void*, const std::vector<std::any>&)> invoker;
    
    MethodInfo(const std::string& n, const std::string& ret_type,
               const std::vector<std::string>& param_types,
               std::function<std::any(void*, const std::vector<std::any>&)> inv)
        : name(n), return_type(ret_type), parameter_types(param_types), invoker(inv) {}
};

// Holds complete type information
class TypeInfo {
public:
    std::string class_name;
    std::unordered_map<std::string, std::unique_ptr<MemberInfo>> members;
    std::unordered_map<std::string, std::unique_ptr<MethodInfo>> methods;
    
    explicit TypeInfo(const std::string& name) : class_name(name) {}
    
    void addMember(std::unique_ptr<MemberInfo> member) {
        members[member->name] = std::move(member);
    }
    
    void addMethod(std::unique_ptr<MethodInfo> method) {
        methods[method->name] = std::move(method);
    }
    
    const MemberInfo* getMember(const std::string& name) const {
        auto it = members.find(name);
        return (it != members.end()) ? it->second.get() : nullptr;
    }
    
    const MethodInfo* getMethod(const std::string& name) const {
        auto it = methods.find(name);
        return (it != methods.end()) ? it->second.get() : nullptr;
    }
    
    std::vector<std::string> getMemberNames() const {
        std::vector<std::string> names;
        for (const auto& pair : members) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    std::vector<std::string> getMethodNames() const {
        std::vector<std::string> names;
        for (const auto& pair : methods) {
            names.push_back(pair.first);
        }
        return names;
    }
};

// Template helpers for type name extraction
template<typename T>
std::string getTypeName() {
    return typeid(T).name();
}

// Specialized versions for common types
template<> std::string getTypeName<int>() { return "int"; }
template<> std::string getTypeName<double>() { return "double"; }
template<> std::string getTypeName<float>() { return "float"; }
template<> std::string getTypeName<std::string>() { return "string"; }
template<> std::string getTypeName<bool>() { return "bool"; }
template<> std::string getTypeName<void>() { return "void"; }

// Template-based registration system
template<typename Class>
class TypeRegistrar {
private:
    TypeInfo& info;

public:
    explicit TypeRegistrar(TypeInfo& type_info) : info(type_info) {}

    // Member registration
    template<typename MemberType>
    TypeRegistrar& member(const std::string& name, MemberType Class::* member_ptr) {
        info.addMember(std::make_unique<MemberInfo>(
            name,
            getTypeName<MemberType>(),
            [member_ptr](const void* obj) -> std::any {
                const auto* typed_obj = static_cast<const Class*>(obj);
                return std::any{typed_obj->*member_ptr};
            },
            [member_ptr](void* obj, const std::any& value) {
                auto* typed_obj = static_cast<Class*>(obj);
                typed_obj->*member_ptr = std::any_cast<MemberType>(value);
            }
        ));
        return *this;
    }

    // Method registration for methods with no parameters
    template<typename ReturnType>
    TypeRegistrar& method(const std::string& name, ReturnType (Class::*method_ptr)()) {
        info.addMethod(std::make_unique<MethodInfo>(
            name,
            getTypeName<ReturnType>(),
            std::vector<std::string>(),
            [method_ptr](void* obj, const std::vector<std::any>&) -> std::any {
                auto* typed_obj = static_cast<Class*>(obj);
                if constexpr (std::is_void_v<ReturnType>) {
                    (typed_obj->*method_ptr)();
                    return std::any{};
                } else {
                    return std::any{(typed_obj->*method_ptr)()};
                }
            }
        ));
        return *this;
    }

    // Method registration for const methods with no parameters
    template<typename ReturnType>
    TypeRegistrar& method(const std::string& name, ReturnType (Class::*method_ptr)() const) {
        info.addMethod(std::make_unique<MethodInfo>(
            name,
            getTypeName<ReturnType>(),
            std::vector<std::string>(),
            [method_ptr](void* obj, const std::vector<std::any>&) -> std::any {
                auto* typed_obj = static_cast<Class*>(obj);
                if constexpr (std::is_void_v<ReturnType>) {
                    (typed_obj->*method_ptr)();
                    return std::any{};
                } else {
                    return std::any{(typed_obj->*method_ptr)()};
                }
            }
        ));
        return *this;
    }

    // Method registration for methods with one parameter
    template<typename ReturnType, typename Param1>
    TypeRegistrar& method(const std::string& name, ReturnType (Class::*method_ptr)(Param1)) {
        info.addMethod(std::make_unique<MethodInfo>(
            name,
            getTypeName<ReturnType>(),
            std::vector<std::string>{getTypeName<Param1>()},
            [method_ptr](void* obj, const std::vector<std::any>& args) -> std::any {
                auto* typed_obj = static_cast<Class*>(obj);
                auto param1 = std::any_cast<Param1>(args[0]);
                if constexpr (std::is_void_v<ReturnType>) {
                    (typed_obj->*method_ptr)(param1);
                    return std::any{};
                } else {
                    return std::any{(typed_obj->*method_ptr)(param1)};
                }
            }
        ));
        return *this;
    }

    // Method registration for methods with two parameters
    template<typename ReturnType, typename Param1, typename Param2>
    TypeRegistrar& method(const std::string& name, ReturnType (Class::*method_ptr)(Param1, Param2)) {
        info.addMethod(std::make_unique<MethodInfo>(
            name,
            getTypeName<ReturnType>(),
            std::vector<std::string>{getTypeName<Param1>(), getTypeName<Param2>()},
            [method_ptr](void* obj, const std::vector<std::any>& args) -> std::any {
                auto* typed_obj = static_cast<Class*>(obj);
                auto param1 = std::any_cast<Param1>(args[0]);
                auto param2 = std::any_cast<Param2>(args[1]);
                if constexpr (std::is_void_v<ReturnType>) {
                    (typed_obj->*method_ptr)(param1, param2);
                    return std::any{};
                } else {
                    return std::any{(typed_obj->*method_ptr)(param1, param2)};
                }
            }
        ));
        return *this;
    }
};

// Simplified macro for introspectable classes
#define INTROSPECTABLE_CLASS(ClassName) \
public: \
    static TypeInfo& getStaticTypeInfo() { \
        static TypeInfo info(#ClassName); \
        static bool initialized = false; \
        if (!initialized) { \
            registerIntrospection(TypeRegistrar<ClassName>(info)); \
            initialized = true; \
        } \
        return info; \
    } \
    const TypeInfo& getTypeInfo() const override { \
        return getStaticTypeInfo(); \
    } \
private: \
    static void registerIntrospection(TypeRegistrar<ClassName> reg); \
public:

// Example class using the introspection system
class Person : public Introspectable {
    INTROSPECTABLE_CLASS(Person)
    
private:
    std::string name;
    int age;
    double height;
    
public:
    Person() : name(""), age(0), height(0.0) {}
    Person(const std::string& n, int a, double h) : name(n), age(a), height(h) {}
    
    // Regular methods
    void introduce() {
        std::cout << "Hello, I'm " << name << ", " << age << " years old, " 
                  << height << "m tall." << std::endl;
    }
    
    std::string getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    
    int getAge() const { return age; }
    void setAge(int a) { age = a; }
    
    double getHeight() const { return height; }
    void setHeight(double h) { height = h; }
    
    std::string getDescription() const {
        return name + " (" + std::to_string(age) + " years, " + 
               std::to_string(height) + "m)";
    }
};

// Registration implementation using templates
void Person::registerIntrospection(TypeRegistrar<Person> reg) {
    reg.member("name", &Person::name)
       .member("age", &Person::age)
       .member("height", &Person::height)
       .method("introduce", &Person::introduce)
       .method("getName", &Person::getName)
       .method("setName", &Person::setName)
       .method("getAge", &Person::getAge)
       .method("setAge", &Person::setAge)
       .method("getHeight", &Person::getHeight)
       .method("setHeight", &Person::setHeight)
       .method("getDescription", &Person::getDescription);
}

// Example usage
int main() {
    Person person("Alice", 30, 1.65);
    
    std::cout << "=== Class Introspection Demo ===" << std::endl;
    
    // Print class information using member method
    person.printClassInfo();
    std::cout << std::endl;
    
    // Access members through introspection using member methods
    std::cout << "=== Member Access ===" << std::endl;
    person.printMemberValue("name");
    person.printMemberValue("age");
    person.printMemberValue("height");
    std::cout << std::endl;
    
    // Modify members through introspection using member methods
    std::cout << "=== Member Modification ===" << std::endl;
    person.setMemberValue("name", std::string("Bob"));
    person.setMemberValue("age", 25);
    person.printMemberValue("name");
    person.printMemberValue("age");
    std::cout << std::endl;
    
    // Call methods through introspection using member methods
    std::cout << "=== Method Invocation ===" << std::endl;
    person.callMethod("introduce");
    
    auto desc = person.callMethod("getDescription");
    std::cout << "Description: " << std::any_cast<std::string>(desc) << std::endl;
    
    // Call method with parameters
    person.callMethod("setName", std::vector<std::any>{std::string("Charlie")});
    person.callMethod("introduce");
    
    std::cout << std::endl;
    
    // Demonstrate utility methods
    std::cout << "=== Utility Methods ===" << std::endl;
    std::cout << "Class name: " << person.getClassName() << std::endl;
    std::cout << "Has 'name' member: " << (person.hasMember("name") ? "yes" : "no") << std::endl;
    std::cout << "Has 'weight' member: " << (person.hasMember("weight") ? "yes" : "no") << std::endl;
    std::cout << "Has 'introduce' method: " << (person.hasMethod("introduce") ? "yes" : "no") << std::endl;
    
    // Get all member and method names
    std::cout << "\nAll members: ";
    for (const auto& name : person.getMemberNames()) {
        std::cout << name << " ";
    }
    std::cout << "\nAll methods: ";
    for (const auto& name : person.getMethodNames()) {
        std::cout << name << " ";
    }
    std::cout << std::endl;
    
    return 0;
}