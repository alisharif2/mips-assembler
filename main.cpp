#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <bitset>
#include <string>
#include <fstream>
#include <algorithm>

enum arg_format {
	rt_,
	rd_,
	rs_,
	rs_rt,
	rd_rs,
	rd_rt_shamt,
	rd_rt_rs,
	rd_rs_rt,
	addr,
	rs_rt_imm,
	rs_imm,
	rt_rs_imm,
	rt_imm,
	rt_imm_rs,
	rs_rt_addr,
	rs_addr
};

std::map<arg_format, int> arg_fmt_count = {
	{ rt_,         1 },
	{ rd_,         1 },
	{ rs_,         1 },
	{ rs_rt,       2 },
	{ rd_rs,       2 },
	{ rd_rt_shamt, 3 },
	{ rd_rt_rs,    3 },
	{ rd_rs_rt,    3 },
	{ addr,        1 },
	{ rs_rt_imm,   3 },
	{ rs_imm,      2 },
	{ rt_rs_imm,   3 },
	{ rt_imm,      2 },
	{ rt_imm_rs,   3 },
	{ rs_rt_addr,  3 },
	{ rs_addr,     2 }
};

inline bool valid_arg_count(arg_format fmt, int n) {
	return n == arg_fmt_count.at(fmt);
}

std::map<std::string, std::pair<std::bitset<6>, arg_format>> opcode_table = {
	{ "ADDI" , { 0b001000 , rt_rs_imm    }},
	{ "ANDI" , { 0b001100 , rt_rs_imm    }},
	{ "BEQ"  , { 0b000100 , rs_rt_addr   }},
	{ "BGEZ" , { 0b001100 , rs_addr      }},
	{ "BGTZ" , { 0b000111 , rs_addr      }},
	{ "BLEZ" , { 0b000110 , rs_addr      }},
	{ "BNE"  , { 0b000101 , rs_rt_addr   }},
	{ "LW"   , { 0b100011 , rt_imm_rs    }},
	{ "SW"   , { 0b101011 , rt_imm_rs    }},
	{ "ORI"  , { 0b001101 , rt_rs_imm    }},
	{ "XORI" , { 0b001110 , rt_rs_imm    }},
	{ "J"    , { 0b000010 , addr         }},
	{ "JAL"  , { 0b000011 , addr         }}
};

std::map<std::string, std::pair<std::bitset<6>, arg_format>> funct_table = {
	{ "ADD"  , { 0b100000 , rd_rs_rt    }},
	{ "AND"  , { 0b100100 , rd_rs_rt    }},
	{ "DIV"  , { 0b011010 , rs_rt       }},
	{ "JR"   , { 0b001000 , rs_         }},
	{ "JALR" , { 0b001001 , rd_rs       }},
	{ "MFHI" , { 0b010000 , rd_         }},
	{ "MFLO" , { 0b010010 , rd_         }},
	{ "MULT" , { 0b011001 , rs_rt       }},
	{ "OR"   , { 0b100101 , rd_rs_rt    }},
	{ "SLL"  , { 0b000000 , rd_rt_shamt }},
	{ "SLT"  , { 0b101010 , rd_rs_rt    }},
	{ "SLTU" , { 0b101011 , rd_rs_rt    }},
	{ "SRL"  , { 0b000010 , rd_rt_shamt }},
	{ "SUB"  , { 0b100010 , rd_rs_rt    }},
	{ "XOR"  , { 0b100110 , rd_rs_rt    }}
};

std::vector<std::string> split(const std::string & s, const char delim) {
	std::vector<std::string> ls;
	for (int pos = 0, offset = 0; pos != std::string::npos; offset = pos + 1) {
		pos = s.find(delim, offset);
		ls.push_back(s.substr(offset, pos - offset));
	}
	return ls;
}

std::bitset<32> make_R(std::bitset<32> rs, std::bitset<32> rt, std::bitset<32> rd, std::bitset<32> shamt, std::bitset<32> funct) {
	std::bitset<32> instruction = rs << 21 | rt << 16 | rd << 11 | shamt << 6 | funct;
	return instruction;
}

std::bitset<32> make_I(std::bitset<32> opcode, std::bitset<32> rs, std::bitset<32> rt, std::bitset<32> imm) {
	std::bitset<32> instruction = opcode << 26 | rs << 21 | rt << 16 | imm;
	return instruction;
}

