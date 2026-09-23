#include "json/json_reader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

namespace gnome_appimage::json {
namespace {

class parser_c {
public:
    parser_c(const std::string &s_text, std::string &s_error)
        : s_text_(s_text), s_error_(s_error) {}

    bool parse(value_c &o_result) {
        skip_space();
        if (!parse_value(o_result)) {
            return false;
        }
        skip_space();
        if (i_index_ != s_text_.size()) {
            return fail("unexpected trailing text");
        }
        return true;
    }

private:
    bool fail(const std::string &s_message) {
        std::ostringstream o_error;
        o_error << s_message << " at offset " << i_index_;
        s_error_ = o_error.str();
        return false;
    }

    bool at_end() const { return i_index_ >= s_text_.size(); }

    char peek() const { return at_end() ? '\0' : s_text_[i_index_]; }

    void skip_space() {
        while (!at_end()) {
            const char c_character = s_text_[i_index_];
            if (' ' != c_character && '\t' != c_character && '\n' != c_character
                && '\r' != c_character) {
                break;
            }
            i_index_++;
        }
    }

    bool consume(const char *s_literal) {
        const std::string s_expected(s_literal);
        if (0 != s_text_.compare(i_index_, s_expected.size(), s_expected)) {
            return fail("expected " + s_expected);
        }
        i_index_ += s_expected.size();
        return true;
    }

    bool parse_value(value_c &o_result) {
        if (at_end()) {
            return fail("unexpected end of input");
        }
        switch (peek()) {
            case '{':
                return parse_object(o_result);
            case '[':
                return parse_array(o_result);
            case '"': {
                std::string s_text;
                if (!parse_string(s_text)) {
                    return false;
                }
                o_result = value_c::make_string(s_text);
                return true;
            }
            case 't':
                if (!consume("true")) {
                    return false;
                }
                o_result = value_c::make_boolean(true);
                return true;
            case 'f':
                if (!consume("false")) {
                    return false;
                }
                o_result = value_c::make_boolean(false);
                return true;
            case 'n':
                if (!consume("null")) {
                    return false;
                }
                o_result = value_c::make_null();
                return true;
            default:
                return parse_number(o_result);
        }
    }

    bool parse_object(value_c &o_result) {
        i_index_++;  // '{'
        value_c o_object = value_c::make_object();
        skip_space();
        if ('}' == peek()) {
            i_index_++;
            o_result = o_object;
            return true;
        }
        while (true) {
            skip_space();
            std::string s_key;
            if (!parse_string(s_key)) {
                return false;
            }
            skip_space();
            if (':' != peek()) {
                return fail("expected :");
            }
            i_index_++;
            skip_space();
            value_c o_value;
            if (!parse_value(o_value)) {
                return false;
            }
            o_object.set(s_key, o_value);
            skip_space();
            if (',' == peek()) {
                i_index_++;
                continue;
            }
            if ('}' == peek()) {
                i_index_++;
                o_result = o_object;
                return true;
            }
            return fail("expected , or }");
        }
    }

    bool parse_array(value_c &o_result) {
        i_index_++;  // '['
        value_c o_array = value_c::make_array();
        skip_space();
        if (']' == peek()) {
            i_index_++;
            o_result = o_array;
            return true;
        }
        while (true) {
            skip_space();
            value_c o_value;
            if (!parse_value(o_value)) {
                return false;
            }
            o_array.append(o_value);
            skip_space();
            if (',' == peek()) {
                i_index_++;
                continue;
            }
            if (']' == peek()) {
                i_index_++;
                o_result = o_array;
                return true;
            }
            return fail("expected , or ]");
        }
    }

    bool parse_hex4(unsigned int &u_value) {
        if (i_index_ + 4 > s_text_.size()) {
            return fail("truncated \\u escape");
        }
        u_value = 0;
        for (int i_digit = 0; i_digit < 4; i_digit++) {
            const char c_character = s_text_[i_index_++];
            u_value <<= 4;
            if ('0' <= c_character && '9' >= c_character) {
                u_value |= static_cast<unsigned int>(c_character - '0');
            } else if ('a' <= c_character && 'f' >= c_character) {
                u_value |= static_cast<unsigned int>(c_character - 'a' + 10);
            } else if ('A' <= c_character && 'F' >= c_character) {
                u_value |= static_cast<unsigned int>(c_character - 'A' + 10);
            } else {
                return fail("invalid hexadecimal digit");
            }
        }
        return true;
    }

    static void append_utf8(std::string &s_text, unsigned int u_code_point) {
        if (0x80 > u_code_point) {
            s_text += static_cast<char>(u_code_point);
        } else if (0x800 > u_code_point) {
            s_text += static_cast<char>(0xc0 | (u_code_point >> 6));
            s_text += static_cast<char>(0x80 | (u_code_point & 0x3f));
        } else {
            s_text += static_cast<char>(0xe0 | (u_code_point >> 12));
            s_text += static_cast<char>(0x80 | ((u_code_point >> 6) & 0x3f));
            s_text += static_cast<char>(0x80 | (u_code_point & 0x3f));
        }
    }

