#include <bitset>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

#define MemSize \
    1000  // memory size, in reality, the memory size should be 2^32, but for
          // this lab, for the space resaon, we keep it as this large number,
          // but the memory is still 32-bit addressable.

struct IFStruct {
    bitset<32> PC;
    bool nop = false;
};

struct IDStruct {
    bitset<32> Instr;
    bool nop = false;
};

struct EXStruct {
    bitset<32> Operand1;
    bitset<32> Operand2;
    bitset<32> StData;
    bitset<5> Rs1;
    bitset<5> Rs2;
    bitset<5> DestReg;
    bitset<4> AluControl;
    bool UpdatePC = false;
    bool wrt_enable = false;
    bool rd_mem = false;
    bool wrt_mem = false;
    bool Halt = false;
    bool nop = false;
};

struct MEMStruct {
    bitset<32> ALUresult;
    bitset<32> Store_data;
    bitset<5> Rs1;
    bitset<5> Rs2;
    bitset<5> Wrt_reg_addr;
    bool rd_fin = false;
    bool rd_mem = false;
    bool wrt_mem = false;
    bool wrt_enable = false;
    bool nop = false;
};

struct WBStruct {
    bitset<32> Wrt_data;
    bitset<5> Rs1;
    bitset<5> Rs2;
    bitset<5> Wrt_reg_addr;
    bool wrt_enable = false;
    bool nop = false;
};

struct stateStruct {
    IFStruct IF;
    IDStruct ID;
    EXStruct EX;
    MEMStruct MEM;
    WBStruct WB;
};

class InsMem {
   public:
    int instr_num = 0;
    string id, ioDir;
    InsMem(string name, string ioDir) {
        id = name;
        IMem.resize(MemSize);
        ifstream imem;
        string line;
        int i = 0;
        imem.open(ioDir + "\\imem.txt");
        if (imem.is_open()) {
            while (getline(imem, line)) {
                IMem[i] = bitset<8>(line);
                i++;
            }
            instr_num = i / 4;
        } else
            cout << "Unable to open IMEM input file.";
        imem.close();
    }

    bitset<32> readInstr(bitset<32> ReadAddress) {
        // read instruction memory
        // return bitset<32> val
        uint32_t instruction = 0;
        for (auto i = 0; i < 4; ++i) {
            uint32_t bit = IMem[ReadAddress.to_ulong() + i].to_ulong()
                           << (3 - i) * 8;
            instruction |= bit;
        }
        return instruction;
    }

   private:
    vector<bitset<8>> IMem;
};

class DataMem {
   public:
    string id, opFilePath, ioDir;
    DataMem(string name, string ioDir) : id{name}, ioDir{ioDir} {
        DMem.resize(MemSize);
        opFilePath = ioDir + "\\" + name + "_DMEMResult.txt";
        ifstream dmem;
        string line;
        int i = 0;
        dmem.open(ioDir + "\\dmem.txt");
        if (dmem.is_open()) {
            while (getline(dmem, line)) {
                DMem[i] = bitset<8>(line);
                i++;
            }
        } else
            cout << "Unable to open DMEM input file.";
        dmem.close();
    }

    bitset<32> readDataMem(bitset<32> Address) {
        // read data memory
        // return bitset<32> val
        uint32_t data_mem = 0;
        for (auto i = 0; i < 4; ++i) {
            uint32_t bit = DMem[Address.to_ulong() + i].to_ulong()
                           << (3 - i) * 8;
            data_mem |= bit;
        }
        return data_mem;
    }

    void writeDataMem(bitset<32> Address, bitset<32> WriteData) {
        // write into memory
        uint32_t data_mem = WriteData.to_ulong();
        for (auto i = 0; i < 4; ++i) {
            DMem[Address.to_ulong() + i] =
                (data_mem >> (3 - i) * 8) & 0b11111111;
        }
    }

    void outputDataMem() {
        ofstream dmemout;
        dmemout.open(opFilePath, std::ios_base::trunc);
        if (dmemout.is_open()) {
            for (int j = 0; j < 1000; j++) {
                dmemout << DMem[j] << endl;
            }
        } else
            cout << "Unable to open " << id << " DMEM result file." << endl;
        dmemout.close();
    }

   private:
    vector<bitset<8>> DMem;
};

