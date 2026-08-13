## picojson
This is a tool made for the Raspberry Pi Pico or any very memory constrained device as it uses static memory allocation instead of dynamic. 
It is a single file `picojson.hpp` library that allows for json parsing and object use. 
### Features
* Basic traditional key access.
```c++
double value = object["key"];
```
* Dynamic casting to desired types (if applicable)
```c++
double value = object["decimal_or_int_key"];
```
* Construction of JSON objects with method chaining
```c++
JsonWriter<1024> writer;
writer.clear();
writer.start_object()
    .key("key1").start_array()
            .value(true)
            .value(false)
            .value("nested string")
            .start_array()
                .value("more nested")
                .start_object()
                    .key("further").value("nested")
                .end_object()
            .end_array()
            .value(3.45)
        .end_array()
    .key("key2").value("value2")
    .key("key3").start_object()
            .key("nestedkey1").value(true)
            .key("ints").value(5)
        .end_object()
.end_object();
```
* Deserialization of null-terminated JSON char pointers (must be a mutable char* as manipulation occurs)
```c++
JsonParser<1024> parser;
char payload[] = R"({"key": "value"})";
JsonObject object = parser.parse(payload);
object["key"] // Returns "value"
```
* Serialization of constructed objects.
```c++
JsonWriter<1024> writer;
writer.clear();
writer.start_object()
    .key("key1").value("value1")
.end_object();
const auto buf = writer.get_buffer();
buf.data(); // the char* to the buffer.
buf.length(); // the length of the buffer. 
```