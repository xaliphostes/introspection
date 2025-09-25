#include <iostream>
#include <unordered_map>
#include <introspection/info.h>
#include <introspection/introspectable.h>

// Example class using the introspection system
//
class Person : public Introspectable // <--------- !!!
{
    INTROSPECTABLE(Person) // <--------- !!!
public:
    Person();
    Person(const std::string &name, int age, double height);

    void introduce();
    std::string getName() const;
    void setName(const std::string &n);
    int getAge() const;
    void setAge(int a);
    double getHeight() const;
    void setHeight(double h);
    void setNameAndAge(const std::string &, int);
    void setNameAgeAndHeight(const std::string &, int, double);
    std::string getDescription() const;

private:
    std::string name;
    int age;
    double height;
};

// Example usage
int main()
{
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

    auto desc = person.callMethod("getDescription");
    std::cout << "Description: " << std::any_cast<std::string>(desc) << std::endl;

    // Call method with parameters
    person.callMethod("setName", std::vector<std::any>{std::string("Charlie")});
    person.callMethod("introduce");

    person.callMethod("setNameAndAge", std::vector<std::any>{std::string("Toto"), 22});
    person.callMethod("introduce");

    person.callMethod("setNameAgeAndHeight", std::vector<std::any>{std::string("Toto"), 22, 1.74});
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
    for (const auto &name : person.getMemberNames())
    {
        std::cout << name << " ";
    }
    std::cout << "\nAll methods: ";
    for (const auto &name : person.getMethodNames())
    {
        std::cout << name << " ";
    }
    std::cout << std::endl;

    return 0;
}

// ===================================================

Person::Person() : name(""), age(0), height(0.0) {}
Person::Person(const std::string &n, int a, double h) : name(n), age(a), height(h) {}

// Regular methods
void Person::introduce()
{
    std::cout << "Hello, I'm " << name << ", " << age << " years old, "
              << height << "m tall." << std::endl;
}

std::string Person::getName() const { return name; }
void Person::setName(const std::string &n) { name = n; }

int Person::getAge() const { return age; }
void Person::setAge(int a) { age = a; }

double Person::getHeight() const { return height; }
void Person::setHeight(double h) { height = h; }

void Person::setNameAndAge(const std::string &n, int a) {
    setName(n);
    setAge(a);
}

void Person::setNameAgeAndHeight(const std::string &n, int a, double h) {
    setNameAndAge(n, a);
    setHeight(h);
}

std::string Person::getDescription() const
{
    return name + " (" + std::to_string(age) + " years, " +
           std::to_string(height) + "m)";
}

/**
 * @brief Registration implementation using templates (inherited static method)
 */
void Person::registerIntrospection(TypeRegistrar<Person> reg)
{
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
        .method("setNameAndAge", &Person::setNameAndAge)
        .method("setNameAgeAndHeight", &Person::setNameAgeAndHeight)
        .method("getDescription", &Person::getDescription);
}