class RegisterFile {
   public:
    string outputFile;
    RegisterFile(string ioDir) : outputFile{ioDir + "RFResult.txt"} {
        Registers.resize(32);
        Registers[0] = bitset<32>(0);
    }

    bitset<32> readRF(bitset<5> Reg_addr) {
        // Fill in
        return Registers[Reg_addr.to_ulong()];
    }

    void writeRF(bitset<5> Reg_addr, bitset<32> Wrt_reg_data) {
        // Fill in
        Registers[Reg_addr.to_ulong()] = Wrt_reg_data;
    }

    void outputRF(int cycle) {
        ofstream rfout;
        if (cycle == 0)
            rfout.open(outputFile, std::ios_base::trunc);
        else
            rfout.open(outputFile, std::ios_base::app);
        if (rfout.is_open()) {
            rfout << "State of RF after executing cycle:\t" << cycle << endl;
            for (int j = 0; j < 32; j++) {
                rfout << Registers[j] << endl;
            }
        } else
            cout << "Unable to open RF output file." << endl;
        rfout.close();
    }

   private:
    vector<bitset<32>> Registers;
};

class Core {
   public:
    RegisterFile myRF;
    uint32_t cycle = 0;
    bool halted = false;
    string ioDir;
    struct stateStruct state, nextState;
    InsMem &ext_imem;
    DataMem &ext_dmem;

    Core(string ioDir, InsMem &imem, DataMem &dmem)
        : myRF(ioDir), ioDir{ioDir}, ext_imem{imem}, ext_dmem{dmem} {}

    uint32_t imm_gen(uint32_t opcode) {
        bitset<32> instr = this->nextState.ID.Instr;
        uint32_t imm = 0;
        switch (opcode) {
            case 19:
                imm = (instr.to_ulong() >> 20) & 0b111111111111;
                if (instr[31] == 1) {
                    imm |= 0b11111111111111111111000000000000;
                }
                break;
            case 35:
                imm = (((instr.to_ulong() >> 25) & 0b1111111) << 5) |
                      ((instr.to_ulong() >> 7) & 0b11111);
                if (instr[31] == 1) {
                    imm |= 0b11111111111111111111000000000000;
                }
                break;
            case 99:
                imm = (instr[31] << 12) | (instr[7] << 11) |
                      (((instr.to_ulong() >> 25) & 0b111111) << 5) |
                      (((instr.to_ulong() >> 8) & 0b1111) << 1) | 0;
                if (instr[31] == 1) {
                    imm |= 0b11111111111111111110000000000000;
                }
                break;
            case 111:
                imm = (instr[31] << 20) |
                      (((instr.to_ulong() >> 12) & 0b11111111) << 12) |
                      (instr[20] << 11) |
                      (((instr.to_ulong() >> 21) & 0b1111111111) << 1) | 0;
                if (instr[31] == 1) {
                    imm |= 0b11111111111000000000000000000000;
                }
                break;
            default:
                cout << "error when generating imm"
                     << endl;  // exception message
                exit(0);
        }
        return imm;
    }

    void parse_func3(uint32_t func3) {
        switch (func3) {
            case 0:  // 000 add
                this->nextState.EX.AluControl = 0b0010;
                break;
            case 4:  // 100 xor
                //  I fabricate an alucontrol because I cannot find the code
                this->nextState.EX.AluControl = 0b0011;
                break;
            case 6:  // 110 or
                this->nextState.EX.AluControl = 0b0001;
                break;
            case 7:  // 111 and
                this->nextState.EX.AluControl = 0b0000;
                break;
            default:
                cout << "wrong func3" << endl;  // exception message
                exit(0);
        }
        return;
    }

    virtual void step() {}

    virtual void printState() {}
};

class SingleStageCore : public Core {
   public:
    SingleStageCore(string ioDir, InsMem &imem, DataMem &dmem)
        : Core(ioDir + "\\SS_", imem, dmem),
          opFilePath(ioDir + "\\StateResult_SS.txt") {}

    void fetch() {
        if (this->state.IF.nop == true) {
            return;
        }
        this->nextState.ID.Instr = this->ext_imem.readInstr(this->state.IF.PC);
        this->nextState.IF.PC = this->state.IF.PC.to_ulong() + 4;
        // cout << "Cycle: " << cycle << " Fetch" << endl;
        return;
    }

