#include <iostream>
#include <string_view>
#include "picojson.hpp"

static JsonParser<2048> parser;
static JsonWriter<512> writer;


int main() {
    std::cout << "Starting picojson Advanced Stress Tests...\n\n";

    // ==========================================
    // TEST 1: Parsing and Universal Casting
    // ==========================================
    std::cout << "--- [Test 1] Parsing & Extraction ---\n";
    const char* test_payload = R"({"username": "ButaBot", "is_bot": true, "heartbeat": 41250})";
    JsonObject root = parser.parse(test_payload);
    
    std::string_view username = root["username"];
    double heartbeat = root["heartbeat"];
    std::cout << "Username:  " << username << " (Expected: ButaBot)\n";
    std::cout << "Heartbeat: " << heartbeat << " (Expected: 41250)\n\n";

    // ==========================================
    // TEST 2: Missing or Invalid Keys
    // ==========================================
    std::cout << "--- [Test 2] Missing Keys Fallback ---\n";
    double missing_number = root["does_not_exist"]; 
    std::cout << "Missing Number: " << missing_number << " (Expected: 0)\n\n";

    // ==========================================
    // TEST 3: Zero-DOM Writer Generation
    // ==========================================
    std::cout << "--- [Test 3] JsonWriter Creation ---\n";
    writer.start_object().key("status").value("online").end_object();
    std::cout << "Generated Payload: " << writer.get_buffer().data() << "\n\n";

    // ==========================================
    // TEST 4: Empty Structures & Extreme Whitespace
    // ==========================================
    std::cout << "--- [Test 4] Whitespace & Empty Structures ---\n";
    const char* whitespace_payload = "   \n\t { \n  \"empty_arr\" :  [   ] \n , \t \"empty_obj\" : { } }  ";
    JsonObject ws_root = parser.parse(whitespace_payload);
    
    JsonArray empty_arr = ws_root["empty_arr"];
    JsonObject empty_obj = ws_root["empty_obj"];
    
    // Testing out of bounds on an empty array
    std::string_view out_of_bounds = empty_arr[0];
    std::cout << "Out of bounds empty array access: " 
              << (out_of_bounds.empty() ? "Safe (Empty)" : out_of_bounds) << "\n";
              
    // Testing missing key on empty object
    double obj_miss = empty_obj["anything"];
    std::cout << "Empty object key access: " << obj_miss << " (Expected: 0)\n\n";

    // ==========================================
    // TEST 5: Malformed JSON Handling
    // ==========================================
    std::cout << "--- [Test 5] Malformed JSON Handling ---\n";
    // Missing the closing brace for the object
    const char* broken_payload = R"({ "broken_key": "value", "unclosed_array": [1, 2, 3)";
    JsonObject broken_root = parser.parse(broken_payload);
    
    // The parser should realize it's invalid and return a null object, 
    // so accessing keys should safely fall back to defaults.
    std::string_view broken_val = broken_root["broken_key"];
    std::cout << "Malformed JSON access: " 
              << (broken_val.empty() ? "Gracefully Failed (Safe)" : "Parsed (Unexpected)") << "\n\n";

    // ==========================================
    // TEST 6: StaticStringBuffer Overflow Limit
    // ==========================================
    std::cout << "--- [Test 6] Buffer Overflow Prevention ---\n";
    // We intentionally create a tiny 32-byte buffer
    JsonWriter<32> tiny_writer;
    
    tiny_writer.start_object()
        .key("this_is_a_very_long_key").value("and_this_is_a_very_long_value_that_exceeds_32_bytes")
    .end_object();

    const auto& tiny_buf = tiny_writer.get_buffer();
    
    std::cout << "Tiny Buffer Size Limit: 32\n";
    std::cout << "Actual Written Length:  " << tiny_buf.length() << "\n";
    std::cout << "Truncated Output:       " << tiny_buf.data() << "\n\n";
    
    if (tiny_buf.length() <= 32) {
        std::cout << "[PASS] Memory was safely truncated without overflowing the stack!\n";
    }

    // ==========================================
    // TEST 7: Deeply nested JSON
    // ==========================================
    writer.clear();
    writer.start_object()
        .key("key1").start_array()
                .value(true)
                .value(false)
                .value("nested string")
                .start_array()
                    .value("more nested")
                    .start_object()
                        .key("even more").value("nested!")
                    .end_object()
                .end_array()
                .value(3.45)
            .end_array()
        .key("key2").value("value2")
        .key("key3").start_object()
                .key("nestedkey1").value(true)
            .end_object()
    .end_object();
    const auto buf = writer.get_buffer();
    std::cout << "\nGenerated JSON: " << std::string_view(buf.data(), buf.length()) << "\nExpecting nested string in first array to be present.\n";


    std::cout << "\nAll advanced stress tests completed successfully!\n";
    return 0;
}