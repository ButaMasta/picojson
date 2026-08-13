// picojson.hpp
#include <cstddef>
#include <cctype>
#include <cstring>
#include <string_view>
#include <variant>
#include <charconv>
#include <cstdint>
#include <type_traits>


class JsonObject;
class JsonArray;

/**
 * @brief This is the class allowing for normal string appending and use but only 
 * using static memory instead of dynamically allocated memory.
 * 
 * @tparam capacity The max capacity of the contained string.
 */
template <size_t Capacity>
class StaticStringBuffer {
public:
    StaticStringBuffer() {
        // Start with a null terminator.
        if (Capacity > 0) data_[0] = '\0';
    }

    // Appending a single character.
    StaticStringBuffer& operator+=(char c) {
        // Check if there is space for the character and the null terminator.
        if (Capacity > 0 && size_ < (Capacity - 1)) {
            data_[size_++] = c;
            data_[size_] = '\0'; // Always append a null terminator.
        }
        return *this;
    }

    // Append a null-terminated string.
    StaticStringBuffer& operator+=(const char* str) {
        if (str) {
            append(str, std::strlen(str));
        }
        return *this;
    }

    // Appending a char buffer with len.
    void append(const char* str, size_t len) {
        // Early exit.
        if (Capacity == 0 || size_ >= (Capacity - 1) || str == nullptr || len == 0) {
            return;
        }

        // Calculate available space, reserving 1 byte for null-terminator.
        size_t available = (Capacity - 1) - size_;
        size_t to_copy = (len < available) ? len : available;

        std::memcpy(data_ + size_, str, to_copy);
        size_ += to_copy;
        data_[size_] = '\0'; // Always append a null terminator.
    }

    // Clears the buffer for use.
    void clear() {
        size_ = 0;
        if (Capacity > 0) data_[0] = '\0';
    }

    const char* data() const { return data_; }
    size_t length() const { return size_; }
    size_t size() const { return size_; }

private:
    char data_[Capacity];
    size_t size_ = 0;
};

/**
 * @brief This is the namespace containing the core elements of the functionality 
 * that need not be exposed.
 */
namespace detail {
    // Class tools
    struct KeyValueNode;
    struct ArrayNode;
    
    using Value = 
        std::variant<
            std::monostate,     // Represents JSON 'null'.
            KeyValueNode*,      // Represents JSON object.
            ArrayNode*,         // Represents JSON array.
            std::string_view,   // Represents JSON string.
            double,             // Represents JSON number.
            bool                // Represents JSON boolean.
        >;

    /**
     * @brief This is a struct for the generic KV pairs.
     */
    struct KeyValueNode {
        std::string_view key;
        Value value;
        KeyValueNode* next;
    };

    /**
     * @brief This is a struct for housing specifically arrays.
     */
    struct ArrayNode {
        Value value;
        ArrayNode* next;
    };

    /**
     * @brief This is a generic returnable class allowing the Value type to be returned
     * as any of its variant types properly.
     */
    class Returnable {
    public:
        Returnable(Value value) : value_(value) {}
        Returnable() {
            value_ = std::monostate();
        }
        ~Returnable() = default;

        /**
         * @brief A lambda function for converting any type within the variant to 
         * a desired cast type, if possible.
         * 
         * This is confusing but it's extremely helpful. Essentially this is just saying
         * for any casting type `T`, check what the actual type of the argument attempting
         * to be cast is `ActualT` and compare against `T`. If it matches then we return the
         * expected reference or value. If it does not match then we will just return the
         * most basic version of the expected type or a null pointer.
         * 
         * @tparam T The type attempting to cast to.
         * @return T - The type actually cast to, this will always match tparam T's type regardless
         * of a successful type identification.
         */
        template <typename T>
        operator T() const {
            return std::visit([](auto&& arg) -> T {
                using ActualT = std::decay_t<decltype(arg)>;

                // Direct conversions: double, bool, string_view.
                if constexpr (std::is_same_v<ActualT, T>) {
                    return arg;
                }
                // Automatically cast a parsed double into any requested integer type.
                else if constexpr (std::is_same_v<ActualT, double> && std::is_integral_v<T>) {
                    return static_cast<T>(arg);
                }
                // Handles returning a JSON Object pointer. 
                else if constexpr (std::is_same_v<ActualT, KeyValueNode*> && std::is_same_v<T, JsonObject>) {
                    return JsonObject(arg);
                }
                // Handles returning a JSON Array pointer.
                else if constexpr (std::is_same_v<ActualT, ArrayNode*> && std::is_same_v<T, JsonArray>) {
                    return JsonArray(arg);
                }
                // Return default type if there is not match.
                return T{};
            }, value_);
        }

