#include "picojson.hpp"
#include <iostream>

static JsonParser<1024> parser;

int main() {
    const char* example = R"(
            {
                "key1": "value1",
                "key2": "value2",
                "key3": 123,
                "key4": [1, 2, 3, {"example_key": "example_value"}],
                "key5": 5.3,
                "key6": false,
                "key7": true,
                "key8": null
            }
        )";
    JsonObject test = parser.parse(example);
    // double value = test["key5"];
    // std::string_view value = "example";
    // if (test["key7"]) {
    //     value = "NOT null";
    // } else {
    //     value = "null";
    // }
    // std::string_view value = test["key2"];
    JsonArray nums = test["key4"];
    JsonObject test_object = nums[3];
    std::string_view value = test_object["example_key"];
    std::cout << "Value: " << value << std::endl;
    return 0;
}