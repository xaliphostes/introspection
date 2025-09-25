// websocket_gui.cxx - WebSocket-enhanced web server implementation
#include <httplib.h>
#include <iostream>
#include <nlohmann/json.hpp>
#include <introspection/introspectable.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <set>
#include <memory>

// Simple JSON helper (you can replace with nlohmann/json or jsoncpp)
class SimpleJson
{
public:
    static std::string object(const std::map<std::string, std::string> &data)
    {
        std::string result = "{";
        bool first = true;
        for (const auto &[key, value] : data)
        {
            if (!first)
                result += ",";
            result += "\"" + key + "\":\"" + value + "\"";
            first = false;
        }
        result += "}";
        return result;
    }

    static std::string array(const std::vector<std::string> &data)
    {
        std::string result = "[";
        for (size_t i = 0; i < data.size(); ++i)
        {
            result += "\"" + data[i] + "\"";
            if (i < data.size() - 1)
                result += ",";
        }
        result += "]";
        return result;
    }
};

class WebSocketGuiServer
{
private:
    httplib::Server server;
    Introspectable *target_object;
    int port;

    // WebSocket connection management
    std::mutex connections_mutex;
    std::set<std::shared_ptr<httplib::websocket::connection>> active_connections;

    // Auto-refresh thread
    std::atomic<bool> running{false};
    std::thread refresh_thread;
    std::chrono::milliseconds refresh_interval{1000}; // 1 second

    // Object state tracking
    std::mutex state_mutex;
    std::string last_state;

public:
    WebSocketGuiServer(Introspectable *obj, int p = 8080, int refresh_ms = 1000)
        : target_object(obj), port(p), refresh_interval(refresh_ms)
    {
        setupRoutes();
    }

    ~WebSocketGuiServer()
    {
        stop();
    }

    void setupRoutes()
    {
        // Serve the main GUI page with WebSocket support
        server.Get("/", [this](const httplib::Request &, httplib::Response &res)
                   {
            std::string html = generateWebSocketPage();
            res.set_content(html, "text/html"); });

        // WebSocket endpoint for real-time communication
        server.set_websocket_upgrade_handler([this](const httplib::Request &req, httplib::Response &res)
                                             {
            std::cout << "WebSocket upgrade request from: " << req.get_header_value("Origin") << std::endl;
            return httplib::websocket_upgrade_result::ACCEPT; });

        server.set_websocket_handler([this](httplib::websocket::connection &conn)
                                     { handleWebSocketConnection(conn); });

        // REST API endpoints (fallback for non-WebSocket clients)
        server.Get("/api/object", [this](const httplib::Request &, httplib::Response &res)
                   {
            std::string json = generateObjectJson();
            res.set_content(json, "application/json");
            res.set_header("Access-Control-Allow-Origin", "*"); });

        // Serve static assets
        server.Get("/style.css", [](const httplib::Request &, httplib::Response &res)
                   { res.set_content(getWebSocketStyleSheet(), "text/css"); });

        server.Get("/websocket.js", [this](const httplib::Request &, httplib::Response &res)
                   { res.set_content(getWebSocketJavaScript(), "application/javascript"); });
    }

    void handleWebSocketConnection(httplib::websocket::connection &conn)
    {
        auto connection = std::shared_ptr<httplib::websocket::connection>(&conn, [](auto *) {});

        // Add to active connections
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            active_connections.insert(connection);
        }

        std::cout << "WebSocket client connected. Total connections: " << active_connections.size() << std::endl;

        // Send initial object state
        sendObjectState(conn);

        // Handle incoming messages
        conn.set_message_handler([this, connection](const std::string &message)
                                 { handleWebSocketMessage(message, *connection); });

