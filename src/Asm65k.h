//
//  AsmA65k.h
//  AsmA65k - The assembler for the A65000 microprocessor
//
//  Created by Zoltán Majoros on 2013.05.16
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//
//  C++11
//

#pragma once

#include "Segment.h"
#include <iostream>
#include <vector>
#include <map>

using string = std::string;

struct AsmError
{
    AsmError(unsigned int lineNumber, string lineContent) : lineNumber(lineNumber),
                                                            lineContent(lineContent) {}

    AsmError(unsigned int lineNumber, string lineContent, string errorString) : lineNumber(lineNumber),
                                                                                lineContent(lineContent),
                                                                                errorMessage(errorString) {}

    unsigned int lineNumber;
    string lineContent;
    string errorMessage;
};

class AsmA65k
{
public:
    std::vector<Segment> *assemble(std::stringstream &source);

private:
    // constants, structs
    enum Directives
    {
        DIRECTIVE_NONE,   // return code for a line that doesn't contain a directive
        DIRECTIVE_SETPC,  // .pc = $1000
        DIRECTIVE_DEFINE, // .def PI = 314159265
        DIRECTIVE_TEXT,   // .text "Hello world!\n"
        DIRECTIVE_TEXTZ,  // .text "Hello world!\n", 0
        DIRECTIVE_BYTE,   // .byte 1, 2, 3, $4, %1100101
        DIRECTIVE_WORD,   // .word 1, 2, 3, $ffff, %1100101
        DIRECTIVE_DWORD   // .dword 1, 2, 3, $ffffffff, %1100101
    };

    enum OperandTypes
    {
        OT_NONE,

        // monadic
        OT_CONSTANT,                        // 1234
        OT_LABEL,                           // names
        OT_REGISTER,                        // r0
        OT_INDIRECT_CONSTANT,               // [1234]
        OT_INDIRECT_REGISTER,               // [r0]
        OT_INDIRECT_LABEL,                  // [names]
        OT_INDIRECT_REGISTER_PLUS_CONSTANT, // [r0 + 1234]
        OT_INDIRECT_CONSTANT_PLUS_REGISTER, // [1234 + r0]
        OT_INDIRECT_REGISTER_PLUS_LABEL,    // [r0 + names]
        OT_INDIRECT_LABEL_PLUS_REGISTER,    // [names + r0]

        // diadic
        OT_REGISTER__REGISTER,                        // r0, r1               !
        OT_REGISTER__CONSTANT,                        // r0, 33               !
        OT_REGISTER__LABEL,                           // r0, names            !
        OT_INDIRECT_REGISTER__REGISTER,               // [r0], r1             !
        OT_INDIRECT_LABEL__REGISTER,                  // [names], r0          !
        OT_INDIRECT_CONSTANT__REGISTER,               // [$1234], r0          !
        OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER,    // [r0 + names], r1     !
        OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER, // [r0 + 1234], r1      !
        OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER,    // [names + r0], r1     !
        OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER, // [$2344 + r0], r1     !

        OT_INDIRECT_REGISTER__CONSTANT,               // [r0], 64
        OT_INDIRECT_LABEL__CONSTANT,                  // [names], 64
        OT_INDIRECT_CONSTANT__CONSTANT,               // [$1234], 64
        OT_INDIRECT_REGISTER_PLUS_LABEL__CONSTANT,    // [r0 + names], 64
        OT_INDIRECT_REGISTER_PLUS_CONSTANT__CONSTANT, // [r0 + 1234], 64
        OT_INDIRECT_LABEL_PLUS_REGISTER__CONSTANT,    // [names + r0], 64
        OT_INDIRECT_CONSTANT_PLUS_REGISTER__CONSTANT, // [$2344 + r0], 64

        OT_REGISTER__INDIRECT_REGISTER,               // r0, [r1]             !
        OT_REGISTER__INDIRECT_LABEL,                  // r0, [names]          !
        OT_REGISTER__INDIRECT_CONSTANT,               // r0, [1234]           !
        OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT, // r0, [r1 + 1234]      !
        OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL,    // r0, [r1 + names]     !
        OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER, // r0, [1234 + r1]      !
        OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER,    // r0, [names + r1]     !
        OT_CONSTANT__LABEL,                           // 1234, label
        OT_CONSTANT__CONSTANT,                        // 1234, 5678
        OT_LABEL__CONSTANT,                           // label, 1234
        OT_LABEL__LABEL,                              // label1, label2
    };