    void decode() {
        if (this->state.ID.nop == true) {
            return;
        }
        if (this->nextState.ID.Instr.all()) {
            this->nextState.IF.nop = true;
            this->nextState.ID.nop = true;
            this->nextState.EX.nop = true;
            this->nextState.MEM.nop = true;
            this->nextState.WB.nop = true;
            this->nextState.IF.PC = this->state.IF.PC;
            this->state.EX.nop = true;
            this->state.MEM.nop = true;
            this->state.WB.nop = true;
            return;
        }
        uint32_t instr = this->nextState.ID.Instr.to_ulong();
        uint32_t opcode = instr & 0b1111111;
        this->nextState.EX.DestReg = (instr >> 7) & 0b11111;
        this->nextState.EX.Rs1 = (instr >> 15) & 0b11111;
        this->nextState.EX.Rs2 = (instr >> 20) & 0b11111;
        this->nextState.EX.Operand1 = this->myRF.readRF(this->nextState.EX.Rs1);
        switch (opcode) {
            case 51:  // 0110011 R-type
                this->nextState.EX.wrt_enable = true;
                this->nextState.EX.UpdatePC = false;
                this->nextState.EX.rd_mem = false;
                this->nextState.EX.wrt_mem = false;
                this->nextState.EX.Operand2 =
                    this->myRF.readRF(this->nextState.EX.Rs2);
                if (this->nextState.ID.Instr[30] == 1) {
                    this->nextState.EX.AluControl = 0b0110;
                } else {
                    parse_func3((instr >> 12) & 0b111);
                }
                break;
            case 19:  // 0010011 I-type general
                this->nextState.EX.wrt_enable = true;
                this->nextState.EX.UpdatePC = false;
                this->nextState.EX.rd_mem = false;
                this->nextState.EX.wrt_mem = false;
                this->nextState.EX.Operand2 = imm_gen(19);
                parse_func3((instr >> 12) & 0b111);
                break;
            case 111:  // 1101111 J-type
                this->nextState.EX.rd_mem = false;
                this->nextState.EX.wrt_enable = false;
                this->nextState.EX.wrt_mem = false;
                this->nextState.EX.UpdatePC = true;
                // store pc + 4
                myRF.writeRF(this->nextState.EX.DestReg, this->nextState.IF.PC);
                this->nextState.IF.PC =
                    this->state.IF.PC.to_ulong() + imm_gen(111);
                this->state.EX.nop = true;
                this->state.MEM.nop = true;
                this->state.WB.nop = true;
                break;
            case 99: {  // 1100011 B-type
                this->nextState.EX.rd_mem = false;
                this->nextState.EX.wrt_enable = false;
                this->nextState.EX.wrt_mem = false;
                this->nextState.EX.UpdatePC = false;
                this->nextState.EX.Operand2 =
                    this->myRF.readRF(this->nextState.EX.Rs2);
                bool is_equal =
                    this->nextState.EX.Operand1 == this->nextState.EX.Operand2;
                bool flag = this->nextState.ID.Instr[12];  // bne or beq
                if (is_equal ^ flag) {
                    this->nextState.IF.PC =
                        this->state.IF.PC.to_ulong() + imm_gen(99);
                    this->nextState.EX.UpdatePC = true;
                }
                this->state.EX.nop = true;
                this->state.MEM.nop = true;
                this->state.WB.nop = true;
                break;
            }
            case 3:  // 0000011 I-type lw
                this->nextState.EX.rd_mem = true;
                this->nextState.EX.wrt_enable = true;
                this->nextState.EX.UpdatePC = false;
                this->nextState.EX.wrt_mem = false;
                // the same as I-type general, so use the same opcode
                this->nextState.EX.Operand2 = imm_gen(19);
                this->nextState.EX.AluControl = 0b0010;
                break;
            case 35:  // 0100011 S-type sw
                this->nextState.EX.wrt_mem = true;
                this->nextState.EX.UpdatePC = false;
                this->nextState.EX.rd_mem = false;
                this->nextState.EX.wrt_enable = false;
                this->nextState.EX.Operand2 = imm_gen(35);
                this->nextState.EX.AluControl = 0b0010;
                break;
            default:
                cout << "wrong opcode" << endl;  // exception message
                exit(0);
        }
        // cout << "Cycle: " << cycle << " Decode" << endl;
        return;
    }

