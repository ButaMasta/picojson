#include "picojson.hpp"
#include <iostream>

int main() {
    JsonParser<1024> parser;
    const char* example = R"(
            {
                "key1": "value1",
                "key2": "value2",
                "key3": 123
            }
        )";
    JsonObject test = parser.parse(example);
    std::cout << "Value: " << test["key3"] << std::endl;
    return 0;
}