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
	rt_imm_rs
};

std::map<std::string, std::pair<std::bitset<6>, arg_format>> opcode_table = {
	{ "ADDI" , { 0b001000 , rt_rs_imm }},
	{ "ANDI" , { 0b001100 , rt_rs_imm }},
	{ "BEQ"  , { 0b000100 , rs_rt_imm }},
	{ "BGEZ" , { 0b001100 , rs_imm    }},
	{ "BGTZ" , { 0b000111 , rs_imm    }},
	{ "BLEZ" , { 0b000110 , rs_imm    }},
	{ "BNE"  , { 0b000101 , rs_rt_imm }},
	{ "LW"   , { 0b100011 , rt_imm_rs }},
	{ "SW"   , { 0b101011 , rt_imm_rs }},
	{ "ORI"  , { 0b001101 , rt_rs_imm }},
	{ "XORI" , { 0b001110 , rt_rs_imm }},
	{ "J"    , { 0b000010 , addr      }},
	{ "JAL"  , { 0b000011 , addr      }}
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

std::bitset<32> make_r_type(std::bitset<32> rs, std::bitset<32> rt, std::bitset<32> rd, std::bitset<32> shamt, std::bitset<32> funct) {
	std::bitset<32> instruction = rs << 21 | rt << 16 | rd << 11 | shamt << 6 | funct;
	return instruction;
}

std::bitset<32> make_i_type(std::bitset<32> opcode, std::bitset<32> rs, std::bitset<32> rt, std::bitset<32> imm) {
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
	std::vector<std::bitset<32>> wip_asm;
	std::map<int, std::string> unresolved_jumps;
	std::map<std::string, int> labels;
	std::string line;
	while (std::getline(src_stream, line)) {
		// ignore empty lines
		if (line.size() == 0) continue;

		// break line up into tokens using spaces
		std::vector<std::string> tokens = split(line, ' ');

		// check if label
		if (tokens.size() == 1) {
			labels.insert({ tokens.at(0), linenumber });
			continue;
		}

		std::string instruction_str = tokens.at(0);
		int rt = 0, rd = 0, rs = 0, shamt = 0, imm = 0, opcode = 0, funct = 0;
		std::string address;

		std::transform(instruction_str.begin(), instruction_str.end(), instruction_str.begin(), ::toupper);

		arg_format format;

		auto f_iter = funct_table.find(instruction_str);
		bool is_funct = f_iter != funct_table.end();

		auto o_iter = opcode_table.find(instruction_str);
		bool is_opcode = o_iter != opcode_table.end();

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
			instruction = make_r_type(rs, rt, rd, shamt, funct);
		}
		else if (is_opcode) {
			instruction = make_i_type(opcode, rs, rt, imm);
		}

		// store jump to be resolved later
		if (!address.empty()) {
			unresolved_jumps.insert( { instruction.to_ulong(), address } );
		}

		wip_asm.push_back(instruction);
		linenumber++;
	}

	// resolve all jumps
	std::vector<std::bitset<32>> assembly;
	for (auto e : wip_asm) {
		auto iter = unresolved_jumps.find(e.to_ulong());
		if (iter == unresolved_jumps.end()) {
			assembly.push_back(e);
		}
		else {
			std::string label = iter->second;
			std::bitset<32> address = labels.at(label);
			std::bitset<32> instruction = e | address;
			assembly.push_back(instruction);
		}
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