    void execute() {
        if (this->state.EX.nop == true) {
            return;
        }
        this->nextState.MEM.Rs1 = this->nextState.EX.Rs1;
        this->nextState.MEM.Rs2 = this->nextState.EX.Rs2;
        this->nextState.MEM.Wrt_reg_addr = this->nextState.EX.DestReg;
        this->nextState.MEM.rd_mem = this->nextState.EX.rd_mem;
        this->nextState.MEM.wrt_mem = this->nextState.EX.wrt_mem;
        this->nextState.MEM.wrt_enable = this->nextState.EX.wrt_enable;
        this->nextState.MEM.Store_data = myRF.readRF(this->nextState.EX.Rs2);
        uint32_t alu_ctrl = this->nextState.EX.AluControl.to_ulong();
        uint32_t oprand1 = this->nextState.EX.Operand1.to_ulong();
        uint32_t oprand2 = this->nextState.EX.Operand2.to_ulong();
        switch (alu_ctrl) {
            case 2:  // add 0b0010
                this->nextState.MEM.ALUresult = oprand1 + oprand2;
                break;
            case 3:  // xor 0b0011
                //  I fabricate an alucontrol because I fail to find the code
                this->nextState.MEM.ALUresult = oprand1 ^ oprand2;
                break;
            case 1:  // or 0b0001
                this->nextState.MEM.ALUresult = oprand1 | oprand2;
                break;
            case 0:  // and 0b0000
                this->nextState.MEM.ALUresult = oprand1 & oprand2;
                break;
            case 6:  // sub 0b0110
                this->nextState.MEM.ALUresult = oprand1 - oprand2;
                break;
            default:
                cout << "wrong aluoperation" << endl;  // exception message
                exit(0);
        }
        // cout << "Cycle: " << cycle << " Execute" << endl;
        return;
    }

    void memory_access() {
        if (this->state.MEM.nop == true) {
            return;
        }
        this->nextState.WB.Rs1 = this->nextState.MEM.Rs1;
        this->nextState.WB.Rs2 = this->nextState.MEM.Rs2;
        this->nextState.WB.Wrt_reg_addr = this->nextState.MEM.Wrt_reg_addr;
        this->nextState.WB.wrt_enable = this->nextState.MEM.wrt_enable;
        if (this->nextState.MEM.rd_mem == true) {
            this->nextState.WB.Wrt_data =
                ext_dmem.readDataMem(this->nextState.MEM.ALUresult);
        } else {
            this->nextState.WB.Wrt_data = this->nextState.MEM.ALUresult;
        }
        if (this->nextState.MEM.wrt_mem == true) {
            ext_dmem.writeDataMem(this->nextState.MEM.ALUresult,
                                  this->nextState.MEM.Store_data);
        }
        // cout << "Cycle: " << cycle << " Mem" << endl;
        return;
    }

    void write_back() {
        if (this->state.WB.nop == true) {
            return;
        }
        if (this->nextState.WB.wrt_enable == true &&
            this->nextState.WB.Wrt_reg_addr.any()) {
            myRF.writeRF(this->nextState.WB.Wrt_reg_addr,
                         this->nextState.WB.Wrt_data);
        }
        // cout << "Cycle: " << cycle << " Write_back" << endl;
        return;
    }

    void step() {
        /* Your implementation*/
        fetch();
        decode();
        execute();
        memory_access();
        write_back();

        if (state.IF.nop) {
            halted = true;
        }

        myRF.outputRF(cycle);  // dump RF
        printState(nextState,
                   cycle);  // print states after executing cycle 0,
                            // cycle 1, cycle 2 ...

        this->state = this->nextState;  // The end of the cycle and updates
                                        // the current state with the values
                                        // calculated in this cycle
        cycle++;
    }

    void printState(stateStruct state, int cycle) {
        ofstream printstate;
        if (cycle == 0)
            printstate.open(opFilePath, std::ios_base::trunc);
        else
            printstate.open(opFilePath, std::ios_base::app);
        if (printstate.is_open()) {
            printstate << "State after executing cycle:\t" << cycle << endl;

            printstate << "IF.PC:\t" << state.IF.PC.to_ulong() << endl;
            printstate << "IF.nop:\t" << state.IF.nop << endl;
        } else
            cout << "Unable to open SS StateResult output file." << endl;
        printstate.close();
    }

   private:
    string opFilePath;
};