        conn.set_close_handler([this, connection]()
                               {
            std::lock_guard<std::mutex> lock(connections_mutex);
            active_connections.erase(connection);
            std::cout << "WebSocket client disconnected. Remaining connections: " << active_connections.size() << std::endl; });
    }

    void handleWebSocketMessage(const std::string &message, httplib::websocket::connection &conn)
    {
        std::cout << "Received WebSocket message: " << message << std::endl;

        try
        {
            // Parse simple JSON-like messages
            // Format: {"type":"update","field":"name","value":"newvalue"}
            // Format: {"type":"method","name":"methodName","args":[]}

            if (message.find("\"type\":\"update\"") != std::string::npos)
            {
                handleUpdateMessage(message, conn);
            }
            else if (message.find("\"type\":\"method\"") != std::string::npos)
            {
                handleMethodMessage(message, conn);
            }
            else if (message.find("\"type\":\"ping\"") != std::string::npos)
            {
                // Respond to ping with pong
                sendMessage(conn, "{\"type\":\"pong\"}");
            }
        }
        catch (const std::exception &e)
        {
            std::string error_msg = "{\"type\":\"error\",\"message\":\"" + std::string(e.what()) + "\"}";
            sendMessage(conn, error_msg);
        }
    }

    void handleUpdateMessage(const std::string &message, httplib::websocket::connection &conn)
    {
        // Simple parsing for: {"type":"update","field":"name","value":"newvalue"}
        std::string field, value;

        auto field_pos = message.find("\"field\":\"");
        if (field_pos != std::string::npos)
        {
            field_pos += 9; // Skip "field":"
            auto field_end = message.find("\"", field_pos);
            field = message.substr(field_pos, field_end - field_pos);
        }

        auto value_pos = message.find("\"value\":\"");
        if (value_pos != std::string::npos)
        {
            value_pos += 9; // Skip "value":"
            auto value_end = message.find("\"", value_pos);
            value = message.substr(value_pos, value_end - value_pos);
        }

        if (!field.empty())
        {
            updateMemberFromString(field, value);

            // Broadcast updated state to all clients
            broadcastObjectState();

            // Send confirmation
            std::string response = "{\"type\":\"update_success\",\"field\":\"" + field + "\"}";
            sendMessage(conn, response);
        }
    }

    void handleMethodMessage(const std::string &message, httplib::websocket::connection &conn)
    {
        // Simple parsing for: {"type":"method","name":"methodName"}
        std::string method_name;

        auto name_pos = message.find("\"name\":\"");
        if (name_pos != std::string::npos)
        {
            name_pos += 8; // Skip "name":"
            auto name_end = message.find("\"", name_pos);
            method_name = message.substr(name_pos, name_end - name_pos);
        }

        if (!method_name.empty())
        {
            auto result = target_object->callMethod(method_name);

            // Broadcast updated state after method call
            broadcastObjectState();

            // Send confirmation
            std::string response = "{\"type\":\"method_success\",\"method\":\"" + method_name + "\"}";
            sendMessage(conn, response);
        }
    }

    void sendMessage(httplib::websocket::connection &conn, const std::string &message)
    {
        conn.send(message);
    }

    void sendObjectState(httplib::websocket::connection &conn)
    {
        std::string state = generateObjectStateMessage();
        sendMessage(conn, state);
    }

    void broadcastObjectState()
    {
        std::string state = generateObjectStateMessage();

        std::lock_guard<std::mutex> lock(connections_mutex);
        for (auto it = active_connections.begin(); it != active_connections.end();)
        {
            try
            {
                (*it)->send(state);
                ++it;
            }
            catch (const std::exception &)
            {
                // Connection is dead, remove it
                it = active_connections.erase(it);
            }
        }
    }

    std::string generateObjectStateMessage()
    {
        const auto &type_info = target_object->getTypeInfo();
        std::string json = "{\"type\":\"state\",\"className\":\"" + type_info.class_name + "\",\"members\":{";

        auto member_names = type_info.getMemberNames();
        for (size_t i = 0; i < member_names.size(); ++i)
        {
            const auto *member = type_info.getMember(member_names[i]);
            auto value = member->getter(target_object);

            json += "\"" + member->name + "\":{";
            json += "\"type\":\"" + member->type_name + "\",";
            json += "\"value\":";

            if (member->type_name == "string")
            {
                json += "\"" + std::any_cast<std::string>(value) + "\"";
            }
            else if (member->type_name == "int")
            {
                json += std::to_string(std::any_cast<int>(value));
            }
            else if (member->type_name == "double")
            {
                json += std::to_string(std::any_cast<double>(value));
            }
            else if (member->type_name == "bool")
            {
                json += (std::any_cast<bool>(value) ? "true" : "false");
            }
            else
            {
                json += "null";
            }

            json += "}";
            if (i < member_names.size() - 1)
                json += ",";
        }

        json += "},\"timestamp\":" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) + "}";

        return json;
    }

    void startAutoRefresh()
    {
        running = true;
        refresh_thread = std::thread([this]()
                                     {
            while (running) {
                std::this_thread::sleep_for(refresh_interval);
                
                if (!active_connections.empty()) {
                    std::string current_state = generateObjectStateMessage();
                    
                    {
                        std::lock_guard<std::mutex> lock(state_mutex);
                        if (current_state != last_state) {
                            last_state = current_state;
                            broadcastObjectState();
                        }
                    }
                }
            } });
    }

    void start()
    {
        std::cout << "Starting WebSocket-enabled web server on http://localhost:" << port << std::endl;
        std::cout << "WebSocket endpoint: ws://localhost:" << port << "/ws" << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;

        startAutoRefresh();

        // Run server (blocking)
        server.listen("0.0.0.0", port);
    }

    void stop()
    {
        running = false;
        if (refresh_thread.joinable())
        {
            refresh_thread.join();
        }
        server.stop();

        // Close all WebSocket connections
        std::lock_guard<std::mutex> lock(connections_mutex);
        for (auto &conn : active_connections)
        {
            try
            {
                conn->close();
            }
            catch (...)
            {
                // Ignore errors during shutdown
            }
        }
        active_connections.clear();
    }

