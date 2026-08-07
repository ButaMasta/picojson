// picojson.hpp
#include <cstddef>
#include <cctype>
#include <cstring>
#include <string_view>
#include <variant>
#include <charconv>


class JsonObject;
class JsonArray;

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


    class Returnable {
    public:
        Returnable(Value value) : value_(value) {}
        Returnable() {
            value_ = std::monostate();
        }
        ~Returnable() = default;

        operator double() const {
            if (const double* val = std::get_if<double>(&value_)) {
                return *val;
            }
            return 0.0;
        }

        operator std::string_view() const {
            if (const std::string_view* val = std::get_if<std::string_view>(&value_)) {
                return *val;
            }
            return {};
        }

        operator bool() const {
            if (const bool* val = std::get_if<bool>(&value_)) {
                return *val;
            }
            return false;
        }

        bool is_null() const {
            return std::holds_alternative<std::monostate>(value_);
        }

        operator JsonObject() const;
        operator JsonArray() const;

    private:
        Value value_;
    };


    inline bool is_number_char(char c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '.';
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


class JsonArray {
public:
    JsonArray(detail::ArrayNode* root = nullptr) : root_(root) {}
    ~JsonArray() = default;

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

private:
    detail::ArrayNode* root_;
};


/**
 * @brief Allows for an implicit cast of a value to a JsonObject.
 * 
 * @return JsonObject - The object if found from the value.
 */
inline detail::Returnable::operator JsonObject() const {
    if (detail::KeyValueNode* const *val = std::get_if<detail::KeyValueNode*>(&value_)) {
        return JsonObject(*val);
    }
    return JsonObject(nullptr);
}


/**
 * @brief Allows for an implicit cast of a value to a JsonArray.
 * 
 * @return JsonArray - The array if found from the value.
 */
inline detail::Returnable::operator JsonArray() const {
    if (detail::ArrayNode* const *val = std::get_if<detail::ArrayNode*>(&value_)) {
        return JsonArray(*val);
    }
    return JsonArray(nullptr);
}


/**
 * @brief The core parser handling the conversion of a JSON string to a 
 * JsonObject.
 * 
 * @tparam max_bytes The maximum bytes this parser is allowed to use on the
 * stack. There is zero dynamic allocation so this value should be at least
 * 1024 or higher for most cases.
 */
template <std::size_t max_bytes>
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
    JsonObject parse(const char* json_str) {
        cursor_ = json_str;
        pool_offset_ = 0;
        skip_whitespace();

        return detail::Returnable(parse_value());
    }

private:
    alignas(std::max_align_t) std::byte pool_[max_bytes];
    size_t pool_offset_ = 0;
    const char* cursor_;
    
    /**
     * @brief Generic allocator to reserve space in the pool.
     * 
     * @tparam T The node type, KV or Array.
     * @return T* - The new pointer to the node.
     */
    template<typename T>
    inline T* alloc_node() {
        if (pool_offset_ + sizeof(T) > max_bytes) {
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
        cursor_++;
        skip_whitespace();
    }


    /**
     * @brief Parses a string from within the json string.
     * 
     * @return std::string_view The zero copy reference to the parsed string.
     */
    std::string_view parse_string() {
        if (*cursor_ != '"') { return {}; }
        const char* start = ++cursor_;
        while (*cursor_ && *cursor_ != '"') {
            if (*cursor_ == '\\' && *(cursor_ + 1) == '"') {
                cursor_++;
            }
            cursor_++;
        }
        return std::string_view(start, static_cast<size_t>(cursor_ - start));
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