class FiveStageCore : public Core {
   public:
    FiveStageCore(string ioDir, InsMem &imem, DataMem &dmem)
        : Core(ioDir + "\\FS_", imem, dmem),
          opFilePath(ioDir + "\\StateResult_FS.txt") {
        this->state.ID.nop = true;
        this->state.EX.nop = true;
        this->state.MEM.nop = true;
        this->state.WB.nop = true;
    }

    void fetch() {
        if (this->nextState.EX.Halt == true ||
            this->nextState.EX.UpdatePC == true) {
            return;
        }

        if (this->state.IF.nop != true) {
            this->nextState.ID.Instr =
                this->ext_imem.readInstr(this->state.IF.PC);
            this->nextState.IF.PC = this->state.IF.PC.to_ulong() + 4;
            if (this->nextState.ID.Instr.all()) {
                this->nextState.ID = this->state.ID;
                this->state.IF.nop = true;
                this->nextState.IF.PC = this->state.IF.PC;
                this->nextState.IF.nop = true;
            }
            cout << "Cycle: " << cycle << " Fetch" << endl;
        }
        this->nextState.ID.nop = this->state.IF.nop;
        return;
    }

    void decode() {
        this->nextState.EX.UpdatePC = false;
        if (this->state.ID.nop != true) {
            uint32_t instr = this->nextState.ID.Instr.to_ulong();
            uint32_t opcode = instr & 0b1111111;
            this->nextState.EX.DestReg = (instr >> 7) & 0b11111;
            this->nextState.EX.Rs1 = (instr >> 15) & 0b11111;
            this->nextState.EX.Rs2 = (instr >> 20) & 0b11111;
            // data hazard
            if (this->nextState.EX.Halt == false &&
                this->nextState.MEM.rd_mem == true &&
                this->nextState.MEM.wrt_enable &&
                this->nextState.MEM.Wrt_reg_addr.any()) {
                if (this->state.EX.DestReg == this->nextState.EX.Rs1 ||
                    this->state.EX.DestReg == this->nextState.EX.Rs2) {
                    this->nextState.IF = this->state.IF;
                    this->nextState.ID = this->state.ID;
                    this->nextState.EX = this->state.EX;
                    this->nextState.EX.Halt = true;
                    return;
                }
            }

            this->nextState.EX.Operand1 =
                this->myRF.readRF(this->nextState.EX.Rs1);
            this->nextState.EX.Operand2 =
                this->myRF.readRF(this->nextState.EX.Rs2);
            this->nextState.EX.StData = 0;

            if (this->nextState.WB.wrt_enable &&
                this->nextState.WB.Wrt_reg_addr.any()) {
                if (this->nextState.WB.Wrt_reg_addr == this->nextState.EX.Rs1) {
                    this->nextState.EX.Operand1 = this->nextState.WB.Wrt_data;
                    cout << "MEM/WB Fwd A" << endl;
                }
                if (this->nextState.WB.Wrt_reg_addr == this->nextState.EX.Rs2) {
                    if (opcode == 35) {
                        this->nextState.EX.StData = this->nextState.WB.Wrt_data;
                    } else {
                        this->nextState.EX.Operand2 =
                            this->nextState.WB.Wrt_data;
                    }
                    cout << "MEM/WB Fwd B" << endl;
                }
            }
            if (this->nextState.EX.Halt == false &&
                this->nextState.MEM.wrt_enable &&
                this->nextState.MEM.Wrt_reg_addr.any()) {
                if (this->nextState.MEM.Wrt_reg_addr ==
                    this->nextState.EX.Rs1) {
                    this->nextState.EX.Operand1 = this->nextState.MEM.ALUresult;
                    cout << "EX/MEM Fwd A" << endl;
                }
                if (this->nextState.MEM.Wrt_reg_addr ==
                    this->nextState.EX.Rs2) {
                    if (opcode == 35) {
                        this->nextState.EX.StData =
                            this->nextState.MEM.ALUresult;
                    } else {
                        this->nextState.EX.Operand2 =
                            this->nextState.MEM.ALUresult;
                    }
                    cout << "EX/MEM Fwd B" << endl;
                }
            }
            this->nextState.EX.Halt = false;

            switch (opcode) {
                case 51:  // 0110011 R-type
                    this->nextState.EX.wrt_enable = true;
                    this->nextState.EX.rd_mem = false;
                    this->nextState.EX.wrt_mem = false;
                    if (this->nextState.ID.Instr[30] == 1) {
                        this->nextState.EX.AluControl = 0b0110;
                    } else {
                        parse_func3((instr >> 12) & 0b111);
                    }
                    break;
                case 19:  // 0010011 I-type general
                    this->nextState.EX.wrt_enable = true;
                    this->nextState.EX.rd_mem = false;
                    this->nextState.EX.wrt_mem = false;
                    this->nextState.EX.Operand2 = imm_gen(19);
                    parse_func3((instr >> 12) & 0b111);
                    break;
                case 111:  // 1101111 J-type
                    this->nextState.EX.wrt_enable = true;
                    this->nextState.EX.rd_mem = false;
                    this->nextState.EX.wrt_mem = false;
                    this->nextState.EX.UpdatePC = true;
                    // store pc + 4
                    this->nextState.EX.Operand1 = this->state.IF.PC;
                    this->nextState.EX.Operand2 = 0;
                    this->nextState.IF.PC =
                        this->state.IF.PC.to_ulong() - 4 + imm_gen(111);
                    this->nextState.EX.AluControl = 0b0010;
                    // cope ID state
                    this->nextState.ID.nop = true;
                    this->nextState.ID.Instr = this->state.ID.Instr;
                    // this->state.ID.nop = true;  // used as stop for test
                    break;
                case 99: {  // 1100011 B-type
                    this->nextState.EX.rd_mem = false;
                    this->nextState.EX.wrt_enable = false;
                    this->nextState.EX.wrt_mem = false;
                    bool is_equal = this->nextState.EX.Operand1 ==
                                    this->nextState.EX.Operand2;
                    bool flag = this->nextState.ID.Instr[12];  // bne or beq
                    if (is_equal ^ flag) {
                        this->nextState.IF.PC =
                            this->state.IF.PC.to_ulong() - 4 + imm_gen(99);
                        // cout << nextState.IF.PC << endl;
                        // cout << nextState.IF.nop << endl;
                        this->nextState.EX.UpdatePC = true;
                        this->nextState.ID.nop = true;
                        this->nextState.ID.Instr = this->state.ID.Instr;
                    }
                    this->state.ID.nop = true;
                    // for simplicity set this->nextState.EX.nop by
                    // this->state.ID.nop
                    break;
                }
                case 3:  // 0000011 I-type lw
                    this->nextState.EX.rd_mem = true;
                    this->nextState.EX.wrt_enable = true;
                    this->nextState.EX.wrt_mem = false;
                    // the same as I-type general, so use the same opcode
                    this->nextState.EX.Operand2 = imm_gen(19);
                    this->nextState.EX.AluControl = 0b0010;
                    break;
                case 35:  // 0100011 S-type sw
                    this->nextState.EX.wrt_mem = true;
                    this->nextState.EX.rd_mem = false;
                    this->nextState.EX.wrt_enable = false;
                    this->nextState.EX.Operand2 = imm_gen(35);
                    this->nextState.EX.AluControl = 0b0010;
                    break;
                default:
                    cout << "wrong opcode" << endl;  // exception message
                    exit(0);
            }
            cout << "Cycle: " << cycle << " Decode" << endl;
        }
        this->nextState.EX.nop = this->state.ID.nop;
        return;
    }