private:
    std::string generateObjectJson()
    {
        // Same as before, kept for REST API compatibility
        const auto &type_info = target_object->getTypeInfo();
        std::string json = "{\"className\": \"" + type_info.class_name + "\",\"members\": {";

        auto member_names = type_info.getMemberNames();
        for (size_t i = 0; i < member_names.size(); ++i)
        {
            const auto *member = type_info.getMember(member_names[i]);
            auto value = member->getter(target_object);

            json += "\"" + member->name + "\": {";
            json += "\"type\": \"" + member->type_name + "\",";
            json += "\"value\": ";

            if (member->type_name == "string")
            {
                json += "\"" + std::any_cast<std::string>(value) + "\"";
            }
            else if (member->type_name == "int")
            {
                json += std::to_string(std::any_cast<int>(value));
            }
            else if (member->type_name == "double")
            {
                json += std::to_string(std::any_cast<double>(value));
            }
            else if (member->type_name == "bool")
            {
                json += (std::any_cast<bool>(value) ? "true" : "false");
            }
            else
            {
                json += "null";
            }

            json += "}";
            if (i < member_names.size() - 1)
                json += ",";
        }

        json += "}}";
        return json;
    }

    void updateMemberFromString(const std::string &field, const std::string &value)
    {
        const auto *member = target_object->getTypeInfo().getMember(field);
        if (!member)
        {
            throw std::runtime_error("Unknown field: " + field);
        }

        if (member->type_name == "string")
        {
            target_object->setMemberValue(field, value);
        }
        else if (member->type_name == "int")
        {
            target_object->setMemberValue(field, std::stoi(value));
        }
        else if (member->type_name == "double")
        {
            target_object->setMemberValue(field, std::stod(value));
        }
        else if (member->type_name == "bool")
        {
            target_object->setMemberValue(field, value == "true" || value == "1");
        }
    }

    std::string generateWebSocketPage()
    {
        const auto &type_info = target_object->getTypeInfo();
        std::string html = R"(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)" + type_info.class_name +
                           R"( Editor (WebSocket)</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>)" + type_info.class_name +
                           R"( Editor</h1>
            <div class="connection-status">
                <span id="connectionStatus" class="status-disconnected">Disconnected</span>
                <span id="lastUpdate">Never</span>
            </div>
        </div>
        
        <div class="section">
            <h2>Properties <span class="live-badge">LIVE</span></h2>
            <div id="propertiesForm">)";

        // Generate form fields for members
        auto member_names = type_info.getMemberNames();
        for (const auto &name : member_names)
        {
            const auto *member = type_info.getMember(name);
            html += R"(
                <div class="field">
                    <label for=")" +
                    name + R"(">)" + name + R"( <span class="type-hint">()" + member->type_name + R"()</span>:</label>)";

            if (member->type_name == "string")
            {
                html += R"(<input type="text" id=")" + name + R"(" name=")" + name + R"(" data-type="string">)";
            }
            else if (member->type_name == "int")
            {
                html += R"(<input type="number" id=")" + name + R"(" name=")" + name + R"(" data-type="int">)";
            }
            else if (member->type_name == "double")
            {
                html += R"(<input type="number" step="0.01" id=")" + name + R"(" name=")" + name + R"(" data-type="double">)";
            }
            else if (member->type_name == "bool")
            {
                html += R"(<input type="checkbox" id=")" + name + R"(" name=")" + name + R"(" data-type="bool">)";
            }

            html += R"(
                </div>)";
        }

        html += R"(
            </div>
        </div>
        
        <div class="section">
            <h2>Methods</h2>
            <div class="methods" id="methodsContainer">)";

        // Generate method buttons
        auto method_names = type_info.getMethodNames();
        for (const auto &name : method_names)
        {
            html += R"(<button class="method-btn" data-method=")" + name + R"(">)" + name + R"(()</button>)";
        }

        html += R"(
            </div>
        </div>
        
        <div class="section">
            <h2>Activity Log</h2>
            <div id="activityLog" class="activity-log"></div>
        </div>
    </div>
    
    <script src="/websocket.js"></script>