    enum AddressingModes
    {
        AM_IMPLIED = 0,             // -no operand-
        AM_REG_IMMEDIATE,           // Rx, const
        AM_CONST_IMMEDIATE,         // const
        AM_REGISTER1,               // Rx
        AM_REGISTER2,               // Rx, Ry
        AM_ABSOLUTE1,               // [Address]
        AM_ABSOLUTE_SRC,            // Rx, [Address]
        AM_ABSOLUTE_DEST,           // [Address], Rx
        AM_ABSOLUTE_CONST,          // [Address], const       !
        AM_REGISTER_INDIRECT1,      // [Rx]
        AM_REGISTER_INDIRECT_SRC,   // Rx, [Ry]
        AM_REGISTER_INDIRECT_DEST,  // [Rx], Ry
        AM_REGISTER_INDIRECT_CONST, // [Rx], const            !
        AM_INDEXED1,                // [Rx + const]
        AM_INDEXED_SRC,             // Rx, [Ry + const]
        AM_INDEXED_DEST,            // [Rx + const], Ry
        AM_INDEXED_CONST,           // [Rx + const], const    !
        AM_RELATIVE,                // branch
        AM_DIRECT,                  // direct
        AM_SYSCALL,                 // syscall
        AM_AMBIGOUS,
    };

    enum RegisterConfigurations
    {
        RC_NOREGISTER = 0,
        RC_REGISTER = 1,
        RC_2REGISTERS = 2,
        RC_REGISTER_POSTINCREMENT = 3,
        RC_2REGISTERS_POSTINCREMENT = 4,
        RC_REGISTER_PREDECREMENT = 5,
        RC_2REGISTERS_PREDECREMENT = 6,
    };

    enum Instructions
    {
        I_BRK,
        I_MOV,
        I_CLR,
        I_ADD,
        I_SUB,
        I_ADC,
        I_SBC,
        I_INC,
        I_DEC,
        I_MUL,
        I_DIV,
        I_AND,
        I_OR,
        I_XOR,
        I_SHL,
        I_SHR,
        I_ROL,
        I_ROR,
        I_CMP,
        I_SEC,
        I_CLC,
        I_SEI,
        I_CLI,
        I_PUSH,
        I_POP,
        I_PUSHA,
        I_POPA,
        I_JMP,
        I_JSR,
        I_RTS,
        I_RTI,
        I_NOP,
        I_BRA,
        I_BEQ,
        I_BNE,
        I_BCC,
        I_BCS,
        I_BPL,
        I_BMI,
        I_BVC,
        I_BVS,
        I_BLT,
        I_BGT,
        I_BLE,
        I_BGE,
        I_SEV,
        I_CLV,
        I_SLP,
        I_SXB,
        I_SXW,
        I_SYS,
    };

    enum OpcodeSize
    {
        OS_NONE = 0,
        OS_32BIT = 0,
        OS_16BIT = 1,
        OS_8BIT = 2,
        OS_DIVSIGN = 3
    };

    struct OpcodeAttribute
    {
        uint8_t instructionCode;
        std::vector<AddressingModes> addressingModesAllowed;
        bool isSizeSpecifierAllowed;
        bool isPostfixEnabled;
    };

    struct InstructionWord
    {
        uint16_t addressingMode : 5;
        uint16_t registerConfiguration : 3;
        uint16_t instructionCode : 6;
        uint16_t opcodeSize : 2;
    };

    struct StringPair
    {
        StringPair(){};
        StringPair(string left, string right) : left(left), right(right){};

        string left;
        string right;
    };

    enum RegisterType
    {
        REG_R0,
        REG_R1,
        REG_R2,
        REG_R3,
        REG_R4,
        REG_R5,
        REG_R6,
        REG_R7,
        REG_R8,
        REG_R9,
        REG_R10,
        REG_R11,
        REG_R12,
        REG_R13,
        REG_SP,
        REG_PC
    };

    struct LabelLocation
    {
        uint32_t address;
        OpcodeSize opcodeSize;
        uint32_t lineNumber;
        string lineContent;
        bool isRelative;
    };

    enum PostfixType
    {
        PF_NONE,
        PF_INC,
        PF_DEC
    };

    // variables
    std::vector<Segment> segments;             // the machine code & data get compiled into this
    std::map<string, OpcodeAttribute> opcodes; // contains info about each instruction, indexed by their names
    std::map<string, uint32_t> labels;         // symbol table containing all labels and their addresses
    std::map<string, std::vector<LabelLocation>> unresolvedLabels;
    uint32_t PC = 0;                // keeps track of the current compiling position
    unsigned int actLineNumber = 1; // keeps track of the current line in the source code
    string actLine;                 // the content of the current source code line being assembled

    // AsmA65k.cpp
    void processLabelDefinition(const string line); // catalogs a new label
    void initializeOpcodetable();                   // populate the 'opcodes' map

    // AsmA65k-Assembly.cpp
    void processAsmLine(string line);                                                             // prepares and assembles the line. see also assembleInstruction()
    void assembleInstruction(const string mnemonic, const string modifier, const string operand); // does the actual assembly -> machine code translation