    bool parse_string(std::string &s_result) {
        if ('"' != peek()) {
            return fail("expected a string");
        }
        i_index_++;
        s_result.clear();
        while (true) {
            if (at_end()) {
                return fail("unterminated string");
            }
            const char c_character = s_text_[i_index_++];
            if ('"' == c_character) {
                return true;
            }
            if ('\\' != c_character) {
                s_result += c_character;
                continue;
            }
            if (at_end()) {
                return fail("unterminated escape");
            }
            const char c_escape = s_text_[i_index_++];
            switch (c_escape) {
                case '"':
                    s_result += '"';
                    break;
                case '\\':
                    s_result += '\\';
                    break;
                case '/':
                    s_result += '/';
                    break;
                case 'b':
                    s_result += '\b';
                    break;
                case 'f':
                    s_result += '\f';
                    break;
                case 'n':
                    s_result += '\n';
                    break;
                case 'r':
                    s_result += '\r';
                    break;
                case 't':
                    s_result += '\t';
                    break;
                case 'u': {
                    unsigned int u_code_point = 0;
                    if (!parse_hex4(u_code_point)) {
                        return false;
                    }
                    append_utf8(s_result, u_code_point);
                    break;
                }
                default:
                    return fail("unknown escape");
            }
        }
    }

    bool parse_number(value_c &o_result) {
        const std::size_t i_begin = i_index_;
        if ('-' == peek()) {
            i_index_++;
        }
        bool b_digits = false;
        while (!at_end() && '0' <= peek() && '9' >= peek()) {
            i_index_++;
            b_digits = true;
        }
        if (!b_digits) {
            return fail("expected a value");
        }
        if ('.' == peek()) {
            i_index_++;
            while (!at_end() && '0' <= peek() && '9' >= peek()) {
                i_index_++;
            }
        }
        if ('e' == peek() || 'E' == peek()) {
            i_index_++;
            if ('+' == peek() || '-' == peek()) {
                i_index_++;
            }
            while (!at_end() && '0' <= peek() && '9' >= peek()) {
                i_index_++;
            }
        }
        const std::string s_number = s_text_.substr(i_begin, i_index_ - i_begin);
        o_result = value_c::make_number(std::strtod(s_number.c_str(), nullptr));
        return true;
    }