    private:
        Value value_;
    };


    inline bool is_number_char(char c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '.';
    }

    template <size_t C>
    inline void serialize_value(const Value& val, StaticStringBuffer<C>& out);

    template <size_t C>
    inline void serialize_object(const KeyValueNode* node, StaticStringBuffer<C>& out) {
        out += '{';
        const KeyValueNode* current = node;
        while (current) {
            out += '"';
            out.append(current->key.data(), current->key.length());
            out += "\":\"";
            serialize_value(current->value, out);
            current = current->next;
            if (current) out += ',';
        }
        out += '}';
    }

    template <size_t C>
    inline void serialize_array(const ArrayNode* node, StaticStringBuffer<C>& out) {
        out += '[';
        const ArrayNode* current = node;
        while (current) {
            serialize_value(current->value, out);
            current = current->next;
            if (current) out += ',';
        }
        out += ']';
    }

    template <size_t C>
    inline void serialize_value(const Value& val, StaticStringBuffer<C>& out) {
        std::visit([&out](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, std::monostate>) {
                out += "null";
            } else if constexpr (std::is_same_v<T, KeyValueNode*>) {
                serialize_object(arg, out);
            } else if constexpr (std::is_same_v<T, ArrayNode*>) {
                serialize_array(arg, out);
            } else if constexpr (std::is_same_v<T, std::string_view>) {
                out += '"';
                out.append(arg.data(), arg.length());
                out += '"';
            } else if constexpr (std::is_same_v<T, double>) {
                char buf[32];
                auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), arg);
                out.append(buf, ptr - buf);
            } else if constexpr (std::is_same_v<T, bool>) {
                out += arg ? "true" : "false";
            }
        }, val);
    }

}


/**
 * @brief The standard object representation of a JSON string. 
 * Values are referencable with the use of the `[]` characters.
 */
class JsonObject {
public:
    JsonObject(detail::KeyValueNode* root = nullptr) : root_(root) {}
    ~JsonObject() = default;

    detail::Returnable operator[](const char* key) const {
        if (!root_) {
            return detail::Returnable();
        }
        detail::KeyValueNode* current = root_;
        while (current) {
            if (current->key == key) {
                return detail::Returnable(current->value);
            }
            current = current->next;
        }
        return detail::Returnable();
    }

private:
    // The root KeyValueNode for the linked list.
    detail::KeyValueNode* root_;
};

/**
 * @brief The JSON method of representing an array. This is separate from JSON object due to 
 * the lack of a key and thus saving space.
 */
class JsonArray {
public:
    JsonArray(detail::ArrayNode* root = nullptr) : root_(root) {}
    ~JsonArray() = default;

    /**
     * @brief Allows for standary array access ex: value = array[index].
     * 
     * @param index The index to go to.
     * @return detail::Returnable The value returned in the wrapper class to become the correct type.
     */
    detail::Returnable operator[](size_t index) const {
        detail::ArrayNode* current = root_;
        size_t i = 0;
        while ( current && i < index) {
            current = current->next;
            i++;
        }
        if (current) {
            return detail::Returnable(current->value);
        }
        return detail::Returnable(); // Return null if out of bounds.
    }

    /**
     * @brief An iterator to allow for the use of for-each loops.
     */
    class Iterator {
    public:
        Iterator(detail::ArrayNode* node) : current_(node) {}

        bool operator!=(const Iterator& other) const {
            return current_ != other.current_;
        }

        Iterator& operator++() {
            if (current_) {
                current_ = current_->next;
            }
            return *this;
        }

        detail::Returnable operator*() const {
            return detail::Returnable(current_->value);
        }

    private:
        detail::ArrayNode* current_;
    };

    Iterator begin() const {
        return Iterator(root_);
    }

    Iterator end() const {
        return Iterator(nullptr);
    }

private:
    detail::ArrayNode* root_;
};


/**
 * @brief The core parser handling the conversion of a JSON string to a 
 * JsonObject.
 * 
 * @tparam MaxBytes The maximum bytes this parser is allowed to use on the
 * stack. There is zero dynamic allocation so this value should be at least
 * 1024 or higher for most cases.
 */
template <std::size_t MaxBytes>
class JsonParser {
public:
    JsonParser() = default;

    /**
     * @brief Parses a json string provided by the user and returns the root 
     * JsonValue. 
     * 
     * @param json_str This must be kept alive for the duration this object 
     * is being used.
     * @return detail::Returnable - The generic root value of a JSON payload.
     */
    JsonObject parse(char* json_str) {
        cursor_ = json_str;
        pool_offset_ = 0;
        skip_whitespace();

        return detail::Returnable(parse_value());
    }

