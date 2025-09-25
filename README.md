# C++ Introspection Library

A lightweight, header-only C++ introspection system that enables runtime inspection and manipulation of class members and methods without external dependencies.

## Features

- **Runtime Member Access**: Get/set member variables by name
- **Runtime Method Invocation**: Call methods by name with parameters
- **Type-Safe**: Compile-time registration with runtime type checking
- **Template-Based**: Clean, fluent registration API using member/method pointers
- **Zero Dependencies**: No external libraries required
- **C++17 Compatible**: Uses modern C++ features like `std::any` and `if constexpr`

## Quick Start

### 1. Make Your Class Introspectable

```cpp
#include "introspection.hpp"

class Person : public Introspectable {
    INTROSPECTABLE_CLASS(Person)
    
private:
    std::string name;
    int age;
    
public:
    Person(const std::string& n, int a) : name(n), age(a) {}
    
    std::string getName() const { return name; }
    void setName(const std::string& n) { name = n; }
    int getAge() const { return age; }
    void introduce() { std::cout << "Hi, I'm " << name << std::endl; }
};
```

### 2. Register Members and Methods

```cpp
void Person::registerIntrospection(TypeRegistrar<Person> reg) {
    reg.member("name", &Person::name)
       .member("age", &Person::age)
       .method("getName", &Person::getName)
       .method("setName", &Person::setName)
       .method("getAge", &Person::getAge)
       .method("introduce", &Person::introduce);
}
```

### 3. Use Runtime Introspection

```cpp
Person person("Alice", 30);

// Access members by name
person.printMemberValue("name");  // Prints: name (string): Alice
person.setMemberValue("age", 25);
std::cout << std::any_cast<int>(person.getMemberValue("age")); // Prints: 25

// Call methods by name
person.callMethod("introduce");   // Prints: Hi, I'm Alice
person.callMethod("setName", std::vector<std::any>{std::string("Bob")});
auto name = person.callMethod("getName");
std::cout << std::any_cast<std::string>(name); // Prints: Bob

// Introspection utilities
std::cout << person.getClassName();        // Prints: Person
std::cout << person.hasMember("name");     // Prints: 1 (true)
std::cout << person.hasMethod("introduce"); // Prints: 1 (true)
person.printClassInfo(); // Prints complete class information
```

## API Reference

### Introspectable Base Class

All introspectable classes inherit from `Introspectable` and gain these methods:

- `getMemberValue(name)` → `std::any` - Get member value by name
- `setMemberValue(name, value)` → `void` - Set member value by name
- `callMethod(name, args = {})` → `std::any` - Call method by name
- `hasMember(name)` → `bool` - Check if member exists
- `hasMethod(name)` → `bool` - Check if method exists
- `getMemberNames()` → `vector<string>` - Get all member names
- `getMethodNames()` → `vector<string>` - Get all method names
- `getClassName()` → `string` - Get class name
- `printMemberValue(name)` - Print member with type info
- `printClassInfo()` - Print complete class information

### TypeRegistrar Template

Fluent registration API:

- `member(name, &Class::member)` - Register a member variable
- `method(name, &Class::method)` - Register a method (supports 0-2 parameters)

Both return `TypeRegistrar&` for method chaining.

## Compilation

Requires C++17 or later:

```cmake
cmake_minimum_required(VERSION 3.17)
set(CMAKE_CXX_STANDARD 17)
add_executable(your_app main.cpp)
```

```bash
g++ -std=c++17 main.cpp -o your_app
```

## Supported Types

- All fundamental types (`int`, `double`, `float`, `bool`, etc.)
- `std::string`
- Custom classes (with appropriate `std::any` casting)
- Method parameters and return values (including `void`)

## Limitations

- Maximum 2 method parameters (easily extensible)
- Requires explicit registration of members/methods
- Runtime overhead due to `std::any` and function pointers
- No inheritance introspection (each class registers independently)

## License

MIT License - feel free to use in your project