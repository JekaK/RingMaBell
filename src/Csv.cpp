#include "Csv.h"

namespace rmb {

std::vector<std::string> parseCsvLine(const std::string& line, bool& ok, std::string& error) {
    ok = true;
    error.clear();

    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;

    for (size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cell.push_back('"');
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                cell.push_back(c);
            }
        } else {
            if (c == ',') {
                cells.push_back(cell);
                cell.clear();
            } else if (c == '"') {
                if (!cell.empty()) {
                    ok = false;
                    error = "quote must appear at the beginning of a CSV field";
                    return {};
                }
                quoted = true;
            } else {
                cell.push_back(c);
            }
        }
    }

    if (quoted) {
        ok = false;
        error = "unterminated quoted CSV field";
        return {};
    }

    cells.push_back(cell);
    return cells;
}

std::string csvEscape(const std::string& value) {
    bool mustQuote = false;
    for (const char c : value) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            mustQuote = true;
            break;
        }
    }

    if (!mustQuote) {
        return value;
    }

    std::string out = "\"";
    for (const char c : value) {
        if (c == '"') {
            out += "\"\"";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

} // namespace rmb