    // Helper function to see memory used when parsing, useful for tuning the buffer.
    size_t get_used_memory() const { return pool_offset_; }

private:
    alignas(std::max_align_t) std::byte pool_[MaxBytes];
    size_t pool_offset_ = 0;
    char* cursor_;
    
    /**
     * @brief Generic allocator to reserve space in the pool.
     * 
     * @tparam T The node type, KV or Array.
     * @return T* - The new pointer to the node.
     */
    template<typename T>
    inline T* alloc_node() {
        if (pool_offset_ + sizeof(T) > MaxBytes) {
            return nullptr; // Out of memory.
        }
        T* node = reinterpret_cast<T*>(&pool_[pool_offset_]);
        pool_offset_ += sizeof(T);
        return node;
    }

    /**
     * @brief Skips any whitespace characters in the current string.
     */
    inline void skip_whitespace() {
        while (*cursor_ && isspace(*cursor_)) { cursor_++; }
    }

    /**
     * @brief Finds the next token from the current: {, ", [, etc.
     */
    inline void next_token() {
        if (*cursor_ != '\0') {
            cursor_++;
        }
        skip_whitespace();
    }


    /**
     * @brief Parses a string from within the json string.
     * 
     * @return std::string_view The zero copy reference to the parsed string.
     */
    std::string_view parse_string() {
        if (*cursor_ != '"') { return {}; }
        cursor_++; // Skip the quote.
        
        char* start = cursor_;  // Write pointer.
        char* read_ptr = cursor_;   // Read pointer.

        while (*read_ptr && *read_ptr != '"') {
            if (*read_ptr == '\\' && *(read_ptr + 1) != '\0') {
                read_ptr++; // Skip the backslash.
                switch (*read_ptr) {
                    case '"': *start++ = '"'; break;
                    case '\\': *start++ = '\\'; break;
                    case 'n': *start++ = '\n'; break;
                    case 'r': *start++ = '\r'; break;
                    case 't': *start++ = '\t'; break;
                    default: *start++ = *read_ptr; break;
                }
            } else {
                // Normal character. Copy it over.
                *start++ = *read_ptr;
            }
            read_ptr++;
        }
        cursor_ = read_ptr;

        return std::string_view(cursor_ - (read_ptr - start), static_cast<size_t>(start - (cursor_ - (read_ptr - start))));
    }


    /**
     * @brief Parses a double from the json string
     * 
     * @return double - The parsed value
     */
    double parse_double() {
        const char* start = cursor_;
        while (detail::is_number_char(*cursor_)) {
            cursor_++;
        }
        double value{};
        std::from_chars(start, cursor_, value);
        cursor_--;  // Ensure next_token does not skip a token.
        return value;
    }


    /**
     * @brief Helper function to match string literals "true", "false", and "null".
     * 
     * @return true - If the literal was found.
     * @return false - If the literal was not found.
     */
    bool match_literal(const char* literal, size_t len) {
        if (std::strncmp(cursor_, literal, len) == 0) {
            cursor_ += (len - 1);
            return true;
        }
        return false;
    }


    /**
     * @brief Parses a json value from the json string.
     * 
     * @return detail::Value The value struct for assignment to a KeyValueNode.
     */
    detail::Value parse_value() {
        detail::Value value;
        if (*cursor_ == '{') {
            value = parse_object();
        } else if (*cursor_ == '[') {
            value = parse_array();
        } else if (*cursor_ == '"') {
            value = parse_string();
        } else if (detail::is_number_char(*cursor_)) {
            value = parse_double();
        } else if (*cursor_ == 't' && match_literal("true", 4)) {
            value = true;
        } else if (*cursor_ == 'f' && match_literal("false", 5)) {
            value = false;
        } else if (*cursor_ == 'n' && match_literal("null", 4)) {
            value = std::monostate();
        } else {
            value = std::monostate();
        }
        return value;
    }


    /**
     * @brief Parses an entire object looping over all key value pairs.
     * 
     * @return detail::KeyValueNode* The KeyValueNode head reference to 
     * the object.
     */
    detail::KeyValueNode* parse_object() {
        // Make sure cursor is at the start of an object.
        if (*cursor_ != '{') return nullptr;
        next_token();

        // If this is an empty object return as such.
        if (*cursor_ == '}') return nullptr;

        // Object to parse.
        detail::KeyValueNode* root = alloc_node<detail::KeyValueNode>();
        if (!root) return nullptr; // Out of memory.

        detail::KeyValueNode* current = root;
        while (true) {
            current->key = parse_string();
            next_token();

            if (*cursor_ != ':') {
                return nullptr; // Invalid format.
            }
            next_token();
            current->value = parse_value();
            next_token();

            if (*cursor_ == '}') {
                current->next = nullptr;
                break; // End of object.
            }

            if (*cursor_ != ',') return nullptr; // Invalid format.

            next_token();
            current->next = alloc_node<detail::KeyValueNode>();
            if (!current->next) return nullptr; // Out of memory.
            current = current->next;
        }
        return root;
    }