</body>
</html>)";

        return html;
    }

    static std::string getWebSocketStyleSheet()
    {
        return R"(
body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    max-width: 900px;
    margin: 0 auto;
    padding: 20px;
    background-color: #f5f5f5;
}

.container {
    background: white;
    border-radius: 8px;
    box-shadow: 0 2px 10px rgba(0,0,0,0.1);
    padding: 30px;
}

.header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 30px;
    border-bottom: 2px solid #007cba;
    padding-bottom: 15px;
}

.connection-status {
    text-align: right;
}

.status-connected {
    color: #28a745;
    font-weight: bold;
}

.status-disconnected {
    color: #dc3545;
    font-weight: bold;
}

.status-connecting {
    color: #ffc107;
    font-weight: bold;
}

h1, h2 {
    color: #333;
    margin: 0;
}

.live-badge {
    background: #28a745;
    color: white;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 0.8em;
    font-weight: normal;
    animation: pulse 2s infinite;
}

@keyframes pulse {
    0% { opacity: 1; }
    50% { opacity: 0.7; }
    100% { opacity: 1; }
}

.section {
    margin: 30px 0;
}

.field {
    margin: 15px 0;
    display: flex;
    align-items: center;
    transition: background-color 0.3s;
}

.field.field-updated {
    background-color: #d4edda;
    border-radius: 4px;
    padding: 5px;
}

.type-hint {
    color: #6c757d;
    font-size: 0.9em;
    font-weight: normal;
}

label {
    display: block;
    margin-bottom: 5px;
    font-weight: bold;
    color: #555;
    min-width: 200px;
}

input {
    padding: 10px;
    border: 2px solid #ddd;
    border-radius: 4px;
    font-size: 14px;
    flex: 1;
    max-width: 300px;
    transition: border-color 0.3s, box-shadow 0.3s;
}

