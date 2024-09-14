#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <unistd.h>
#include <vector>

#include "elf_parser.h"
#include "tag_parser.h"

#include "lca.h"
#include "policy.h"

using namespace std;

const string policy_output_file_name = "policy.mtag";
const string tags_output_file_name   = "tags.mtag";

void print_tags(
    ofstream &out, elf_data_t &elf_data, const tag_data_t &tag_data,
    const policy_t &policy
);
static inline void out_print_line(
    ofstream &out, const uint64_t addr, const size_t size, const int tag_index
);

/**
 * ENTRY POINT
 * 
 * The function takes in 3 arguments:
 * - the ELF file (result of compilation)
 * - the tag file (contains annotated variables, like, atom <name>: "<tag>")
 * - the policy file (contains topologies)
 *
 * 3 unique_ptr variables point to these files.
 *
 *
 */
int main(int argc, char *argv[]) {
    if (argc < 4) {
        cout << "Missing arguments!" << endl;
        cout << "Usage: " << argv[0] << " <elf-file> <tag-file> <policy-file>"
             << endl;
        return 0;
    }

    unique_ptr<policy_t> policy;
    unique_ptr<elf_data_t> elf_data;
    unique_ptr<tag_data_t> tag_data;

    try {
        policy          = make_unique<policy_t>(argv[3]); // Opens file
        auto &matrix    = policy->topology->matrix();
        auto lca_matrix = compute_lca(matrix);

        if (lca_matrix.size() > 256) {
            cerr << "The policy is too big: " << lca_matrix.size()
                 << " tags found, but there are only 256 available!" << endl;
            exit(1);
        }

        policy->set_lca_matrix(lca_matrix);
    } catch (runtime_error &err) {
        cerr << err.what() << endl;
        exit(1);
    } catch (...) {
        cerr << "Failed policy!" << endl;
        exit(1);
    }

    try {
        elf_data = make_unique<elf_data_t>(argv[1]);
    } catch (exception &e) {
        cerr << "exception: " << e.what() << endl;
        exit(1);
    } catch (...) {
        cerr << "Failed to get ELF data!" << endl;
        exit(1);
    }

    try {
        tag_data = make_unique<tag_data_t>(argv[2], *policy);
    } catch (exception &e) {
        cerr << e.what() << endl;
        exit(1);
    } catch (...) {
        cerr << "Failed to get tag data!" << endl;
        exit(1);
    }

    ofstream out_file(policy_output_file_name);
    if (out_file.is_open()) {
        policy->dump(out_file);
        print_tags(out_file, *elf_data, *tag_data, *policy);
    }

    ofstream dup_elf(tags_output_file_name, ios::out | ios::binary);
    if (dup_elf.is_open()) {
        elf_data->dump(dup_elf);
    }

    return 0;
}

void print_tags(
    ofstream &out, elf_data_t &elf_data, const tag_data_t &tag_data,
    const policy_t &policy
) {
    for (auto &tag_entry : tag_data.getentries()) {
        try {
            elf_symbol_t elf_symbol =
                elf_data.get_symbol_info(tag_entry.symbol);
            elf_data.set_tag_data(
                elf_symbol.value, elf_symbol.size,
                policy.tag_index(tag_entry.tag)
            );
            if (tag_entry.type == Tag_type::PTR) {
                uint64_t addr = elf_data.get_ptr_addr(elf_symbol.value);
                if (addr > 0) {
                    elf_data.set_tag_data(
                        addr, tag_entry.ptr_size,
                        policy.tag_index(tag_entry.tag)
                    );
                    out_print_line(
                        out, addr, tag_entry.ptr_size,
                        policy.tag_index(tag_entry.tag)
                    );
                }
            }
            out_print_line(
                out, elf_symbol.value, elf_symbol.size,
                policy.tag_index(tag_entry.tag)
            );
        } catch (runtime_error &e) {
            cerr << "Couldn't locate symbol '" << tag_entry.symbol
                 << "' in the ELF file!" << endl;
        }
    }
}

static inline void out_print_line(
    ofstream &out, const uint64_t addr, const size_t size, const int tag_index
) {
    out << "0x" << hex << addr << "," << dec << size << "," << tag_index
        << endl;
}
