#ifndef GNOME_APPIMAGE_JSON_READER_H
#define GNOME_APPIMAGE_JSON_READER_H

#include <string>
#include <utility>
#include <vector>

namespace gnome_appimage::json {

// A minimal JSON value: enough to read what this project writes, and to keep the
// activator's geometry file while editing one entry of it.  It is a value, not a
// general-purpose library; malformed input is reported through an error string.
//
// Member order is preserved, so writing back a document that was read keeps the
// members this code does not understand exactly where they were.
class value_c {
public:
    enum class kind_e { null_value, boolean, number, string, array, object };

    // Parse one document.  On failure the returned value is null and s_error says
    // why and where.
    static value_c parse(const std::string &s_text, std::string &s_error);

    static value_c make_null();
    static value_c make_boolean(bool b_value);
    static value_c make_number(double d_value);
    static value_c make_string(const std::string &s_text);
    static value_c make_array();
    static value_c make_object();

    kind_e kind() const;
    bool is_null() const;
    bool is_boolean() const;
    bool is_number() const;
    bool is_string() const;
    bool is_array() const;
    bool is_object() const;

    // Accessors for a value of the matching kind, otherwise a safe default.
    bool as_boolean(bool b_fallback = false) const;
    double as_number(double d_fallback = 0.0) const;
    const std::string &as_string() const;
    const std::vector<value_c> &items() const;

    // Members of an object, or nullptr when this value is not an object.
    const value_c *member(const std::string &s_key) const;
    const std::vector<std::pair<std::string, value_c>> &members() const;
    void set(const std::string &s_key, value_c o_value);
    void append(value_c o_value);

    // Convenience readers for a member of an object.
    std::string string_or(const std::string &s_key,
                          const std::string &s_fallback = std::string()) const;
    bool boolean_or(const std::string &s_key, bool b_fallback) const;
    double number_or(const std::string &s_key, double d_fallback = 0.0) const;
    std::vector<value_c> array_or_empty(const std::string &s_key) const;

    // Serialise with two-space indentation and sorted keys, the format the
    // activator has always written.
    std::string to_text() const;

private:
    kind_e e_kind_ = kind_e::null_value;
    bool b_boolean_ = false;
    double d_number_ = 0.0;
    std::string s_string_;
    std::vector<value_c> o_items_;
    std::vector<std::pair<std::string, value_c>> o_members_;
};

}  // namespace gnome_appimage::json

#endif