    const std::string &s_text_;
    std::string &s_error_;
    std::size_t i_index_ = 0;
};

std::string number_text(double d_value) {
    if (std::floor(d_value) == d_value && std::abs(d_value) < 1e15) {
        std::ostringstream o_text;
        o_text << static_cast<long long>(d_value);
        return o_text.str();
    }
    std::ostringstream o_text;
    o_text << d_value;
    return o_text.str();
}

void write_indent(std::ostringstream &o_out, int i_depth) {
    for (int i_index = 0; i_index < i_depth; i_index++) {
        o_out << "  ";
    }
}

void write_value(std::ostringstream &o_out, const value_c &o_value, int i_depth) {
    switch (o_value.kind()) {
        case value_c::kind_e::null_value:
            o_out << "null";
            return;
        case value_c::kind_e::boolean:
            o_out << (o_value.as_boolean() ? "true" : "false");
            return;
        case value_c::kind_e::number:
            o_out << number_text(o_value.as_number());
            return;
        case value_c::kind_e::string: {
            o_out << '"';
            for (const char c_character : o_value.as_string()) {
                switch (c_character) {
                    case '"':
                        o_out << "\\\"";
                        break;
                    case '\\':
                        o_out << "\\\\";
                        break;
                    case '\n':
                        o_out << "\\n";
                        break;
                    case '\r':
                        o_out << "\\r";
                        break;
                    case '\t':
                        o_out << "\\t";
                        break;
                    default:
                        o_out << c_character;
                        break;
                }
            }
            o_out << '"';
            return;
        }
        case value_c::kind_e::array: {
            const std::vector<value_c> &o_items = o_value.items();
            if (o_items.empty()) {
                o_out << "[]";
                return;
            }
            o_out << "[\n";
            for (std::size_t i_index = 0; i_index < o_items.size(); i_index++) {
                write_indent(o_out, i_depth + 1);
                write_value(o_out, o_items[i_index], i_depth + 1);
                if (i_index + 1 < o_items.size()) {
                    o_out << ',';
                }
                o_out << '\n';
            }
            write_indent(o_out, i_depth);
            o_out << ']';
            return;
        }
        case value_c::kind_e::object: {
            // Members are sorted on the way out, so the file is stable however it
            // was built up.
            std::vector<std::pair<std::string, const value_c *>> o_sorted;
            for (const std::pair<std::string, value_c> &o_member : o_value.members()) {
                o_sorted.emplace_back(o_member.first, &o_member.second);
            }
            std::sort(o_sorted.begin(), o_sorted.end(),
                      [](const std::pair<std::string, const value_c *> &o_left,
                         const std::pair<std::string, const value_c *> &o_right) {
                          return o_left.first < o_right.first;
                      });
            if (o_sorted.empty()) {
                o_out << "{}";
                return;
            }
            o_out << "{\n";
            for (std::size_t i_index = 0; i_index < o_sorted.size(); i_index++) {
                write_indent(o_out, i_depth + 1);
                o_out << '"' << o_sorted[i_index].first << "\": ";
                write_value(o_out, *o_sorted[i_index].second, i_depth + 1);
                if (i_index + 1 < o_sorted.size()) {
                    o_out << ',';
                }
                o_out << '\n';
            }
            write_indent(o_out, i_depth);
            o_out << '}';
            return;
        }
    }
}

const std::string s_no_string;
const std::vector<value_c> o_no_items;

}  // namespace

value_c value_c::parse(const std::string &s_text, std::string &s_error) {
    value_c o_result;
    parser_c o_parser(s_text, s_error);
    if (!o_parser.parse(o_result)) {
        return value_c();
    }
    return o_result;
}

value_c value_c::make_null() {
    return value_c();
}

value_c value_c::make_boolean(bool b_value) {
    value_c o_value;
    o_value.e_kind_ = kind_e::boolean;
    o_value.b_boolean_ = b_value;
    return o_value;
}

value_c value_c::make_number(double d_value) {
    value_c o_value;
    o_value.e_kind_ = kind_e::number;
    o_value.d_number_ = d_value;
    return o_value;
}

value_c value_c::make_string(const std::string &s_text) {
    value_c o_value;
    o_value.e_kind_ = kind_e::string;
    o_value.s_string_ = s_text;
    return o_value;
}

value_c value_c::make_array() {
    value_c o_value;
    o_value.e_kind_ = kind_e::array;
    return o_value;
}

value_c value_c::make_object() {
    value_c o_value;
    o_value.e_kind_ = kind_e::object;
    return o_value;
}

value_c::kind_e value_c::kind() const {
    return e_kind_;
}

bool value_c::is_null() const {
    return kind_e::null_value == e_kind_;
}

bool value_c::is_boolean() const {
    return kind_e::boolean == e_kind_;
}

bool value_c::is_number() const {
    return kind_e::number == e_kind_;
}

bool value_c::is_string() const {
    return kind_e::string == e_kind_;
}

bool value_c::is_array() const {
    return kind_e::array == e_kind_;
}

bool value_c::is_object() const {
    return kind_e::object == e_kind_;
}

bool value_c::as_boolean(bool b_fallback) const {
    return kind_e::boolean == e_kind_ ? b_boolean_ : b_fallback;
}

double value_c::as_number(double d_fallback) const {
    return kind_e::number == e_kind_ ? d_number_ : d_fallback;
}

const std::string &value_c::as_string() const {
    return kind_e::string == e_kind_ ? s_string_ : s_no_string;
}

const std::vector<value_c> &value_c::items() const {
    return kind_e::array == e_kind_ ? o_items_ : o_no_items;
}

const value_c *value_c::member(const std::string &s_key) const {
    if (kind_e::object != e_kind_) {
        return nullptr;
    }
    for (const std::pair<std::string, value_c> &o_member : o_members_) {
        if (o_member.first == s_key) {
            return &o_member.second;
        }
    }
    return nullptr;
}

const std::vector<std::pair<std::string, value_c>> &value_c::members() const {
    return o_members_;
}

void value_c::set(const std::string &s_key, value_c o_value) {
    if (kind_e::object != e_kind_) {
        return;
    }
    for (std::pair<std::string, value_c> &o_member : o_members_) {
        if (o_member.first == s_key) {
            o_member.second = std::move(o_value);
            return;
        }
    }
    o_members_.emplace_back(s_key, std::move(o_value));
}

void value_c::append(value_c o_value) {
    if (kind_e::array == e_kind_) {
        o_items_.push_back(std::move(o_value));
    }
}

std::string value_c::string_or(const std::string &s_key,
                               const std::string &s_fallback) const {
    const value_c *p_member = member(s_key);
    if (nullptr == p_member || !p_member->is_string()) {
        return s_fallback;
    }
    return p_member->as_string();
}

bool value_c::boolean_or(const std::string &s_key, bool b_fallback) const {
    const value_c *p_member = member(s_key);
    if (nullptr == p_member || !p_member->is_boolean()) {
        return b_fallback;
    }
    return p_member->as_boolean();
}

double value_c::number_or(const std::string &s_key, double d_fallback) const {
    const value_c *p_member = member(s_key);
    if (nullptr == p_member || !p_member->is_number()) {
        return d_fallback;
    }
    return p_member->as_number();
}

std::vector<value_c> value_c::array_or_empty(const std::string &s_key) const {
    const value_c *p_member = member(s_key);
    if (nullptr == p_member || !p_member->is_array()) {
        return {};
    }
    return p_member->items();
}

std::string value_c::to_text() const {
    std::ostringstream o_out;
    write_value(o_out, *this, 0);
    o_out << '\n';
    return o_out.str();
}

}  // namespace gnome_appimage::json
