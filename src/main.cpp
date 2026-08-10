#include <iostream>
#include <string_view>
#include "picojson.hpp"

static JsonParser<2048> parser;
static JsonWriter<512> writer;

int main() {
    std::cout << "Starting picojson Stress Tests...\n\n";

    // ==========================================
    // TEST 1: Parsing and Universal Casting
    // ==========================================
    std::cout << "--- [Test 1] Parsing & Extraction ---\n";
    
    // A mock payload simulating a Discord Gateway event
    const char* test_payload = R"({
        "username": "ButaBot",
        "is_bot": true,
        "heartbeat": 41250,
        "intents": null,
        "roles": ["admin", "music"],
        "system": {
            "os": "linux",
            "version": 2.5
        }
    })";

    JsonObject root = parser.parse(test_payload);

    // 2. Extracting data using the new std::visit Returnable casts
    std::string_view username = root["username"];
    bool is_bot = root["is_bot"];
    double heartbeat = root["heartbeat"];
    
    std::cout << "Username:  " << username << " (Expected: ButaBot)\n";
    std::cout << "Is Bot:    " << (is_bot ? "true" : "false") << " (Expected: true)\n";
    std::cout << "Heartbeat: " << heartbeat << " (Expected: 41250)\n";

    // 3. Nested Object test
    JsonObject system = root["system"];
    std::string_view os = system["os"];
    double version = system["version"];
    std::cout << "System OS: " << os << " (Expected: linux)\n";
    std::cout << "Version:   " << version << " (Expected: 2.5)\n";

    // 4. Array test
    JsonArray roles = root["roles"];
    std::string_view role1 = roles[0];
    std::string_view role2 = roles[1];
    std::cout << "Role 0:    " << role1 << " (Expected: admin)\n";
    std::cout << "Role 1:    " << role2 << " (Expected: music)\n\n";

    // ==========================================
    // TEST 2: Missing or Invalid Keys (Safety)
    // ==========================================
    std::cout << "--- [Test 2] Missing Keys Fallback ---\n";
    
    // Requesting a key that does not exist in the JSON
    double missing_number = root["does_not_exist"]; 
    bool missing_bool = root["does_not_exist"];     
    
    std::cout << "Missing Number: " << missing_number << " (Expected: 0)\n";
    std::cout << "Missing Bool:   " << (missing_bool ? "true" : "false") << " (Expected: false)\n\n";

    // ==========================================
    // TEST 3: Zero-DOM Writer Generation
    // ==========================================
    std::cout << "--- [Test 3] JsonWriter Generation ---\n";
    
    writer.start_object()
        .key("op").value(3.0)
        .key("d").start_object()
            .key("status").value("online")
            .key("since").value(0.0)
            .key("afk").value(false)
        .end_object()
    .end_object();

    const auto& buf = writer.get_buffer();
    
    std::cout << "Generated Payload (" << buf.length() << " bytes):\n";
    std::cout << buf.data() << "\n\n";

    std::cout << "All stress tests completed successfully!\n";
    return 0;
}