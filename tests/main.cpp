#include <iostream>
#include <bits/c++config.h>
#include <string_view>
#include "../include/picojson.hpp"

static JsonParser<2048> parser;
static JsonWriter<512> writer;

void print_used_memory() {
    std::cout << "Parser consumed: " << parser.get_used_memory() << " bytes.\n";
}

int main() {
    std::cout << "--- Memory Footprint Analysis ---\n";
    std::cout << "Pointer Size:         " << sizeof(void*) << " bytes\n";
    std::cout << "Float Size:           " << sizeof(float) << " bytes\n";
    std::cout << "double Size:          " << sizeof(double) << " bytes\n";
    std::cout << "string_view Size:     " << sizeof(std::string_view) << " bytes\n";
    std::cout << "Value Variant Size:   " << sizeof(detail::Value) << " bytes\n";
    std::cout << "KeyValueNode Size:    " << sizeof(detail::KeyValueNode) << " bytes\n";
    std::cout << "ArrayNode Size:       " << sizeof(detail::ArrayNode) << " bytes\n";
    std::cout << "Variant (double, std::string_view, KeyValueNode*) Size:   " << sizeof(std::variant<double, std::string_view, detail::KeyValueNode*>) << " bytes\n";
    std::cout << "Variant (float, std::string_view, KeyValueNode*) Size:    " << sizeof(std::variant<float, std::string_view, detail::KeyValueNode*>) << " bytes\n";
    std::cout << "Variant (double, std::string_view, KeyValueNode*) Alignment:   " << alignof(std::variant<double, std::string_view, detail::KeyValueNode*>) << " bytes\n";
    std::cout << "Variant (float, std::string_view, KeyValueNode*) Alignment:    " << alignof(std::variant<float, std::string_view, detail::KeyValueNode*>) << " bytes\n";
    std::cout << "--- Alignment on Current Architecture ---\n";
    std::cout << "std::string_view Size:      " << sizeof(std::string_view) << " bytes\n";
    std::cout << "std::string_view Alignment: " << alignof(std::string_view) << " bytes\n";
    std::cout << "double Size:                " << sizeof(double) << " bytes\n";
    std::cout << "double Alignment:           " << alignof(double) << " bytes\n";
    std::cout << "int32_t Size:               " << sizeof(int32_t) << " bytes\n";
    std::cout << "int32_t Alignment:          " << alignof(int32_t) << " bytes\n";
    std::cout << "Starting picojson Advanced Stress Tests...\n\n";

    // ==========================================
    // TEST 1: Parsing and Universal Casting
    // ==========================================
    std::cout << "--- [Test 1] Parsing & Extraction ---\n";
    char test_payload[] = R"({"username": "ButaBot", "is_bot": true, "heartbeat": 41250})";
    JsonObject root = parser.parse(test_payload);
    print_used_memory();
    
    std::string_view username = root["username"];
    uint32_t heartbeat = root["heartbeat"];
    std::cout << "Username:  " << username << " (Expected: ButaBot)\n";
    std::cout << "Heartbeat: " << heartbeat << " (Expected: 41250)\n\n";

    // ==========================================
    // TEST 2: Missing or Invalid Keys
    // ==========================================
    std::cout << "--- [Test 2] Missing Keys Fallback ---\n";
    uint32_t missing_number = root["does_not_exist"]; 
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
    char whitespace_payload[] = "   \n\t { \n  \"empty_arr\" :  [   ] \n , \t \"empty_obj\" : { } }  ";
    JsonObject ws_root = parser.parse(whitespace_payload);
    print_used_memory();
    
    JsonArray empty_arr = ws_root["empty_arr"];
    JsonObject empty_obj = ws_root["empty_obj"];
    
    // Testing out of bounds on an empty array
    std::string_view out_of_bounds = empty_arr[0];
    std::cout << "Out of bounds empty array access: " 
              << (out_of_bounds.empty() ? "Safe (Empty)" : out_of_bounds) << "\n";
              
    // Testing missing key on empty object
    uint32_t obj_miss = empty_obj["anything"];
    std::cout << "Empty object key access: " << obj_miss << " (Expected: 0)\n\n";

    // ==========================================
    // TEST 5: Malformed JSON Handling
    // ==========================================
    std::cout << "--- [Test 5] Malformed JSON Handling ---\n";
    // Missing the closing brace for the object
    char broken_payload[] = R"({ "broken_key": "value", "unclosed_array": [1, 2, 3)";
    JsonObject broken_root = parser.parse(broken_payload);
    print_used_memory();
    
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
    
    std::cout << "Tiny Buffer Size Limit: 32" << std::endl;
    std::cout << "Actual Written Length:  " << tiny_buf.length() << std::endl;
    std::cout << "Truncated Output:       " << tiny_buf.data() << "\n" << std::endl;
    
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
                .value(3.45f)
            .end_array()
        .key("key2").value("value2")
        .key("key3").start_object()
                .key("nestedkey1").value(true)
                .key("testing int").value(5)
            .end_object()
    .end_object();
    const auto buf = writer.get_buffer();
    std::cout << "\nGenerated JSON: " << std::string_view(buf.data(), buf.length()) << "\nExpecting nested string in first array to be present.\n";


    std::cout << "\nAll advanced stress tests completed successfully!\n";
    return 0;
}