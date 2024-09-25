#include "tag_parser.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace std;

static Tag_type get_type(istringstream &iss);
static pair<string, bool> get_symbol(istringstream &iss);
static string get_tag(istringstream &iss, bool colon);
static size_t get_ptr_size(istringstream &iss, bool &colon);

tag_data_t::tag_data_t(const char *file_path, const policy_t &policy) {
    ifstream infile(file_path);

    if (!infile.is_open()) {
        ostringstream oss;
        oss << "Couldn't open tag file: '" << file_path << "'!";
        throw invalid_argument(oss.str());
    }

    string line;
    int line_num = 1;
    while (getline(infile, line)) {
        if (line.size() == 0) { // skip empty lines
            continue;
        }
        istringstream iss(line);
        try {
            Tag_type type = get_type(iss);
            auto t        = get_symbol(iss);
            string symbol = t.first;
            size_t size
                = (type == Tag_type::PTR) ? get_ptr_size(iss, t.second) : 0;
            string tag = get_tag(iss, t.second);

            if (policy.contains_tag(tag)) {
                tag_struct_t tag_data = {type, symbol, tag, size};
                entries.push_back(tag_data);
            } else {
                cerr << "Tag '" << tag << "' is not in the specified policy!"
                     << endl;
            }
        } catch (runtime_error &err) {
            ostringstream oss;
            oss << "Line " << line_num << ": Wrong syntax! " << err.what();
            throw invalid_argument(oss.str());
        }
        line_num++;
    }
}

static Tag_type get_type(istringstream &iss) {
    string r;
    char c;
    while (iss.get(c)) {
        if (c == ' ') {
            break;
        }
        r += c;
    }
    if (r == "ptr") {
        return Tag_type::PTR;
    } else if (r == "atom") {
        return Tag_type::ATOM;
    }
    throw runtime_error("Only 'ptr' or 'atom' keywords allowed!");
}

static pair<string, bool> get_symbol(istringstream &iss) {
    string r;
    char c;
    bool colon = false;
    while (iss.get(c)) {
        if (c == ':') {
            colon = true;
            break;
        } else if (c == ' ') {
            break;
        }
        r += c;
    }

    if (iss.eof()) {
        throw runtime_error("Missing rest of tag declaration!");
    }
    return make_pair(r, colon);
}

static string get_tag(istringstream &iss, bool colon) {
    string r;
    char c;
    if (!colon) {
        bool colon_found = false;
        do {
            iss.get(c);
            colon_found = c == ':';
        } while (!colon_found && !iss.eof());

        if (!colon_found) {
            throw runtime_error("Colon not found in declaration!");
        }
    }

    do {
        iss.get(c);
    } while (c != '"' && !iss.eof());
    while (iss.get(c) && c != '"') {
        r += c;
    }
    if (c != '"') {
        throw runtime_error("Missing end of tag declaration '\"'!");
    }
    r.erase(remove_if(r.begin(), r.end(), ::isspace), r.end());
    if (r.size() == 0) {
        throw runtime_error("Missing tag in declaration!");
    }
    return r;
}

static size_t get_ptr_size(istringstream &iss, bool &colon) {
    if (colon) {
        throw runtime_error("Pointer declaration needs size argument!");
    }
    char c;
    size_t r = 0;
    string size_word;
    for (int i = 0; i < 4; i++) {
        iss.get(c);
        size_word += c;
    }
    if (size_word != "size") {
        throw runtime_error("Expected 'size' keyword!");
    }

    bool found = false;
    do {
        iss.get(c);
        found = c == '=';
    } while (!found && !iss.eof());

    if (!found) {
        throw runtime_error("Missing '=' sign in declaration!");
    }

    size_word.clear();
    do {
        iss.get(c);
    } while (c == ' ');

    size_word += c;
    while (iss.get(c)) {
        if (c == ' ') {
            break;
        }
        if (c == ':') {
            colon = true;
            break;
        }
        size_word += c;
    }
    r = stoul(size_word);

    return r;
}