    OperandTypes detectOperandType(const string operandStr); // given the operand string, detects its type. see enum OperandType
    AddressingModes getAddressingModeFromOperand(const OperandTypes operandType);

    void handleOperand_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_Constant(const uint32_t constant, InstructionWord instructionWord);
    void handleOperand_IndirectRegister(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstant(const uint32_t constant, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusLabel(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusConstant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectLabelPlusRegister(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstantPlusRegister(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_Label(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectRegister(string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectConstantPlusRegister(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectLabelPlusRegister(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectRegisterPlusLabel(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectRegisterPlusConstant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegister_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusLabel_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusConstant_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectLabelPlusRegister_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstantPlusRegister_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectLabel_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstant_Register(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectLabel(const string operand, InstructionWord instructionWord);
    void handleOperand_Register_IndirectConstant(const string operand, InstructionWord instructionWord);
    //    void handleDoubleRegisters(const StringPair sp, InstructionWord instructionWord, const char postfixChar);
    void handleDoubleRegisters(const StringPair sp, InstructionWord instructionWord, const PostfixType postFix);
    void handleOperand_Constant_Label(const string operand, InstructionWord instructionWord);
    void handleOperand_Constant_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_Label_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_Label_Label(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegister_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectLabel_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstant_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusLabel_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectRegisterPlusConstant_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectLabelPlusRegister_Constant(const string operand, InstructionWord instructionWord);
    void handleOperand_IndirectConstantPlusRegister_Constant(const string operand, InstructionWord instructionWord);

    // AsmA65k-Directives.cpp
    bool processDirectives(const string line);                                      // the main method for processing & handling the directives
    int detectDirective(const string line);                                         // detects if there's a directive on the given line
    void handleDirective_Text(const string line, const int directiveType);          // handles .text "asdf" directives
    void handleDirective_ByteWordDword(const string line, const int directiveType); // handles data entry directives
    void handleDirective_SetPC(const string line);                                  // handles .pc = xxx directives
    void handleDirective_Define(const string line);                                 // handles the .define directive

    // AsmA65k-Misc.cpp
    bool isCommentLine(const string line);              // check if a line is made of entirely out of a comment
    int convertStringToInteger(const string valueStr);  // as the name implies, converts a std::string into an int
    int findChar(const string text, char c);            // searches for the given character and returns its index or -1
    void throwException_ValueOutOfRange();              // throws an exception
    void throwException_InvalidNumberFormat();          // throws an exception
    void checkIntegerRange(const int64_t result);       // checks if the 64 bit value can be fit into 32 bits (that's the max. allowed)
    void throwException_SyntaxError(const string line); // throws an exception
    void throwException_InvalidRegister();              // throws an exception
    void throwException_InvalidOperands();              // throws an exception
    void throwException_InvalidMnemonic();
    void throwException_InternalError(); // throws an exception
    void throwException_SymbolOutOfRange();
    uint32_t resolveLabel(const string label, const uint32_t address, const OpcodeSize size = OS_32BIT, bool isRelative = false); // returns the address associated with a label
    string removeSquaredBrackets(const string operand);                                                  // removes the enclosing squared bracked from a string
    StringPair splitStringByPlusSign(const string operand);                                              // splits a string into a StringPair separated by a '+' character
    StringPair splitStringByComma(const string operand);                                                 // splits a string into a StringPair separated by a ',' character
    RegisterType detectRegisterType(const string registerStr);                                           // converts the string into a RegisterType
    bool findAddressingMode(const string mnemonic, const AddressingModes am);
    void checkIfAddressingModeIsLegalForThisInstruction(const string mnemonic, const OperandTypes operandType);
    void checkIfSizeSpecifierIsAllowed(const string mnemonic, const OpcodeSize opcodeSize);
    OpcodeSize getOpcodeSizeFromSignedInteger(const int32_t value);
    OpcodeSize getOpcodeSizeFromUnsigedInteger(const uint64_t value);
    void verifyRangeForConstant(const string constant, const OpcodeSize opcodeSize);
    void verifyRangeForConstant(const uint32_t constant, OpcodeSize opcodeSize);
    void addData(const OpcodeSize size, const uint32_t data);
    void addData(const string sizeSpecifier, const uint32_t data);
    uint8_t getOpcodeSize(const string modifierCharacter); // takes the modifier character (eg.: mov.b -> 'b') and returns its numerical value
    void addInstructionWord(const InstructionWord instructionWord);
    void addRegisterConfigurationByte(const string registerString, InstructionWord instructionWord, const PostfixType postfixType);
    string detectAndRemoveLabelDefinition(string line);
    PostfixType getPostFixType(const string operand);

public:
    void asmlog(const char *fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        printf(("[%s: %d] %s(): "), __FILE__, __LINE__, __FUNCTION__);
        vprintf(fmt, args);
        va_end(args);
    }
};
