#include "elf_parser.h"

#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <exception>
#include <algorithm>


#define PREAD(fd, buff, size, offset) ({ \
	if (pread((fd), (buff), (size), (offset)) < 0) { \
		std::cerr << "pread: " << strerror(errno) << std::endl; \
		close((fd)); \
		std::abort(); \
	} \
})


static inline bool elf_check_file(Elf64_Ehdr *hdr) {
	return hdr &&
		hdr->e_ident[EI_MAG0] == ELFMAG0 &&
		hdr->e_ident[EI_MAG1] == ELFMAG1 &&
		hdr->e_ident[EI_MAG2] == ELFMAG2 &&
		hdr->e_ident[EI_MAG3] == ELFMAG3;
}

static inline bool elf_is64(Elf64_Ehdr *hdr) {
	return hdr->e_ident[4] == ELFCLASS64;
}

static inline bool elf_is_riscv(Elf64_Ehdr *hdr) {
	return hdr->e_machine == EM_RISCV;
}

elf_data_t::elf_data_t(const char *file_name) :
		fd(-1) {
	fd = open(file_name, O_RDONLY);
	if (fd < 0) {
		std::cerr << "Unable to open " << file_name << "! Error: " << strerror(errno) << std::endl;
		throw std::invalid_argument("Unable to open ELF file!");
	}

	PREAD(fd, &ehdr, sizeof(ehdr), 0);

	if (!elf_check_file(&ehdr) || !elf_is64(&ehdr) || !elf_is_riscv(&ehdr)) {
		close(fd);
		throw std::invalid_argument("File is not a 64-bit RISC-V ELF file!");
	}

	Elf64_Shdr str_shdr;
	PREAD(fd, &str_shdr, ehdr.e_shentsize, ehdr.e_shoff + ehdr.e_shstrndx * ehdr.e_shentsize);

	char *tbl = (char *) malloc(str_shdr.sh_size);
	if (tbl == nullptr) {
		close(fd);
		std::abort();
	}
	PREAD(fd, tbl, str_shdr.sh_size, str_shdr.sh_offset);

	Elf64_Shdr *shdrs = (Elf64_Shdr *) malloc(ehdr.e_shnum * ehdr.e_shentsize);
	if (shdrs == nullptr) {
		close(fd);
		std::abort();
	}
	PREAD(fd, shdrs, ehdr.e_shnum * ehdr.e_shentsize, ehdr.e_ehsize + ehdr.e_shoff);
	for (int i = 0; i < ehdr.e_shnum; i++) {
		elf_shdr_t eshdr = {std::string(tbl + shdrs[i].sh_name), shdrs[i]};
		section_hdrs.push_back(eshdr);
	}
	free(tbl);
	free(shdrs);

	for (auto& eshdr : section_hdrs) {
		if (eshdr.shdr.sh_type == SHT_SYMTAB) {
			Elf64_Sym *sym_table = (Elf64_Sym *) malloc(eshdr.shdr.sh_size);
			if (sym_table == nullptr) {
				close(fd);
				std::cerr << "Failed to malloc symbol table!" << std::endl;
				std::abort();
			}
			PREAD(fd, sym_table, eshdr.shdr.sh_size, eshdr.shdr.sh_offset);

			elf_shdr_t linked_section = section_hdrs.at(eshdr.shdr.sh_link - 1);
			char *str_table = (char *) malloc(linked_section.shdr.sh_size);
			PREAD(fd, str_table, linked_section.shdr.sh_size, linked_section.shdr.sh_offset);

			int entries = eshdr.shdr.sh_size / sizeof(Elf64_Sym);
			for (int i = 0; i < entries; i++) {
				std::string name(str_table + sym_table[i].st_name);
				elf_symbol_t symbol = {name, ELF64_ST_TYPE(sym_table[i].st_info),
					ELF64_ST_BIND(sym_table[i].st_info), sym_table[i].st_other,
					sym_table[i].st_shndx, sym_table[i].st_value,
					sym_table[i].st_size};
				symbol_table[name] = symbol;
			}

			free(str_table);
			free(sym_table);
		}
	}
}

elf_data_t::~elf_data_t() {
	if (fd > 0) {
		if (close(fd) < 0) {
			std::cerr << "Failed to close file descriptor! Error: " << strerror(errno) << std::endl;
		}
	}
}

void elf_data_t::print_symbols() {
	for (auto & symbol : symbol_table) {
		std::cout << symbol.first << " value: " << std::hex << symbol.second.value << std::endl;
	}
}


elf_symbol_t elf_data_t::get_symbol_info(std::string name) {
	auto search = symbol_table.find(name);
	if (search == symbol_table.end()) {
		throw std::runtime_error("Symbol '" + name + "' dosen't exist in ELF file!");
	}
	return search->second;
}


uint64_t elf_data_t::get_ptr_addr(uint64_t ptr) {
	uint64_t r = 0;

	for (auto& eshdr : section_hdrs) {
		// data section
		if (eshdr.shdr.sh_type == SHT_PROGBITS && eshdr.shdr.sh_flags == (SHF_WRITE | SHF_ALLOC)) {
			if (ptr > eshdr.shdr.sh_addr && ptr < eshdr.shdr.sh_addr + eshdr.shdr.sh_size) {
				PREAD(fd, &r, sizeof(uint64_t), eshdr.shdr.sh_offset + (ptr - eshdr.shdr.sh_addr));
				break;
			}
		}
	}

	return r;
}