input:focus {
    border-color: #007cba;
    box-shadow: 0 0 0 3px rgba(0, 123, 186, 0.1);
}

input.input-updated {
    border-color: #28a745;
    box-shadow: 0 0 0 3px rgba(40, 167, 69, 0.1);
}

input[type='checkbox'] {
    transform: scale(1.2);
    max-width: none;
}

.methods {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
}

.method-btn {
    background: #28a745;
    color: white;
    border: none;
    padding: 10px 15px;
    border-radius: 4px;
    cursor: pointer;
    transition: background-color 0.3s, transform 0.1s;
}

.method-btn:hover {
    background: #1e7e34;
    transform: translateY(-1px);
}

.method-btn:active {
    transform: translateY(0);
}

.method-btn.method-called {
    background: #17a2b8;
    animation: methodCall 0.5s ease;
}

@keyframes methodCall {
    0% { transform: scale(1); }
    50% { transform: scale(1.05); }
    100% { transform: scale(1); }
}

.activity-log {
    background: #f8f9fa;
    border: 1px solid #dee2e6;
    border-radius: 4px;
    padding: 15px;
    height: 200px;
    overflow-y: auto;
    font-family: 'Courier New', monospace;
    font-size: 0.9em;
}

.log-entry {
    margin: 5px 0;
    padding: 2px 0;
    border-bottom: 1px solid #e9ecef;
}

.log-timestamp {
    color: #6c757d;
    margin-right: 10px;
}

.log-update {
    color: #007cba;
}

.log-method {
    color: #28a745;
}

.log-error {
    color: #dc3545;
    font-weight: bold;
}

