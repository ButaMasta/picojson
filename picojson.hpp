// picojson.hpp
#include <cstddef>
#include <cctype>
#include <cstring>
#include <string_view>
#include <variant>
#include <charconv>
#include <iostream>


namespace detail {
    // Class tools
    struct KeyValueNode;
    using Value = 
        std::variant<
            std::monostate,
            KeyValueNode*,
            std::string_view,
            double,
            bool
        >;

    struct KeyValueNode {
        std::string_view key;
        Value value;
        KeyValueNode* next;
    };


    class Returnable {
    public:
        Returnable(Value value) : value_(value) {}
        Returnable() {
            value_ = std::monostate();
        }
        ~Returnable() = default;

        operator double() const {
            try {
                std::cout << "printing int" << std::endl;
                return std::get<double>(value_);
            } catch (const std::bad_variant_access&) {
                return 0;
            }
        }
        operator std::string_view() {
            try {
                std::cout << "printing string view" << std::endl;
                return std::get<std::string_view>(value_);
            } catch (const std::bad_variant_access&) {
                return {};
            }
        }

    private:
        Value value_;
    };


    inline bool is_digit(char c) {
        return c >= '0' && c <= '9';
    }
}


/**
 * @brief The standard object representation of a JSON string. 
 * Values are referencable with the use of the `[]` characters.
 */
class JsonObject {
public:
    JsonObject(detail::KeyValueNode* root) : root_(root) {}
    ~JsonObject() = default;

    detail::Returnable operator[](const char* key) {
        if (!root_) {
            return detail::Returnable();
        }
        detail::KeyValueNode* current = root_;
        while (current->key != key) {
            if (!current->next) {
                return detail::Returnable();
            }
            current = current->next;
        }
        return detail::Returnable(current->value);
    }

private:
    // The root KeyValueNode for the linked list.
    detail::KeyValueNode* root_;
};


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
     * @return JsonObject - The root JsonObject for this object.
     */
    JsonObject parse(const char* json_str) {
        cursor_ = json_str;
        skip_whitespace(); 
        return JsonObject(parse_object());
    }

private:
    static constexpr size_t k_max_nodes = max_bytes / sizeof(detail::KeyValueNode);
    detail::KeyValueNode pool_[k_max_nodes];
    size_t pool_index_ = 0;
    const char* cursor_;
    
    /**
     * @brief Allocates a node for the user and returns the pointer, 
     * if the pool is full return nullptr.
     * 
     * @return JsonNode* - The pointer to the allocated node.
     */
    inline detail::KeyValueNode* alloc_node() {
        if (pool_index_ >= k_max_nodes) {
            return nullptr; // Out of memory.
        }
        return &pool_[pool_index_++];
    }

    /**
     * @brief Skips any whitespace characters in the current string.
     * 
     */
    inline void skip_whitespace() {
        while (*cursor_ && isspace(*cursor_)) { cursor_++; }
    }

    /**
     * @brief Finds the next token from the current: {, ", [, etc.
     * 
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
        if (*cursor_ != '"') {
            return {};  // TODO: Make this throw an exception.
        }
        const char* start = ++cursor_;
        while (*cursor_ != '"') {
            cursor_++;
        }
        return {start, static_cast<long unsigned int>(cursor_ - start)};
    }


    /**
     * @brief Parses a double from the json string
     * 
     * @return double - The parsed value
     */
    double parse_double() {
        const char* start = cursor_;
        while (detail::is_digit(*cursor_)) {
            cursor_++;
        }
        double value{};
        std::from_chars(start, cursor_, value);
        return value;
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
        } else if (*cursor_ == '"') {
            value = parse_string();
        } else if (detail::is_digit(*cursor_)) {
            value = parse_double();
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
        if (*cursor_ != '{') {
            return nullptr;
        }
        // Proceeed.
        next_token();
        // If this is an empty object return as such.
        if (*cursor_ == '}') {
            return nullptr;
        }

        // Object to parse.
        detail::KeyValueNode* root = alloc_node();
        if (!root) {
            return nullptr; // Out of memory.
        }
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
                break;  // End of object.
            }

            if (*cursor_ != ',') {
                return nullptr; // Invalid format.
            }
            next_token();
            current->next = alloc_node();
            if (!current->next) {
                return nullptr; // Out of memory.
            }
            current = current->next;
        }
        return root;
    }
};