    void execute() {
        if (this->state.EX.Halt == true) {
            this->nextState.MEM.nop = true;
            return;
        }
        if (this->state.EX.nop != true) {
            this->nextState.MEM.Rs1 = this->nextState.EX.Rs1;
            this->nextState.MEM.Rs2 = this->nextState.EX.Rs2;
            this->nextState.MEM.Wrt_reg_addr = this->nextState.EX.DestReg;
            this->nextState.MEM.rd_mem = this->nextState.EX.rd_mem;
            this->nextState.MEM.wrt_mem = this->nextState.EX.wrt_mem;
            this->nextState.MEM.wrt_enable = this->nextState.EX.wrt_enable;
            uint32_t alu_ctrl = this->nextState.EX.AluControl.to_ulong();
            uint32_t oprand1 = this->nextState.EX.Operand1.to_ulong();
            uint32_t oprand2 = this->nextState.EX.Operand2.to_ulong();
            this->nextState.MEM.Store_data = 0;
            if (this->nextState.MEM.wrt_mem) {
                if (this->nextState.EX.StData.any()) {
                    this->nextState.MEM.Store_data = this->nextState.EX.StData;
                } else {
                    this->nextState.MEM.Store_data =
                        this->myRF.readRF(this->nextState.EX.Rs2);
                }
            }
            switch (alu_ctrl) {
                case 2:  // add 0b0010
                    this->nextState.MEM.ALUresult = oprand1 + oprand2;
                    break;
                case 3:  // xor 0b0011
                    //  I fabricate an alucontrol because I fail to find the
                    //  code
                    this->nextState.MEM.ALUresult = oprand1 ^ oprand2;
                    break;
                case 1:  // or 0b0001
                    this->nextState.MEM.ALUresult = oprand1 | oprand2;
                    break;
                case 0:  // and 0b0000
                    this->nextState.MEM.ALUresult = oprand1 & oprand2;
                    break;
                case 6:  // sub 0b0110
                    this->nextState.MEM.ALUresult = oprand1 - oprand2;
                    break;
                default:
                    cout << "wrong aluoperation" << endl;  // exception message
                    exit(0);
            }
            cout << "Cycle: " << cycle << " Execute" << endl;
        }
        this->nextState.MEM.nop = this->state.EX.nop;
        return;
    }

