#include <introspection/introspectable.h>

// Example classes using the introspection system
class Person : public Introspectable {
    INTROSPECTABLE(Person)

  private:
    std::string name;
    int age;
    double height;
    bool isActive;

  public:
    Person() : name(""), age(0), height(0.0), isActive(true) {}
    Person(const std::string &n, int a, double h)
        : name(n), age(a), height(h), isActive(true) {}

    // Getters and setters
    std::string getName() const { return name; }
    void setName(const std::string &n) { name = n; }
    int getAge() const { return age; }
    void setAge(int a) { age = a; }
    double getHeight() const { return height; }
    void setHeight(double h) { height = h; }
    bool getIsActive() const { return isActive; }
    void setIsActive(bool active) { isActive = active; }

    // Methods
    void introduce() const {
        std::cout << "Hi! I'm " << name << ", " << age << " years old, "
                  << height << "m tall." << std::endl;
    }

    void celebrateBirthday() {
        age++;
        std::cout << "🎉 " << name << " is now " << age << " years old!"
                  << std::endl;
    }

    std::string getDescription() const {
        return name + " (" + std::to_string(age) + " years, " +
               std::to_string(height) + "m, " +
               (isActive ? "active" : "inactive") + ")";
    }
};

void Person::registerIntrospection(TypeRegistrar<Person> reg) {
    reg.member("name", &Person::name)
        .member("age", &Person::age)
        .member("height", &Person::height)
        .member("isActive", &Person::isActive)
        .method("getName", &Person::getName)
        .method("setName", &Person::setName)
        .method("getAge", &Person::getAge)
        .method("setAge", &Person::setAge)
        .method("getHeight", &Person::getHeight)
        .method("setHeight", &Person::setHeight)
        .method("getIsActive", &Person::getIsActive)
        .method("setIsActive", &Person::setIsActive)
        .method("introduce", &Person::introduce)
        .method("celebrateBirthday", &Person::celebrateBirthday)
        .method("getDescription", &Person::getDescription);
}

// Another example class
class Vehicle : public Introspectable {
    INTROSPECTABLE(Vehicle)

  private:
    std::string brand;
    std::string model;
    int year;
    double mileage;
    bool isRunning;

  public:
    Vehicle() : brand(""), model(""), year(0), mileage(0.0), isRunning(false) {}
    Vehicle(const std::string &b, const std::string &m, int y)
        : brand(b), model(m), year(y), mileage(0.0), isRunning(false) {}

    // Getters and setters
    std::string getBrand() const { return brand; }
    void setBrand(const std::string &b) { brand = b; }
    std::string getModel() const { return model; }
    void setModel(const std::string &m) { model = m; }
    int getYear() const { return year; }
    void setYear(int y) { year = y; }
    double getMileage() const { return mileage; }
    void setMileage(double m) { mileage = m; }
    bool getIsRunning() const { return isRunning; }

    // Methods
    void start() {
        isRunning = true;
        std::cout << brand << " " << model << " started!" << std::endl;
    }

    void stop() {
        isRunning = false;
        std::cout << brand << " " << model << " stopped!" << std::endl;
    }

    void drive(double miles) {
        if (isRunning) {
            mileage += miles;
            std::cout << "Drove " << miles
                      << " miles. Total mileage: " << mileage << std::endl;
        } else {
            std::cout << "Can't drive - vehicle is not running!" << std::endl;
        }
    }

    std::string getInfo() const {
        return brand + " " + model + " (" + std::to_string(year) + ") - " +
               std::to_string(mileage) + " miles";
    }
};

void Vehicle::registerIntrospection(TypeRegistrar<Vehicle> reg) {
    reg.member("brand", &Vehicle::brand)
        .member("model", &Vehicle::model)
        .member("year", &Vehicle::year)
        .member("mileage", &Vehicle::mileage)
        .member("isRunning", &Vehicle::isRunning)
        .method("getBrand", &Vehicle::getBrand)
        .method("setBrand", &Vehicle::setBrand)
        .method("getModel", &Vehicle::getModel)
        .method("setModel", &Vehicle::setModel)
        .method("getYear", &Vehicle::getYear)
        .method("setYear", &Vehicle::setYear)
        .method("getMileage", &Vehicle::getMileage)
        .method("setMileage", &Vehicle::setMileage)
        .method("getIsRunning", &Vehicle::getIsRunning)
        .method("start", &Vehicle::start)
        .method("stop", &Vehicle::stop)
        .method("drive", &Vehicle::drive)
        .method("getInfo", &Vehicle::getInfo);
}
