#include "resp_parser.hpp"
#include <sstream>
#include <algorithm>

bool RespParser::parse_command(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens) {
    if (buffer.empty()) return false;
    tokens.clear();
    bytes_consumed = 0;

    if (buffer[0] == '*') {
        return parse_resp_array(buffer, bytes_consumed, tokens);
    } else {
        return parse_inline_command(buffer, bytes_consumed, tokens);
    }
}

bool RespParser::parse_resp_array(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens) {
    size_t pos = 0;
    if (buffer[pos] != '*') return false;
    pos++;

    // Find first \r\n
    size_t crlf = buffer.find("\r\n", pos);
    if (crlf == std::string::npos) return false;

    std::string count_str = buffer.substr(pos, crlf - pos);
    int num_elements = 0;
    try {
        num_elements = std::stoi(count_str);
    } catch (...) {
        return false;
    }
    pos = crlf + 2;

    for (int i = 0; i < num_elements; ++i) {
        if (pos >= buffer.size()) return false;
        if (buffer[pos] != '$') return false;
        pos++;

        crlf = buffer.find("\r\n", pos);
        if (crlf == std::string::npos) return false;

        std::string len_str = buffer.substr(pos, crlf - pos);
        int str_len = 0;
        try {
            str_len = std::stoi(len_str);
        } catch (...) {
            return false;
        }
        pos = crlf + 2;

        if (str_len < 0) {
            tokens.push_back(""); // null bulk string
            continue;
        }

        if (pos + str_len + 2 > buffer.size()) {
            return false; // not enough bytes received yet
        }

        std::string token = buffer.substr(pos, str_len);
        pos += str_len;

        if (buffer.substr(pos, 2) != "\r\n") return false;
        pos += 2;

        tokens.push_back(token);
    }

    bytes_consumed = pos;
    return true;
}

bool RespParser::parse_inline_command(const std::string& buffer, size_t& bytes_consumed, std::vector<std::string>& tokens) {
    size_t eol = buffer.find('\n');
    if (eol == std::string::npos) return false;

    size_t line_len = eol;
    if (line_len > 0 && buffer[line_len - 1] == '\r') {
        line_len--;
    }

    std::string line = buffer.substr(0, line_len);
    bytes_consumed = eol + 1;

    std::istringstream iss(line);
    std::string token;
    while (iss >> token) {
        tokens.push_back(token);
    }

    return !tokens.empty();
}

std::string RespParser::format_simple_string(const std::string& str) {
    return "+" + str + "\r\n";
}

std::string RespParser::format_error(const std::string& err) {
    if (err.rfind("ERR", 0) == 0 || err.rfind("WRONGTYPE", 0) == 0) {
        return "-" + err + "\r\n";
    }
    return "-ERR " + err + "\r\n";
}

std::string RespParser::format_integer(int64_t val) {
    return ":" + std::to_string(val) + "\r\n";
}

std::string RespParser::format_bulk_string(const std::string& str) {
    return "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
}

std::string RespParser::format_nil() {
    return "$-1\r\n";
}

std::string RespParser::format_array(const std::vector<std::string>& items) {
    std::string out = "*" + std::to_string(items.size()) + "\r\n";
    for (const auto& item : items) {
        out += format_bulk_string(item);
    }
    return out;
}

std::string RespParser::format_empty_array() {
    return "*0\r\n";
}
