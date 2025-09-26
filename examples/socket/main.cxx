// main.cxx - Complete example showing how to use WebSocket GUI
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <introspection/introspectable.h>

// Include the WebSocket GUI implementation
#include "websocket_gui.h" // This contains the WebSocketGuiServer class

// Global server pointer for graceful shutdown
WebSocketGuiServer *global_server = nullptr;

// Signal handler for clean shutdown (Ctrl+C)
void signalHandler(int signal)
{
    std::cout << "\nShutting down gracefully..." << std::endl;
    if (global_server)
    {
        global_server->stop();
    }
    exit(0);
}

// Example class that uses introspection
class Person : public Introspectable
{
    INTROSPECTABLE(Person)

private:
    std::string name;
    int age;
    double height;
    bool isActive;

public:
    // Constructors
    Person() : name("Anonymous"), age(0), height(0.0), isActive(true) {}
    Person(const std::string &n, int a, double h) : name(n), age(a), height(h), isActive(true) {}

    // Getters and setters
    std::string getName() const { return name; }
    void setName(const std::string &n) { name = n; }

    int getAge() const { return age; }
    void setAge(int a) { age = a; }

    double getHeight() const { return height; }
    void setHeight(double h) { height = h; }

    bool getIsActive() const { return isActive; }
    void setIsActive(bool active) { isActive = active; }

    // Methods for demonstration
    void introduce()
    {
        std::cout << "Hi! I'm " << name << ", " << age << " years old, "
                  << height << "m tall." << std::endl;
    }

    void celebrateBirthday()
    {
        age++;
        std::cout << "🎉 " << name << " is now " << age << " years old!" << std::endl;
    }

    void grow(double cm)
    {
        height += cm / 100.0;
        std::cout << name << " grew " << cm << "cm! Now " << height << "m tall." << std::endl;
    }

    std::string getInfo() const
    {
        return name + " (" + std::to_string(age) + " years, " +
               std::to_string(height) + "m, " + (isActive ? "active" : "inactive") + ")";
    }

    void toggleActive()
    {
        isActive = !isActive;
        std::cout << name << " is now " << (isActive ? "active" : "inactive") << std::endl;
    }
};

// Register the Person class for introspection
void Person::registerIntrospection(TypeRegistrar<Person> reg)
{
    reg.member("name", &Person::name)
        .member("age", &Person::age)
        .member("height", &Person::height)
        .member("isActive", &Person::isActive)
        .method("introduce", &Person::introduce)
        .method("celebrateBirthday", &Person::celebrateBirthday)
        .method("grow", &Person::grow)
        .method("getInfo", &Person::getInfo)
        .method("toggleActive", &Person::toggleActive)
        .method("getName", &Person::getName)
        .method("setName", &Person::setName)
        .method("getAge", &Person::getAge)
        .method("setAge", &Person::setAge)
        .method("getHeight", &Person::getHeight)
        .method("setHeight", &Person::setHeight)
        .method("getIsActive", &Person::getIsActive)
        .method("setIsActive", &Person::setIsActive);
}

// Function to launch WebSocket GUI
void launchWebSocketGUI(Person &person, int port = 8080)
{
    std::cout << "Starting WebSocket GUI server on port " << port << std::endl;

    // Create the WebSocket server
    // Parameters: (object_pointer, port, refresh_interval_ms)
    WebSocketGuiServer server(&person, port, 500); // 500ms refresh rate

    // Set global reference for signal handler
    global_server = &server;

    // Start the server (this blocks the current thread)
    server.start();
}

// Example 1: Basic usage
void example1_basic()
{
    std::cout << "\n=== Example 1: Basic WebSocket GUI ===" << std::endl;

    Person person("Alice", 25, 1.70);

    std::cout << "Open your browser to: http://localhost:8080" << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;

    launchWebSocketGUI(person);
}

