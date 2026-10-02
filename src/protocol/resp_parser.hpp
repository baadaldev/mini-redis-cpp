#ifndef RESP_PARSER_HPP
#define RESP_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>

class RespParser {
public:
    // Parse raw incoming buffer from client into command tokens (e.g. ["SET", "key", "val"])
    // Returns true if a complete command was parsed, setting bytes_consumed.
    static bool parse_command(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens);

    // Serialization helpers to format responses back to Redis clients
    static std::string format_simple_string(const std::string& str);
    static std::string format_error(const std::string& err);
    static std::string format_integer(int64_t val);
    static std::string format_bulk_string(const std::string& str);
    static std::string format_nil();
    static std::string format_array(const std::vector<std::string>& items);
    static std::string format_empty_array();

private:
    static bool parse_resp_array(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens);
    static bool parse_inline_command(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens);
};

#endif // RESP_PARSER_HPP