.log-connection {
    color: #6f42c1;
}
)";
    }

    static std::string getWebSocketJavaScript()
    {
        return R"(
class WebSocketGUI {
    constructor() {
        this.ws = null;
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 5;
        this.reconnectDelay = 1000;
        this.isUpdatingFromServer = false;
        this.heartbeatInterval = null;
        this.connectionTimeout = null;
        this.inputDebounceTimers = new Map();
        
        this.init();
    }
    
    init() {
        this.setupEventListeners();
        this.connect();
        this.setupVisibilityHandler();
    }
    
    connect() {
        if (this.ws && this.ws.readyState === WebSocket.CONNECTING) {
            return; // Already connecting
        }
        
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const wsUrl = `${protocol}//${window.location.host}/`;
        
        this.updateConnectionStatus('connecting');
        this.log('connection', `Connecting to ${wsUrl}...`);
        
        try {
            this.ws = new WebSocket(wsUrl);
            this.setupWebSocketHandlers();
            this.startConnectionTimeout();
        } catch (error) {
            this.log('error', `Failed to create WebSocket: ${error.message}`);
            this.scheduleReconnect();
        }
    }
    
    setupWebSocketHandlers() {
        this.ws.onopen = () => {
            this.clearConnectionTimeout();
            this.reconnectAttempts = 0;
            this.updateConnectionStatus('connected');
            this.log('connection', 'WebSocket connected successfully');
            this.startHeartbeat();
            
            // Request initial state
            this.sendMessage({ type: 'ping' });
        };
        
        this.ws.onmessage = (event) => {
            try {
                const message = JSON.parse(event.data);
                this.handleMessage(message);
            } catch (error) {
                this.log('error', `Invalid JSON received: ${error.message}`);
                console.error('Raw message:', event.data);
            }
        };
        
        this.ws.onclose = (event) => {
            this.clearConnectionTimeout();
            this.stopHeartbeat();
            
            const reason = event.reason || 'Unknown reason';
            const code = event.code || 'Unknown code';
            
            this.updateConnectionStatus('disconnected');
            this.log('connection', `Connection closed: ${code} - ${reason}`);
            
            if (event.code !== 1000) { // Not a normal closure
                this.scheduleReconnect();
            }
        };
        
        this.ws.onerror = (error) => {
            this.log('error', 'WebSocket error occurred');
            console.error('WebSocket error:', error);
        };
    }
    
    startConnectionTimeout() {
        this.connectionTimeout = setTimeout(() => {
            if (this.ws && this.ws.readyState === WebSocket.CONNECTING) {
                this.log('error', 'Connection timeout');
                this.ws.close();
            }
        }, 10000); // 10 second timeout
    }
    
    clearConnectionTimeout() {
        if (this.connectionTimeout) {
            clearTimeout(this.connectionTimeout);
            this.connectionTimeout = null;
        }
    }
    
    startHeartbeat() {
        this.heartbeatInterval = setInterval(() => {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) {
                this.sendMessage({ type: 'ping' });
            }
        }, 30000); // Every 30 seconds
    }
    
    stopHeartbeat() {
        if (this.heartbeatInterval) {
            clearInterval(this.heartbeatInterval);
            this.heartbeatInterval = null;
        }
    }
    
    scheduleReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            this.log('error', 'Max reconnection attempts reached. Please refresh the page.');
            this.updateConnectionStatus('failed');
            return;
        }
        
        this.reconnectAttempts++;
        const delay = Math.min(this.reconnectDelay * Math.pow(2, this.reconnectAttempts - 1), 30000);
        
        this.log('connection', `Reconnecting in ${delay/1000}s... (attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`);
        
        setTimeout(() => {
            this.connect();
        }, delay);
    }
    
    sendMessage(message) {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            try {
                const jsonMessage = JSON.stringify(message);
                this.ws.send(jsonMessage);
                return true;
            } catch (error) {
                this.log('error', `Failed to send message: ${error.message}`);
                return false;
            }
        } else {
            this.log('error', 'Cannot send message: WebSocket not connected');
            return false;
        }
    }
    
    handleMessage(message) {
        switch (message.type) {
            case 'state':
                this.updateObjectState(message);
                break;
            case 'update_success':
                this.log('update', `✓ Updated ${message.field}`);
                this.highlightField(message.field, 'success');
                break;
            case 'method_success':
                this.log('method', `✓ Called ${message.method}()`);
                this.highlightMethod(message.method);
                break;
            case 'error':
                this.log('error', `✗ ${message.message}`);
                break;
            case 'pong':
                // Heartbeat response - connection is alive
                console.debug('Heartbeat received');
                break;
            default:
                console.warn('Unknown message type:', message.type, message);
        }
    }
    
    updateObjectState(state) {
        this.isUpdatingFromServer = true;
        
        try {
            for (const [memberName, memberInfo] of Object.entries(state.members)) {
                this.updateInputField(memberName, memberInfo);
            }
            
            this.updateLastUpdateTime(state.timestamp);
        } catch (error) {
            this.log('error', `Failed to update UI state: ${error.message}`);
        } finally {
            this.isUpdatingFromServer = false;
        }
    }
    
    updateInputField(memberName, memberInfo) {
        const input = document.getElementById(memberName);
        if (!input) {
            console.warn(`Input field '${memberName}' not found`);
            return;
        }
        
        const currentValue = this.getInputValue(input);
        const newValue = memberInfo.value;
        
        // Only update if value has actually changed
        if (!this.valuesEqual(currentValue, newValue)) {
            this.setInputValue(input, newValue);
            this.highlightField(memberName, 'updated');
        }
    }
    
    valuesEqual(a, b) {
        // Handle type coercion and comparison
        if (typeof a === 'boolean' || typeof b === 'boolean') {
            return Boolean(a) === Boolean(b);
        }
        if (typeof a === 'number' || typeof b === 'number') {
            return Number(a) === Number(b);
        }
        return String(a) === String(b);
    }
    
    getInputValue(input) {
        switch (input.type) {
            case 'checkbox':
                return input.checked;
            case 'number':
                if (input.dataset.type === 'int') {
                    return parseInt(input.value) || 0;
                } else if (input.dataset.type === 'double') {
                    return parseFloat(input.value) || 0.0;
                }
                return parseFloat(input.value) || 0;
            default:
                return input.value;
        }
    }
    
    setInputValue(input, value) {
        switch (input.type) {
            case 'checkbox':
                input.checked = Boolean(value);
                break;
            case 'number':
                input.value = value;
                break;
            default:
                input.value = String(value);
        }
    }
    
    setupEventListeners() {
        // Handle form input changes
        document.addEventListener('change', (event) => {
            if (this.isUpdatingFromServer || !event.target.name) return;
            
            const target = event.target;
            if (target.tagName === 'INPUT') {
                this.updateMember(target.name, this.getInputValue(target));
            }
        });
        
        // Handle real-time input for text fields with debouncing
        document.addEventListener('input', (event) => {
            if (this.isUpdatingFromServer) return;
            
            const target = event.target;
            if (target.tagName === 'INPUT' && target.type === 'text') {
                this.debouncedUpdateMember(target.name, target.value);
            }
        });
        
        // Handle method button clicks
        document.addEventListener('click', (event) => {
            if (event.target.classList.contains('method-btn')) {
                event.preventDefault();
                const methodName = event.target.dataset.method;
                if (methodName) {
                    this.callMethod(methodName);
                }
            }
        });
        
        // Handle keyboard shortcuts
        document.addEventListener('keydown', (event) => {
            if (event.ctrlKey && event.key === 'r') {
                event.preventDefault();
                this.reconnect();
            }
        });
    }
    
    setupVisibilityHandler() {
        // Reconnect when tab becomes visible again (handles sleep/hibernate)
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden && this.ws && this.ws.readyState !== WebSocket.OPEN) {
                this.log('connection', 'Page became visible, attempting reconnection...');
                this.reconnect();
            }
        });
    }
    
    debouncedUpdateMember(fieldName, value) {
        // Clear existing timer for this field
        if (this.inputDebounceTimers.has(fieldName)) {
            clearTimeout(this.inputDebounceTimers.get(fieldName));
        }
        
        // Set new timer
        const timer = setTimeout(() => {
            this.updateMember(fieldName, value);
            this.inputDebounceTimers.delete(fieldName);
        }, 750); // 750ms debounce
        
        this.inputDebounceTimers.set(fieldName, timer);
    }
    
    updateMember(fieldName, value) {
        const message = {
            type: 'update',
            field: fieldName,
            value: String(value)
        };
        
        if (this.sendMessage(message)) {
            this.log('update', `→ Setting ${fieldName} = ${value}`);
        }
    }
    
    callMethod(methodName) {
        const message = {
            type: 'method',
            name: methodName,
            args: []
        };
        
        if (this.sendMessage(message)) {
            this.log('method', `→ Calling ${methodName}()`);
        }
    }
    
    reconnect() {
        this.log('connection', 'Manual reconnection requested');
        if (this.ws) {
            this.ws.close();
        }
        this.reconnectAttempts = 0;
        setTimeout(() => this.connect(), 100);
    }
    
    updateConnectionStatus(status) {
        const statusElement = document.getElementById('connectionStatus');
        if (statusElement) {
            const statusText = status.charAt(0).toUpperCase() + status.slice(1);
            statusElement.textContent = statusText;
            statusElement.className = `status-${status}`;
        }
        
        // Update page title to show connection status
        const originalTitle = document.title.replace(/ \[.*\]/, '');
        if (status === 'connected') {
            document.title = originalTitle + ' [LIVE]';
        } else if (status === 'disconnected' || status === 'failed') {
            document.title = originalTitle + ' [OFFLINE]';
        } else if (status === 'connecting') {
            document.title = originalTitle + ' [CONNECTING]';
        }
    }
    
    updateLastUpdateTime(timestamp) {
        const timeElement = document.getElementById('lastUpdate');
        if (timeElement) {
            const date = timestamp ? new Date(timestamp) : new Date();
            timeElement.textContent = 'Last update: ' + date.toLocaleTimeString();
        }
    }
    
    highlightField(fieldName, type = 'updated') {
        const field = document.getElementById(fieldName);
        if (!field) return;
        
        const classNames = {
            'updated': 'input-updated',
            'success': 'input-success',
            'error': 'input-error'
        };
        
        const className = classNames[type] || classNames['updated'];
        
        field.classList.add(className);
        field.parentElement.classList.add('field-' + type);
        
        setTimeout(() => {
            field.classList.remove(className);
            field.parentElement.classList.remove('field-' + type);
        }, type === 'success' ? 1000 : 2000);
    }
    
    highlightMethod(methodName) {
        const methodBtn = document.querySelector(`[data-method="${methodName}"]`);
        if (methodBtn) {
            methodBtn.classList.add('method-called');
            
            // Create ripple effect
            const ripple = document.createElement('span');
            ripple.classList.add('ripple');
            methodBtn.appendChild(ripple);
            
            setTimeout(() => {
                methodBtn.classList.remove('method-called');
                if (ripple.parentNode) {
                    ripple.parentNode.removeChild(ripple);
                }
            }, 600);
        }
    }
    
    log(type, message) {
        const logContainer = document.getElementById('activityLog');
        if (!logContainer) return;
        
        const timestamp = new Date().toLocaleTimeString();
        const logEntry = document.createElement('div');
        logEntry.className = `log-entry log-${type}`;
        
        // Add icon based on type
        const icons = {
            'connection': '🔗',
            'update': '📝',
            'method': '⚡',
            'error': '❌',
            'info': 'ℹ️'
        };
        
        const icon = icons[type] || '•';
        
        logEntry.innerHTML = `
            <span class="log-icon">${icon}</span>
            <span class="log-timestamp">${timestamp}</span>
            <span class="log-message">${this.escapeHtml(message)}</span>
        `;
        
        logContainer.appendChild(logEntry);
        logContainer.scrollTop = logContainer.scrollHeight;
        
        // Keep only last 100 entries
        while (logContainer.children.length > 100) {
            logContainer.removeChild(logContainer.firstChild);
        }
        
        // Also log to console for debugging
        console.log(`[${type.toUpperCase()}] ${message}`);
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
    
    // Public API methods
    getConnectionState() {
        if (!this.ws) return 'disconnected';
        
        switch (this.ws.readyState) {
            case WebSocket.CONNECTING: return 'connecting';
            case WebSocket.OPEN: return 'connected';
            case WebSocket.CLOSING: return 'disconnecting';
            case WebSocket.CLOSED: return 'disconnected';
            default: return 'unknown';
        }
    }
    
    isConnected() {
        return this.ws && this.ws.readyState === WebSocket.OPEN;
    }
    
    disconnect() {
        this.stopHeartbeat();
        this.clearConnectionTimeout();
        
        if (this.ws) {
            this.ws.close(1000, 'User requested disconnect');
        }
        
        // Clear all debounce timers
        this.inputDebounceTimers.forEach(timer => clearTimeout(timer));
        this.inputDebounceTimers.clear();
        
        this.log('connection', 'Disconnected by user');
    }
}

// Global WebSocket GUI instance
let wsGui = null;

// Initialize when page loads
document.addEventListener('DOMContentLoaded', () => {
    wsGui = new WebSocketGUI();
    
    // Expose to global scope for debugging
    window.wsGui = wsGui;
    
    // Add debug info to console
    console.log('WebSocket GUI initialized');
    console.log('Available commands: wsGui.reconnect(), wsGui.disconnect(), wsGui.getConnectionState()');
    
    // Show connection help message
    setTimeout(() => {
        if (!wsGui.isConnected()) {
            console.warn('Connection failed. Try: wsGui.reconnect() or press Ctrl+R');
        }
    }, 5000);
});

// Cleanup on page unload
window.addEventListener('beforeunload', () => {
    if (wsGui) {
        wsGui.disconnect();
    }
});
)";
    }
};