int main(int argc, char * argv[]) {

	if (argc != 2) {
		std::cout << "Arguments not supported" << std::endl;
		exit(1);
	}

	std::string filename(argv[1]);
	std::ifstream src_stream(filename);

	if (!src_stream.is_open()) {
		std::cout << "Could not open file " << filename << std::endl;
		exit(1);
	}

	int linenumber = 0;
	std::vector<std::bitset<32>> assembly;

	// tuple format := linenumber, label, is_relative?
	std::vector<std::tuple<int, std::string, bool>> unresolved_jumps; 
	std::map<std::string, int> labels;
	std::string line;
	bool error_occurred = false;
	while (std::getline(src_stream, line) && !error_occurred) {
		// ignore empty lines
		if (line.size() == 0) continue;

		// break line up into tokens using spaces
		std::vector<std::string> tokens, uncleaned_tokens = split(line, ' ');

		// remove comments
		for (auto s : uncleaned_tokens) {
			if (s.front() != ';') {
				tokens.push_back(s);
			}
		}

		// Hopefully that line only had comments on it
		if (tokens.size() == 0) continue;

		// check if label
		if (tokens.size() == 1) {
			labels.insert({ tokens.at(0), linenumber });
			continue;
		}

		std::string instruction_str = tokens.at(0);
		int rt = 0, rd = 0, rs = 0, shamt = 0, imm = 0, opcode = 0, funct = 0;
		std::string address;
		bool relative_addr = false;

		std::transform(instruction_str.begin(), instruction_str.end(), instruction_str.begin(), ::toupper);

		arg_format format;

		auto f_iter = funct_table.find(instruction_str);
		bool is_funct = f_iter != funct_table.end();

		auto o_iter = opcode_table.find(instruction_str);
		bool is_opcode = o_iter != opcode_table.end();

		// verify and check instruction format
		if (is_funct) {
			auto iter = f_iter;
			format = iter->second.second;

			opcode = 0;
			funct = iter->second.first.to_ulong();
		}
		else if (is_opcode) {
			auto iter = o_iter;
			format = iter->second.second;

			opcode = iter->second.first.to_ulong();
			funct = 0;
		}
		else {
			std::cout << "Unknown instruction: " << instruction_str << std::endl;
			error_occurred = true;
			continue;
		}

		// validate amount of tokens
		if (!valid_arg_count(format, tokens.size())) {
			std::cout << "Invalid usage of instruction: " << instruction_str
				<< "\nWas expecting " << arg_fmt_count.at(format) << " got " << tokens.size() << " instead" << std::endl;
			error_occurred = true;
			continue;
		}

		// perform parsing
		// TODO need to handle errors with stoi
		switch (format) {
		case rt_:
			rt = stoi(tokens.at(1));
			break;

		case rd_:
			rd = stoi(tokens.at(1));
			break;

		case rs_:
			rs = stoi(tokens.at(1));
			break;

		case rs_rt:
			rs = stoi(tokens.at(1));
			rt = stoi(tokens.at(2));
			break;

		case rd_rt_shamt:
			rd = stoi(tokens.at(1));
			rt = stoi(tokens.at(2));
			shamt = stoi(tokens.at(3));
			break;

		case rd_rs:
			rd = stoi(tokens.at(1));
			rs = stoi(tokens.at(2));
			break;

		case rd_rt_rs:
			rd = stoi(tokens.at(1));
			rt = stoi(tokens.at(2));
			rs = stoi(tokens.at(3));
			break;

		case rd_rs_rt:
			rd = stoi(tokens.at(1));
			rs = stoi(tokens.at(2));
			rt = stoi(tokens.at(3));
			break;

		case addr:
			address = tokens.at(1);
			break;

		case rs_addr:
			rs = stoi(tokens.at(1));
			address = tokens.at(2);
			relative_addr = true;
			break;

		case rs_rt_addr:
			rs = stoi(tokens.at(1));
			rt = stoi(tokens.at(2));
			address = tokens.at(3);
			relative_addr = true;
			break;

		case rs_imm:
			rs = stoi(tokens.at(1));
			imm = stoi(tokens.at(2));
			break;

		case rs_rt_imm:
			rs = stoi(tokens.at(1));
			rt = stoi(tokens.at(2));
			imm = stoi(tokens.at(3));
			break;

		case rt_rs_imm:
			rt = stoi(tokens.at(1));
			rs = stoi(tokens.at(2));
			imm = stoi(tokens.at(3));
			break;

		case rt_imm:
			rt = stoi(tokens.at(1));
			imm = stoi(tokens.at(2));
			break;

		case rt_imm_rs:
			rt = stoi(tokens.at(1));
			imm = stoi(tokens.at(2));
			rs = stoi(tokens.at(3));
			break;
		}

		std::bitset<32> instruction;

		if (is_funct) {
			instruction = make_R(rs, rt, rd, shamt, funct);
		}
		else if (is_opcode) {
			instruction = make_I(opcode, rs, rt, imm);
		}

		// store jump to be resolved later
		if (!address.empty()) {
			unresolved_jumps.push_back( { linenumber, address, relative_addr} );
		}

		assembly.push_back(instruction);
		linenumber++;
	}

	for (auto e : unresolved_jumps) {
		int linenumber = std::get<0>(e);
		std::string label = std::get<1>(e);
		bool relative_addr = std::get<2>(e);

		// TODO handle error here when label cannot be found
		int offset;
		try {
			offset = labels.at(label) - linenumber - 1;
		}
		catch (std::out_of_range e) {
			std::cout << "Label: " << label << " could not be resolved. Are you sure you defined it?" << std::endl;
			error_occurred = true;
			continue;
		}

		if (relative_addr) {
			std::bitset<32> imm = std::bitset<16>(std::bitset<16>().flip().to_ulong() + offset + 1).to_ulong();
			assembly.at(linenumber) |= imm;
		}
		else {
			// no need for sign manipulation
			assembly.at(linenumber) |= labels.at(label);
		}

	}

	if (error_occurred) {
		std::cout << "Could not assemble program: " << filename << std::endl;
		return -1;
	}

	// print to output file
	std::string output_filename(filename.append(".bin"));
	std::ofstream out_stream(output_filename);

	if (!out_stream.is_open()) {
		std::cout << "Could not open file " << output_filename << " for writing" << std::endl;
		exit(1);
	}

	for (auto i : assembly) {
		out_stream << i << "\n";
	}

	return 0;
}