// Example 2: With background thread doing automatic updates
void example2_with_auto_updates()
{
    std::cout << "\n=== Example 2: WebSocket GUI with Auto Updates ===" << std::endl;

    Person person("Bob", 30, 1.75);

    // Start auto-updater thread
    std::thread auto_updater([&person]()
                             {
        std::vector<std::string> names = {"Bob", "Charlie", "Diana", "Eve", "Frank"};
        int name_idx = 0;
        
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait for server to start
        
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            static int counter = 0;
            switch (counter % 4) {
                case 0:
                    person.setName(names[name_idx % names.size()]);
                    name_idx++;
                    std::cout << "[Auto] Changed name" << std::endl;
                    break;
                case 1:
                    person.celebrateBirthday();
                    std::cout << "[Auto] Birthday!" << std::endl;
                    break;
                case 2:
                    person.grow(0.5);
                    std::cout << "[Auto] Growth spurt!" << std::endl;
                    break;
                case 3:
                    person.toggleActive();
                    std::cout << "[Auto] Toggled active status" << std::endl;
                    break;
            }
            counter++;
        } });

    auto_updater.detach();

    std::cout << "Auto-updates will start in 5 seconds..." << std::endl;
    std::cout << "Open your browser to: http://localhost:8080" << std::endl;
    std::cout << "Watch the real-time updates!" << std::endl;

    launchWebSocketGUI(person);
}

// Example 3: Multiple objects (we can extend this)
void example3_multiple_objects()
{
    std::cout << "\n=== Example 3: Multiple Objects ===" << std::endl;

    Person person1("Alice", 25, 1.65);
    Person person2("Bob", 30, 1.80);

    std::cout << "This example shows how you could manage multiple objects" << std::endl;
    std::cout << "(Implementation would require extending the server)" << std::endl;

    // For now, just use the first person
    launchWebSocketGUI(person1);
}

// Example 4: Custom port and settings
void example4_custom_settings()
{
    std::cout << "\n=== Example 4: Custom Port and Settings ===" << std::endl;

    Person person("Charlie", 35, 1.78);

    const int custom_port = 9090;
    const int refresh_rate_ms = 250; // Faster updates

    std::cout << "Using custom port: " << custom_port << std::endl;
    std::cout << "Refresh rate: " << refresh_rate_ms << "ms" << std::endl;
    std::cout << "Open your browser to: http://localhost:" << custom_port << std::endl;

    WebSocketGuiServer server(&person, custom_port, refresh_rate_ms);
    global_server = &server;
    server.start();
}

// Example 5: With console interface running alongside
void example5_console_and_web()
{
    std::cout << "\n=== Example 5: Console + WebSocket GUI ===" << std::endl;

    Person person("Diana", 28, 1.68);

    // Start WebSocket server in background thread
    std::thread web_thread([&person]()
                           {
        WebSocketGuiServer server(&person, 8080, 1000);
        global_server = &server;
        server.start(); });

    // Wait a bit for server to start
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "WebSocket GUI running at: http://localhost:8080" << std::endl;
    std::cout << "Console interface available below:" << std::endl;
    std::cout << "Commands: info, birthday, grow, toggle, name <newname>, quit" << std::endl;

    // Console interface
    std::string command;
    while (true)
    {
        std::cout << "\n> ";
        std::cin >> command;

        if (command == "quit")
        {
            break;
        }
        else if (command == "info")
        {
            std::cout << person.getInfo() << std::endl;
        }
        else if (command == "birthday")
        {
            person.celebrateBirthday();
        }
        else if (command == "grow")
        {
            person.grow(1.0);
        }
        else if (command == "toggle")
        {
            person.toggleActive();
        }
        else if (command == "name")
        {
            std::string newName;
            std::cin >> newName;
            person.setName(newName);
            std::cout << "Name changed to: " << newName << std::endl;
        }
        else
        {
            std::cout << "Unknown command. Available: info, birthday, grow, toggle, name <newname>, quit" << std::endl;
        }
    }

    // Clean shutdown
    if (global_server)
    {
        global_server->stop();
    }
    web_thread.join();
}

int main()
{
    // Register signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "WebSocket GUI Examples" << std::endl;
    std::cout << "Choose an example:" << std::endl;
    std::cout << "1. Basic WebSocket GUI" << std::endl;
    std::cout << "2. With automatic updates" << std::endl;
    std::cout << "3. Multiple objects (demo)" << std::endl;
    std::cout << "4. Custom port and settings" << std::endl;
    std::cout << "5. Console + WebSocket GUI" << std::endl;
    std::cout << "Enter choice (1-5): ";

    int choice;
    std::cin >> choice;

    try
    {
        switch (choice)
        {
        case 1:
            example1_basic();
            break;
        case 2:
            example2_with_auto_updates();
            break;
        case 3:
            example3_multiple_objects();
            break;
        case 4:
            example4_custom_settings();
            break;
        case 5:
            example5_console_and_web();
            break;
        default:
            std::cout << "Invalid choice, running basic example..." << std::endl;
            example1_basic();
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}