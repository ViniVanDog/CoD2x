#include "mongoose/mongoose.h"
#include <stdbool.h>
#include <string.h>
#include <functional>

// Get string field. Caller provides buffer.
// Returns true if found, false otherwise.
static inline bool json_get_str(const char *json, const char *path, char *out, size_t out_len) 
{
    struct mg_str j = mg_str(json);

    // 1) Try as string
    char *s = mg_json_get_str(j, path);
    if (s != NULL) {
        strncpy(out, s, out_len - 1);
        out[out_len - 1] = '\0';
        mg_free(s);
        return true;
    }

    // 2) Try as number, format with no decimals
    double num;
    if (mg_json_get_num(j, path, &num)) {
        if (out_len > 0) {
            snprintf(out, out_len, "%.0f", num);
            out[out_len - 1] = '\0';
        }
        return true;
    }

    // 3) Optional: support booleans as strings
    bool b;
    if (mg_json_get_bool(j, path, &b)) {
        const char *val = b ? "true" : "false";
        strncpy(out, val, out_len - 1);
        out[out_len - 1] = '\0';
        return true;
    }

    // Not found or unsupported type
    return false;
}

// Get numeric field
static inline bool json_get_num(const char *json, const char *path, double *out) {
    struct mg_str j = mg_str(json);
    return mg_json_get_num(j, path, out);
}

// Get boolean field
static inline bool json_get_bool(const char *json, const char *path, bool *out) {
    struct mg_str j = mg_str(json);
    return mg_json_get_bool(j, path, out);
}

// Get long integer field
static inline bool json_get_long(const char *json, const char *path, long *out) {
    struct mg_str j = mg_str(json);
    // Use a sentinel value unlikely to appear in real data
    long sentinel = 0x7fffffffL;  // 2147483647
    long val = mg_json_get_long(j, path, sentinel);
    if (val == sentinel) return false;
    *out = val;
    return true;
}

/**
 * @brief Iterates over elements of a JSON array at the specified path and invokes a callback for each element.
 *
 * This function parses the given JSON string, locates the array at the provided JSON path,
 * and for each element in the array, calls the user-provided callback function.
 *
 * @param json The JSON string to parse.
 * @param path The JSON path to the target array (e.g., ".items").
 * @param cb       Callback invoked for each element: (index, value, length)
 *                 - @p index  = array index (0-based)
 *                 - @p value  = pointer to the raw JSON value (not nul-terminated)
 *                 - @p length = length of the value
 * @return false if callback returns false for any element, true otherwise.
 */
static inline bool json_iter_array(const char *json, const char *path, const std::function<bool(int,const char*,int)> &cb) {
    struct mg_str j = mg_str(json);
    struct mg_str arr = mg_json_get_tok(j, path); // get array token
    if (arr.buf == NULL) return true;

    size_t ofs = 0;
    struct mg_str key, val;
    int index = 0;
    while ((ofs = mg_json_next(arr, ofs, &key, &val)) > 0) {
        if (!cb(index++, val.buf, (int) val.len)) {
            return false;
        }
    }
    return true;
}

/**
 * Escape a string for JSON output.
 * Example: json_escape_string("Hello \"world\"", escaped, sizeof(escaped)); => "Hello \"world\""
 */
static inline std::string json_escape_string(const std::string &in) {
    std::string out;
    out.reserve(in.size() * 2 + 16); // Worst case: every char escapes to two chars, plus a few \uXXXX. Reserve some headroom.

    for (size_t i = 0; i < in.size(); i++) {
        unsigned char c = (unsigned char) in[i];
        switch (c) {
            case '"':  out.push_back('\\'); out.push_back('"'); break;
            case '\\': out.push_back('\\'); out.push_back('\\'); break;
            case '\b': out.push_back('\\'); out.push_back('b');  break;
            case '\f': out.push_back('\\'); out.push_back('f');  break;
            case '\n': out.push_back('\\'); out.push_back('n');  break;
            case '\r': out.push_back('\\'); out.push_back('r');  break;
            case '\t': out.push_back('\\'); out.push_back('t');  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", (unsigned) c);
                    buf[sizeof(buf) - 1] = '\0';
                    out.append(buf);
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    return out;
}