    void memory_access() {
        if (this->state.MEM.nop != true) {
            this->nextState.WB.Rs1 = this->nextState.MEM.Rs1;
            this->nextState.WB.Rs2 = this->nextState.MEM.Rs2;
            this->nextState.WB.Wrt_reg_addr = this->nextState.MEM.Wrt_reg_addr;
            this->nextState.WB.wrt_enable = this->nextState.MEM.wrt_enable;
            if (this->nextState.MEM.rd_mem == true) {
                this->nextState.WB.Wrt_data =
                    ext_dmem.readDataMem(this->nextState.MEM.ALUresult);
            } else {
                this->nextState.WB.Wrt_data = this->nextState.MEM.ALUresult;
            }
            if (this->nextState.MEM.wrt_mem == true) {
                ext_dmem.writeDataMem(this->nextState.MEM.ALUresult,
                                      this->nextState.MEM.Store_data);
            }
            cout << "Cycle: " << cycle << " Mem" << endl;
        }
        this->nextState.WB.nop = this->state.MEM.nop;
        return;
    }

    void write_back() {
        if (this->state.WB.nop != true) {
            if (this->nextState.WB.wrt_enable == true &&
                this->nextState.WB.Wrt_reg_addr.any()) {
                myRF.writeRF(this->nextState.WB.Wrt_reg_addr,
                             this->nextState.WB.Wrt_data);
            }
            cout << "Cycle: " << cycle << " Write_back" << endl;
        }
        return;
    }

    void step() {
        /* Your implementation */
        /* --------------------- WB stage --------------------- */
        write_back();

        /* --------------------- MEM stage -------------------- */
        memory_access();

        /* --------------------- EX stage --------------------- */
        execute();

        /* --------------------- ID stage --------------------- */
        decode();

        /* --------------------- IF stage --------------------- */
        fetch();

        if (state.IF.nop && state.ID.nop && state.EX.nop && state.MEM.nop &&
            state.WB.nop)
            halted = true;

        myRF.outputRF(cycle);          // dump RF
        printState(nextState, cycle);  // print states after executing
                                       // cycle 0, cycle 1, cycle 2 ...

        this->state = this->nextState;  // The end of the cycle and updates the
                                        // current state with the values
                                        // calculated in this cycle

        cycle++;
    }