    /**
     * @brief Parses an entire array within the JSON.
     * 
     * @return detail::ArrayNode* The ArrayNode head reference to the array.
     */
    detail::ArrayNode* parse_array() {
        if (*cursor_ != '[') return nullptr;
        next_token();

        if (*cursor_ == ']') return nullptr; // Empty Array

        detail::ArrayNode* root = alloc_node<detail::ArrayNode>();
        if (!root) return nullptr; // Out of memory.

        detail::ArrayNode* current = root;
        while (true) {
            current->value = parse_value();
            next_token();

            if (*cursor_ == ']') {
                current->next = nullptr;
                break;
            }

            if (*cursor_ != ',') return nullptr; // Improperly formatted array.

            next_token();
            current->next = alloc_node<detail::ArrayNode>();
            if (!current->next) return nullptr; // Out of memory.
            current = current->next;
        }
        return root;
    }
};


/**
 * @brief A JSON Writer class to create JSON more neatly instead of manually creating strings.
 * 
 * The way that this works is it just directly maps the objects, arrays, and values down
 * to their string format and with a bit of context appends the appropriate characters
 * or strings onto the serialized output buffer. 
 * 
 * @tparam MaxCapacity The max capacity of the string buffer output.
 * @tparam MaxDepth The max depth for nested JSON structures.
 */
template <size_t MaxCapacity = 1024, size_t MaxDepth = 16>
class JsonWriter {
public:
    JsonWriter() {
        needs_comma_[0] = false;
    }

    // Returns the buffer directly for use.
    const StaticStringBuffer<MaxCapacity>& get_buffer() const { return buffer_; }

    JsonWriter& start_object() {
        add_comma();
        buffer_ += '{';
        push_level();
        return *this;
    }

    JsonWriter& end_object() {
        buffer_ += '}';
        pop_level();
        set_needs_comma();
        return *this;
    }

    JsonWriter& start_array() {
        add_comma();
        buffer_ += '[';
        push_level();
        return *this;
    }

    JsonWriter& end_array() {
        buffer_ += ']';
        pop_level();
        set_needs_comma();
        return *this;
    }

    JsonWriter& key(std::string_view k) {
        add_comma();
        buffer_ += '"';
        buffer_.append(k.data(), k.length());
        buffer_ += "\":";
        needs_comma_[current_depth_] = false;
        return *this;
    }

    JsonWriter& value(std::string_view v) {
        add_comma();
        buffer_ += '"';
        buffer_.append(v.data(), v.length());
        buffer_ += '"';
        set_needs_comma();
        return *this;
    }

    JsonWriter& value(const char* v) {
        return value(std::string_view(v));
    }

    JsonWriter& value(double v) {
        add_comma();
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        buffer_.append(buf, ptr - buf);
        set_needs_comma();
        return *this;
    }

    JsonWriter& value(int v) {
        add_comma();
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        buffer_.append(buf, ptr - buf);
        set_needs_comma();
        return *this;
    }

    JsonWriter& value(uint32_t v) {
        add_comma();
        char buf[32];
        auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        buffer_.append(buf, ptr - buf);
        set_needs_comma();
        return *this;
    }

    JsonWriter& value(bool v) {
        add_comma();
        buffer_ += v ? "true" : "false";
        set_needs_comma();
        return *this;
    }

    JsonWriter& null_value() {
        add_comma();
        buffer_ += "null";
        set_needs_comma();
        return *this;
    }

    void clear() {
        buffer_.clear();
        current_depth_ = 0;
        needs_comma_[0] = false;
    }

private:
    StaticStringBuffer<MaxCapacity> buffer_;
    bool needs_comma_[MaxDepth];
    size_t current_depth_ = 0;

    inline void push_level() {
        if ((current_depth_ + 1) < MaxDepth) {
            current_depth_++;
            needs_comma_[current_depth_] = false;
        }
    }

    inline void pop_level() {
        if (current_depth_ > 0) {
            current_depth_--;
        }
    }

    inline void add_comma() {
        if (needs_comma_[current_depth_]) {
            buffer_ += ',';
        }
    }

    inline void set_needs_comma() {
        needs_comma_[current_depth_] = true;
    }
};