    void printState(stateStruct state, int cycle) {
        ofstream printstate;
        if (cycle == 0)
            printstate.open(opFilePath, std::ios_base::trunc);
        else
            printstate.open(opFilePath, std::ios_base::app);
        if (printstate.is_open()) {
            printstate
                << "-----------------------------------------------------"
                << endl;
            printstate << "State after executing cycle:\t" << cycle << endl;
            printstate << endl;

            printstate << "IF.PC:\t" << state.IF.PC.to_ulong() << endl;
            printstate << "IF.nop:\t" << state.IF.nop << endl;
            printstate << endl;

            printstate << "ID.Instr:\t" << state.ID.Instr << endl;
            printstate << "ID.nop:\t" << state.ID.nop << endl;
            printstate << endl;

            printstate << "EX.Operand1:\t" << state.EX.Operand1 << endl;
            printstate << "EX.Operand2:\t" << state.EX.Operand2 << endl;
            printstate << "EX.StData:\t" << state.EX.StData << endl;
            printstate << "EX.DestReg:\t" << state.EX.DestReg << endl;
            printstate << endl;
            printstate << "EX.UpdatePC:\t" << state.EX.UpdatePC << endl;
            printstate << "EX.WBEnable:\t" << state.EX.wrt_enable << endl;
            printstate << "EX.RdDMem:\t" << state.EX.rd_mem << endl;
            printstate << "EX.WrDMem:\t" << state.EX.wrt_mem << endl;
            printstate << "EX.Halt:\t" << state.EX.Halt << endl;
            printstate << "EX.nop:\t" << state.EX.nop << endl;
            printstate << endl;

            printstate << "MEM.ALUresult:\t" << state.MEM.ALUresult << endl;
            printstate << "MEM.Store_data:\t" << state.MEM.Store_data << endl;
            printstate << "MEM.Wrt_reg_addr:\t" << state.MEM.Wrt_reg_addr
                       << endl;
            printstate << "MEM.rd_mem:\t" << state.MEM.rd_mem << endl;
            printstate << "MEM.wrt_mem:\t" << state.MEM.wrt_mem << endl;
            printstate << "MEM.wrt_enable:\t" << state.MEM.wrt_enable << endl;
            printstate << "MEM.nop:\t" << state.MEM.nop << endl;
            printstate << endl;

            printstate << "WB.Wrt_data:\t" << state.WB.Wrt_data << endl;
            printstate << "WB.Wrt_reg_addr:\t" << state.WB.Wrt_reg_addr << endl;
            printstate << "WB.wrt_enable:\t" << state.WB.wrt_enable << endl;
            printstate << "WB.nop:\t" << state.WB.nop << endl;
        } else
            cout << "Unable to open FS StateResult output file." << endl;
        printstate.close();
    }

   private:
    string opFilePath;
};

void perf_mtx(SingleStageCore &SSCore, FiveStageCore &FSCore, string &ioDir,
              InsMem &imem) {
    ofstream perf_state;
    double instr_num = imem.instr_num;
    double ss_cycle = SSCore.cycle;
    double fs_cycle = FSCore.cycle;
    string path = ioDir + "/PerformanceMetrics_Result.txt";
    perf_state.open(path);
    if (perf_state.is_open()) {
        perf_state << "Relative IO Directory : " << path << endl;
        perf_state << "Single Stage Core Performance "
                      "Metrics-----------------------------"
                   << endl;
        perf_state << "Number of cycles taken: " << SSCore.cycle << endl;
        perf_state << "Cycles per instruction: " << (ss_cycle / instr_num)
                   << endl;
        perf_state << "Instructions per cycle : " << (instr_num / ss_cycle)
                   << endl;
        perf_state << endl;
        perf_state << "Single Stage Core Performance "
                      "Metrics-----------------------------"
                   << endl;
        perf_state << "Number of cycles taken: " << FSCore.cycle << endl;
        perf_state << "Cycles per instruction: " << (fs_cycle / instr_num)
                   << endl;
        perf_state << "Instructions per cycle : " << (instr_num / fs_cycle)
                   << endl;

    } else {
        cout << "Unable to open PerformanceMetrics_Result." << endl;
    }
    perf_state.close();
}

int main(int argc, char *argv[]) {
    string ioDir = "";
    if (argc == 1) {
        cout << "Enter path containing the memory files: ";
        cin >> ioDir;
    } else if (argc > 2) {
        cout << "Invalid number of arguments. Machine stopped." << endl;
        return -1;
    } else {
        ioDir = argv[1];
        cout << "IO Directory: " << ioDir << endl;
    }

    InsMem imem = InsMem("Imem", ioDir);
    DataMem dmem_ss = DataMem("SS", ioDir);
    DataMem dmem_fs = DataMem("FS", ioDir);

    SingleStageCore SSCore(ioDir, imem, dmem_ss);
    FiveStageCore FSCore(ioDir, imem, dmem_fs);

    while (1) {
        if (!SSCore.halted) SSCore.step();

        if (!FSCore.halted) FSCore.step();

        if (FSCore.halted) break;  // SSCore.halted &&
    }

    // dump SS and FS data mem.
    dmem_ss.outputDataMem();
    dmem_fs.outputDataMem();

    perf_mtx(SSCore, FSCore, ioDir, imem);

    return 0;
}