//
//  AsmA65k-Assembly.cpp
//  AsmA65k - The assembler for the A65000 microprocessor
//
//  Created by Zoltán Majoros on 2013.05.16
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//
//  C++11
//

#include <Asm65k.h>
#include <sstream>
#include <regex>
#include <iostream>

using namespace std;

void AsmA65k::ProcessAsmLine(const string& line)
{
    const string processedLine = DetectAndRemoveLabelDefinition(line);
    if (processedLine.size() == 0)
        return;

    // ==== extract mnemonic and operands ====
    // $1 = instruction, $2 = .b/w, $3 = operands
    const static regex rx_matchInstructionAndOperands(R"(\s*([a-z]{2,5})\.?([bw]?)\s*(.*))", regex_constants::icase);
    smatch asmMatches;
    if (regex_match(processedLine, asmMatches, rx_matchInstructionAndOperands) == false)
        ThrowException_SyntaxError(processedLine);

    // assign matches to individual variables
    string mnemonic = asmMatches[1].str();
    string modifier = asmMatches[2].str();
    string operand = asmMatches[3].str();

    // remove comment from operand
    const static regex rx_detectComment(R"((.*\S)\s*;.*)", regex_constants::icase);
    smatch cleanOperand;
    if (regex_match(operand, cleanOperand, rx_detectComment))
        operand = cleanOperand[1].str();

    // convert strings to lower case
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);
    std::transform(modifier.begin(), modifier.end(), modifier.begin(), ::tolower);
    std::transform(operand.begin(), operand.end(), operand.begin(), ::tolower);

    AssembleInstruction(mnemonic, modifier, operand);
}

void AsmA65k::AssembleInstruction(const string& mnemonic, const string& modifier, const string& operand)
{
    InstructionWord instructionWord;

    instructionWord.opcodeSize = GetOpcodeSize(modifier);

    if (opcodes.find(mnemonic) == opcodes.end())
    {
        ThrowException_InvalidMnemonic();
    }

    instructionWord.instructionCode = opcodes[mnemonic].instructionCode;

    CheckIfSizeSpecifierIsAllowed(mnemonic, (OpcodeSize)instructionWord.opcodeSize);

    OperandTypes operandType = DetectOperandType(operand);
    CheckIfAddressingModeIsLegalForThisInstruction(mnemonic, operandType);

    uint32_t effectiveAddress = 0;

    switch (operandType)
    {
    case OT_NONE: // SEI
        instructionWord.addressingMode = AM_IMPLIED;
        instructionWord.registerConfiguration = RC_NOREGISTER;
        AddInstructionWord(instructionWord);
        break;
    case OT_REGISTER: // INC r0
        HandleOperand_Register(operand, instructionWord);
        break;
    case OT_LABEL: // BEQ label
    {
        const uint8_t instruction = instructionWord.instructionCode;
        effectiveAddress = ResolveLabel(operand, PC + 2, (OpcodeSize)instructionWord.opcodeSize, instruction >= I_BRA && instruction <= I_BGE);
        if (instruction >= I_BRA && instruction <= I_BGE)
            instructionWord.opcodeSize = OS_16BIT;
    }
        HandleOperand_Constant(effectiveAddress, instructionWord);
        break;
    case OT_CONSTANT:
        effectiveAddress = ConvertStringToInteger(operand); // BNE $4000 or PSH $f000
        HandleOperand_Constant(effectiveAddress, instructionWord);
        break;
    case OT_INDIRECT_REGISTER: // INC [r0]
        HandleOperand_IndirectRegister(operand, instructionWord);
        break;
    case OT_INDIRECT_LABEL: // INC [label]
        effectiveAddress = ResolveLabel(RemoveSquaredBrackets(operand), PC + 2);
        HandleOperand_IndirectConstant(effectiveAddress, instructionWord);
        break;
    case OT_INDIRECT_CONSTANT: // INC.w [$ffff]
        effectiveAddress = ConvertStringToInteger(RemoveSquaredBrackets(operand));
        HandleOperand_IndirectConstant(effectiveAddress, instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_LABEL: // INC.b [r0 + label]
        HandleOperand_IndirectRegisterPlusLabel(RemoveSquaredBrackets(operand), instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_CONSTANT: // INC [r0 + 10]
        HandleOperand_IndirectRegisterPlusConstant(RemoveSquaredBrackets(operand), instructionWord);
        break;
    case OT_INDIRECT_LABEL_PLUS_REGISTER: // INC [label + r0]
        HandleOperand_IndirectLabelPlusRegister(RemoveSquaredBrackets(operand), instructionWord);
        break;
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER: // INC [$1000 + r0]
        HandleOperand_IndirectConstantPlusRegister(RemoveSquaredBrackets(operand), instructionWord);
        break;
    case OT_REGISTER__LABEL: // MOV r0, label
        HandleOperand_Register_Label(operand, instructionWord);
        break;
    case OT_REGISTER__CONSTANT: // MOV r0, 1234
        HandleOperand_Register_Constant(operand, instructionWord);
        break;
    case OT_REGISTER__REGISTER: // MOV r0, r1
        HandleOperand_Register_Register(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_REGISTER: // MOV r0, [r1]
        HandleOperand_Register_IndirectRegister(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER: // MOV r0, [$f000 + r1]
        HandleOperand_Register_IndirectConstantPlusRegister(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER: // MOV r0, [label + r1]
        HandleOperand_Register_IndirectLabelPlusRegister(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL: // MOV r0, [r1 + label]
        HandleOperand_Register_IndirectRegisterPlusLabel(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT: // MOV r0, [r1 + 10]
        HandleOperand_Register_IndirectRegisterPlusConstant(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER__REGISTER: // MOV [r0], r1
        HandleOperand_IndirectRegister_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER: // MOV [r0 + label], r1
        HandleOperand_IndirectRegisterPlusLabel_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER: // MOV [r0 + 10], r1
        HandleOperand_IndirectRegisterPlusConstant_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER: // MOV [label + r0], r1
        HandleOperand_IndirectLabelPlusRegister_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER: // MOV [1234 + r0], r1
        HandleOperand_IndirectConstantPlusRegister_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_LABEL__REGISTER: // MOV [kacsa], r0
        HandleOperand_IndirectLabel_Register(operand, instructionWord);
        break;
    case OT_INDIRECT_CONSTANT__REGISTER: // MOV [$6660], r0
        HandleOperand_IndirectConstant_Register(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_LABEL: // MOV r0, [kacsa]
        HandleOperand_Register_IndirectLabel(operand, instructionWord);
        break;
    case OT_REGISTER__INDIRECT_CONSTANT: // MOV r0, [$4434]
        HandleOperand_Register_IndirectConstant(operand, instructionWord);
        break;
    case OT_CONSTANT__LABEL:
        HandleOperand_Constant_Label(operand, instructionWord);
        break;
    case OT_CONSTANT__CONSTANT: // MOV $1234, $5678
        HandleOperand_Constant_Constant(operand, instructionWord);
        break;
    case OT_LABEL__CONSTANT:
        HandleOperand_Label_Constant(operand, instructionWord);
        break;
    case OT_LABEL__LABEL:
        HandleOperand_Label_Label(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER__CONSTANT: // [r0], 64
        HandleOperand_IndirectRegister_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_LABEL__CONSTANT: // [names], 64
        HandleOperand_IndirectLabel_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_CONSTANT__CONSTANT: // [$1234], 64
        HandleOperand_IndirectConstant_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_LABEL__CONSTANT: // [r0 + names], 64
        HandleOperand_IndirectRegisterPlusLabel_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_REGISTER_PLUS_CONSTANT__CONSTANT: // [r0 + 1234], 64
        HandleOperand_IndirectRegisterPlusConstant_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_LABEL_PLUS_REGISTER__CONSTANT: // [names + r0], 64
        HandleOperand_IndirectLabelPlusRegister_Constant(operand, instructionWord);
        break;
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER__CONSTANT: // [2344 + r0], 64
        HandleOperand_IndirectConstantPlusRegister_Constant(operand, instructionWord);
        break;
    }
}

AsmA65k::AddressingModes AsmA65k::GetAddressingModeFromOperand(const OperandTypes operandType)
{
    switch (operandType)
    {
    case OT_NONE: // SEI
        return AM_IMPLIED;

    case OT_REGISTER: // INC r0
        return AM_REGISTER1;

    case OT_LABEL:          // INC label
    case OT_CONSTANT:       // BNE 40 or INC $f000 or PSH.w 0
        return AM_AMBIGOUS; // AM_RELATIVE or AM_DIRECT or AM_CONST_IMMEDIATE

    case OT_INDIRECT_REGISTER: // INC [r0]
        return AM_REGISTER1;

    case OT_INDIRECT_LABEL:    // INC [label]
    case OT_INDIRECT_CONSTANT: // INC.w [$ffff]
        return AM_REGISTER_INDIRECT1;

    case OT_INDIRECT_REGISTER_PLUS_LABEL:    // INC.b [r0 + label]
    case OT_INDIRECT_REGISTER_PLUS_CONSTANT: // INC [r0 + 10]
    case OT_INDIRECT_LABEL_PLUS_REGISTER:    // INC [label + r0]
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER: // INC [$1000 + r0]
        return AM_INDEXED1;

    case OT_REGISTER__LABEL:    // MOV r0, label
    case OT_REGISTER__CONSTANT: // MOV r0, 1234
        return AM_REG_IMMEDIATE;

    case OT_REGISTER__REGISTER: // MOV r0, r1
        return AM_REGISTER2;

    case OT_REGISTER__INDIRECT_REGISTER: // MOV r0, [r1]
        return AM_REGISTER_INDIRECT_SRC;

    case OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER: // MOV r0, [$f000 + r1]
    case OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER:    // MOV r0, [label + r1]
    case OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL:    // MOV r0, [r1 + label]
    case OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT: // MOV r0, [r1 + 10]
        return AM_INDEXED_SRC;

    case OT_INDIRECT_REGISTER__CONSTANT: // MOV [r0], 64
        return AM_REGISTER_INDIRECT_CONST;

    case OT_INDIRECT_REGISTER__REGISTER: // MOV [r0], r1
        return AM_REGISTER_INDIRECT_DEST;

    case OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER:    // MOV [r0 + label], r1
    case OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER: // MOV [r0 + 10], r1
    case OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER:    // MOV [label + r0], r1
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER: // MOV [1234 + r0], r1
        return AM_INDEXED_DEST;

    case OT_INDIRECT_LABEL__REGISTER:    // MOV [kacsa], r0
    case OT_INDIRECT_CONSTANT__REGISTER: // MOV [$6660], r0
        return AM_ABSOLUTE_DEST;

    case OT_REGISTER__INDIRECT_LABEL:    // MOV r0, [kacsa]
    case OT_REGISTER__INDIRECT_CONSTANT: // MOV r0, [$4434]
        return AM_ABSOLUTE_SRC;

    case OT_CONSTANT__LABEL:    // SYS $1234, symbol
    case OT_CONSTANT__CONSTANT: // SYS $1234, $5678
    case OT_LABEL__CONSTANT:    // SYS symbol, $5678
    case OT_LABEL__LABEL:       // SYS symbol, symbol
        return AM_SYSCALL;

    case OT_INDIRECT_CONSTANT__CONSTANT: // [1234], 64
    case OT_INDIRECT_LABEL__CONSTANT:    // [names], 64
        return AM_ABSOLUTE_CONST;

    case OT_INDIRECT_REGISTER_PLUS_CONSTANT__CONSTANT: // [r0 + 1234], 64
    case OT_INDIRECT_REGISTER_PLUS_LABEL__CONSTANT:    // [r0 + names], 64
    case OT_INDIRECT_CONSTANT_PLUS_REGISTER__CONSTANT: // [$2344 + r0], 64
    case OT_INDIRECT_LABEL_PLUS_REGISTER__CONSTANT:    // [names + r0], 64
        return AM_INDEXED_CONST;

    default:
        ThrowException_InvalidOperands();
    }

    ThrowException_InternalError();

    return AM_IMPLIED; // will never get here
}

void AsmA65k::HandleOperand_Register_IndirectConstant(const string& operand, InstructionWord instructionWord) // MOV r0, [$4434]
{
    StringPair sp = SplitStringByComma(operand);
    sp.right = RemoveSquaredBrackets(sp.right);

    instructionWord.addressingMode = AM_ABSOLUTE_SRC;
    AddRegisterConfigurationByte(sp.left, instructionWord, PF_NONE);
    AddData(OS_32BIT, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_Register_IndirectLabel(const string& operand, InstructionWord instructionWord) // MOV r0, [kacsa]
{
    StringPair sp = SplitStringByComma(operand);
    sp.right = RemoveSquaredBrackets(sp.right);

    instructionWord.addressingMode = AM_ABSOLUTE_SRC;
    AddRegisterConfigurationByte(sp.left, instructionWord, PF_NONE);
    AddData(OS_32BIT, ResolveLabel(sp.right, PC));
}

void AsmA65k::HandleOperand_IndirectConstant_Register(const string& operand, InstructionWord instructionWord) // MOV [$6660], r0
{
    StringPair sp = SplitStringByComma(operand);
    sp.left = RemoveSquaredBrackets(sp.left);

    instructionWord.addressingMode = AM_ABSOLUTE_DEST;
    AddRegisterConfigurationByte(sp.right, instructionWord, PF_NONE);
    AddData(OS_32BIT, ConvertStringToInteger(sp.left));
}

void AsmA65k::HandleOperand_IndirectLabel_Register(const string& operand, InstructionWord instructionWord) // MOV [kacsa], r0
{
    StringPair sp = SplitStringByComma(operand);
    sp.left = RemoveSquaredBrackets(sp.left);

    instructionWord.addressingMode = AM_ABSOLUTE_DEST;
    AddRegisterConfigurationByte(sp.right, instructionWord, PF_NONE);
    AddData(OS_32BIT, ResolveLabel(sp.left, PC));
}

void AsmA65k::HandleOperand_IndirectConstantPlusRegister_Register(const string& operand, InstructionWord instructionWord) // MOV [1234 + r0]+, r1
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    sp.left = RemoveSquaredBrackets(sp.left);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.left);

    instructionWord.addressingMode = AM_INDEXED_DEST; // this must precede the HandleDoubleRegisters() method!
    HandleDoubleRegisters(StringPair(indexedOperandPair.right, sp.right), instructionWord, postFix);
    AddData(OS_32BIT, ConvertStringToInteger(indexedOperandPair.left));
}

void AsmA65k::HandleOperand_IndirectLabelPlusRegister_Register(const string& operand, InstructionWord instructionWord) // MOV [label + r0], r1
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    sp.left = RemoveSquaredBrackets(sp.left);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.left);

    instructionWord.addressingMode = AM_INDEXED_DEST;
    HandleDoubleRegisters(StringPair(indexedOperandPair.right, sp.right), instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indexedOperandPair.left, PC));
}

void AsmA65k::HandleOperand_IndirectRegisterPlusConstant_Register(const string& operand, InstructionWord instructionWord) // MOV [r0 + 10], r1
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    sp.left = RemoveSquaredBrackets(sp.left);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.left);

    instructionWord.addressingMode = AM_INDEXED_DEST;
    HandleDoubleRegisters(StringPair(indexedOperandPair.left, sp.right), instructionWord, postFix);
    AddData(OS_32BIT, ConvertStringToInteger(indexedOperandPair.right));
}

void AsmA65k::HandleOperand_IndirectRegisterPlusLabel_Register(const string& operand, InstructionWord instructionWord) // MOV [r0 + label], r1
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    sp.left = RemoveSquaredBrackets(sp.left);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.left);

    instructionWord.addressingMode = AM_INDEXED_DEST;
    HandleDoubleRegisters(StringPair(indexedOperandPair.left, sp.right), instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indexedOperandPair.right, PC));
}

void AsmA65k::HandleOperand_IndirectRegister_Register(const string& operand, InstructionWord instructionWord) // MOV [r0], r1
{
    StringPair sp = SplitStringByComma(operand);
    instructionWord.addressingMode = AM_REGISTER_INDIRECT_DEST;
    HandleDoubleRegisters(StringPair(RemoveSquaredBrackets(sp.left), sp.right), instructionWord, GetPostFixType(sp.left));
}

void AsmA65k::HandleOperand_Register_IndirectRegisterPlusConstant(const string& operand, InstructionWord instructionWord) // MOV r0, [r1 + 10]
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.right);
    sp.right = RemoveSquaredBrackets(sp.right);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.right);

    instructionWord.addressingMode = AM_INDEXED_SRC;
    HandleDoubleRegisters(StringPair(sp.left, indexedOperandPair.left), instructionWord, postFix); // T1, T2, T3
    AddData(OS_32BIT, ConvertStringToInteger(indexedOperandPair.right));                           // T4
}

void AsmA65k::HandleOperand_Register_IndirectConstantPlusRegister(const string& operand, InstructionWord instructionWord) // MOV r0, [$f000 + r1]
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.right);
    sp.right = RemoveSquaredBrackets(sp.right);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.right);

    instructionWord.addressingMode = AM_INDEXED_SRC;
    HandleDoubleRegisters(StringPair(sp.left, indexedOperandPair.right), instructionWord, postFix); // T1, T2, T3
    AddData(OS_32BIT, ConvertStringToInteger(indexedOperandPair.left));                             // T4
}

void AsmA65k::HandleOperand_Register_IndirectRegisterPlusLabel(const string& operand, InstructionWord instructionWord) // MOV r0, [r1 + label]
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.right);
    sp.right = RemoveSquaredBrackets(sp.right);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.right);

    instructionWord.addressingMode = AM_INDEXED_SRC;
    HandleDoubleRegisters(StringPair(sp.left, indexedOperandPair.left), instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indexedOperandPair.right, PC));
}

void AsmA65k::HandleOperand_Register_IndirectLabelPlusRegister(const string& operand, InstructionWord instructionWord) // MOV r0, [csoki + r1]
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.right);
    sp.right = RemoveSquaredBrackets(sp.right);
    StringPair indexedOperandPair = SplitStringByPlusSign(sp.right);

    instructionWord.addressingMode = AM_INDEXED_SRC;
    HandleDoubleRegisters(StringPair(sp.left, indexedOperandPair.right), instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indexedOperandPair.left, PC));
}

void AsmA65k::HandleOperand_Register_IndirectRegister(const string& operand, InstructionWord instructionWord) // mov r0, [r1]
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.right);
    sp.right = RemoveSquaredBrackets(sp.right);

    instructionWord.addressingMode = AM_REGISTER_INDIRECT_SRC;
    HandleDoubleRegisters(sp, instructionWord, postFix);
}

void AsmA65k::HandleOperand_Register_Register(const string& operand, InstructionWord instructionWord) // MOV.b r0, r1
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_REGISTER2;
    HandleDoubleRegisters(sp, instructionWord, PF_NONE);
}

void AsmA65k::HandleOperand_Register_Constant(const string& operand, InstructionWord instructionWord) // MOV.b r0, 1234
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_REG_IMMEDIATE;
    AddRegisterConfigurationByte(sp.left, instructionWord, PF_NONE);
    VerifyRangeForConstant(sp.right, (OpcodeSize)instructionWord.opcodeSize);
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_Register_Label(const string& operand, InstructionWord instructionWord) // MOV.b r0, label
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_REG_IMMEDIATE;
    AddRegisterConfigurationByte(sp.left, instructionWord, PF_NONE);

    uint32_t address = ResolveLabel(sp.right, PC, (OpcodeSize)instructionWord.opcodeSize);
    VerifyRangeForConstant(std::to_string(address), (OpcodeSize)instructionWord.opcodeSize);
    AddData((OpcodeSize)instructionWord.opcodeSize, address);
}

void AsmA65k::HandleOperand_IndirectConstantPlusRegister(const string& operand, InstructionWord instructionWord) // INC.b [1233 + r0]+
{
    PostfixType postFix = GetPostFixType(operand);
    StringPair sp = SplitStringByPlusSign(RemoveSquaredBrackets(operand));

    // fill in rest of the instruction word
    instructionWord.addressingMode = AM_INDEXED1;
    AddRegisterConfigurationByte(sp.right, instructionWord, postFix);
    // add constant after i.w.
    AddData(OS_32BIT, ConvertStringToInteger(sp.left));
}

void AsmA65k::HandleOperand_IndirectLabelPlusRegister(const string& operand, InstructionWord instructionWord) // INC.b [label + r0]
{
    PostfixType postFix = GetPostFixType(operand);
    StringPair sp = SplitStringByPlusSign(RemoveSquaredBrackets(operand));

    instructionWord.addressingMode = AM_INDEXED1;
    AddRegisterConfigurationByte(sp.right, instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(sp.left, PC)); // 4 bytes
}

void AsmA65k::HandleOperand_IndirectRegisterPlusConstant(const string& operand, InstructionWord instructionWord) // INC.w [r0 + 1234]
{
    PostfixType postFix = GetPostFixType(operand);
    StringPair sp = SplitStringByPlusSign(RemoveSquaredBrackets(operand));

    instructionWord.addressingMode = AM_INDEXED1;
    AddRegisterConfigurationByte(sp.left, instructionWord, postFix);
    AddData(OS_32BIT, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectRegisterPlusLabel(const string& operand, InstructionWord instructionWord) // INC.w [r0 + label]
{
    PostfixType postFix = GetPostFixType(operand);
    StringPair sp = SplitStringByPlusSign(RemoveSquaredBrackets(operand));

    instructionWord.addressingMode = AM_INDEXED1;
    AddRegisterConfigurationByte(sp.left, instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(sp.right, PC)); // 4 bytes
}

void AsmA65k::HandleOperand_IndirectConstant(const uint32_t constant, InstructionWord instructionWord) // INC.w [$ffff]
{
    instructionWord.addressingMode = AM_ABSOLUTE1;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_32BIT, constant);
}

void AsmA65k::HandleOperand_Register(const string& operand, InstructionWord instructionWord) // inc r0
{
    instructionWord.addressingMode = AM_REGISTER1;
    instructionWord.registerConfiguration = RC_REGISTER;
    RegisterType registerIndex = DetectRegisterType(operand);

    AddInstructionWord(instructionWord);
    AddData(OS_8BIT, registerIndex);
}

void AsmA65k::HandleOperand_Constant(const uint32_t effectiveAddress, InstructionWord instructionWord) // bne $4000 or psh $f000
{
    const uint8_t instruction = instructionWord.instructionCode;
    // if branching instruction, eg.: BNE 363
    if (instruction >= I_BRA && instruction <= I_BGE)
    {
        instructionWord.addressingMode = AM_RELATIVE;
        instructionWord.registerConfiguration = RC_NOREGISTER;

        int32_t diff = effectiveAddress - PC - 4;
        if (diff < -32768 || diff > 32767)
            ThrowException_SymbolOutOfRange();

        instructionWord.opcodeSize = OS_16BIT;
        AddInstructionWord(instructionWord);
        AddData(OS_16BIT, diff);
    }
    else
        switch (instruction)
        {
        case I_PUSH: // push $ff
            VerifyRangeForConstant(effectiveAddress, (OpcodeSize)instructionWord.opcodeSize);
            instructionWord.addressingMode = AM_CONST_IMMEDIATE;
            instructionWord.registerConfiguration = RC_NOREGISTER;
            AddInstructionWord(instructionWord);
            AddData((OpcodeSize)instructionWord.opcodeSize, effectiveAddress);
            break;
        case I_JMP: // jmp $43434
        case I_JSR: // jsr $3434
            // these always use 32 bit addresses/constants
            instructionWord.addressingMode = AM_DIRECT;
            instructionWord.registerConfiguration = RC_NOREGISTER;
            AddInstructionWord(instructionWord);
            AddData(OS_32BIT, effectiveAddress);
            break;
        }
}

void AsmA65k::HandleOperand_IndirectRegister(const string& operand, InstructionWord instructionWord) // inc [r0]
{
    PostfixType postFix = GetPostFixType(operand);
    string registerString = RemoveSquaredBrackets(operand);

    instructionWord.addressingMode = AM_REGISTER_INDIRECT1;
    AddRegisterConfigurationByte(registerString, instructionWord, postFix);
}

void AsmA65k::HandleOperand_IndirectRegister_Constant(const string& operand, InstructionWord instructionWord) // [r0], 64
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);

    instructionWord.addressingMode = AM_REGISTER_INDIRECT_CONST;
    AddRegisterConfigurationByte(RemoveSquaredBrackets(sp.left), instructionWord, postFix);
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectLabel_Constant(const string& operand, InstructionWord instructionWord) // [names], 64
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_ABSOLUTE_CONST;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_32BIT, ResolveLabel(RemoveSquaredBrackets(sp.left), PC));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectConstant_Constant(const string& operand, InstructionWord instructionWord) // [$1234], 64
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_ABSOLUTE_CONST;
    instructionWord.registerConfiguration = RC_REGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_32BIT, ConvertStringToInteger(RemoveSquaredBrackets(sp.left)));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectRegisterPlusLabel_Constant(const string& operand, InstructionWord instructionWord) // [r0 + names], 64
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    StringPair indirectSp = SplitStringByPlusSign(RemoveSquaredBrackets(sp.left));

    instructionWord.addressingMode = AM_INDEXED_CONST;
    AddRegisterConfigurationByte(indirectSp.left, instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indirectSp.right, PC));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectRegisterPlusConstant_Constant(const string& operand, InstructionWord instructionWord) // [r0 + 1234], 64
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    StringPair indirectSp = SplitStringByPlusSign(RemoveSquaredBrackets(sp.left));

    instructionWord.addressingMode = AM_INDEXED_CONST;
    AddRegisterConfigurationByte(indirectSp.left, instructionWord, postFix);
    AddData(OS_32BIT, ConvertStringToInteger(indirectSp.right));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectConstantPlusRegister_Constant(const string& operand, InstructionWord instructionWord) // [1234 + r0], 64
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    StringPair indirectSp = SplitStringByPlusSign(RemoveSquaredBrackets(sp.left));

    instructionWord.addressingMode = AM_INDEXED_CONST;
    AddRegisterConfigurationByte(indirectSp.left, instructionWord, postFix);
    AddData(OS_32BIT, ConvertStringToInteger(indirectSp.right));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

void AsmA65k::HandleOperand_IndirectLabelPlusRegister_Constant(const string& operand, InstructionWord instructionWord) // [names + r0], 64
{
    StringPair sp = SplitStringByComma(operand);
    PostfixType postFix = GetPostFixType(sp.left);
    StringPair indirectSp = SplitStringByPlusSign(RemoveSquaredBrackets(sp.left));

    instructionWord.addressingMode = AM_INDEXED_DEST;
    AddRegisterConfigurationByte(indirectSp.right, instructionWord, postFix);
    AddData(OS_32BIT, ResolveLabel(indirectSp.left, PC));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

// used only for the syscall instruction
void AsmA65k::HandleOperand_Constant_Label(const string& operand, InstructionWord instructionWord) // sys $1234, label
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_IMPLIED;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_16BIT, ConvertStringToInteger(sp.left));
    AddData(OS_32BIT, ResolveLabel(sp.right, PC));
}

// used only for the syscall instruction
void AsmA65k::HandleOperand_Constant_Constant(const string& operand, InstructionWord instructionWord) // sys $1234, $5678
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_IMPLIED;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_16BIT, ConvertStringToInteger(sp.left));
    AddData(OS_32BIT, ConvertStringToInteger(sp.right));
}

// used only for the syscall instruction
void AsmA65k::HandleOperand_Label_Label(const string& operand, InstructionWord instructionWord) // sys label, label
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_IMPLIED;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_16BIT, ResolveLabel(sp.left, PC, OS_16BIT));
    AddData(OS_32BIT, ResolveLabel(sp.right, PC));
}

// used only for the syscall instruction
void AsmA65k::HandleOperand_Label_Constant(const string& operand, InstructionWord instructionWord) // sys label, $1234
{
    StringPair sp = SplitStringByComma(operand);

    instructionWord.addressingMode = AM_SYSCALL;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    AddInstructionWord(instructionWord);
    AddData(OS_16BIT, ResolveLabel(sp.left, PC, OS_16BIT));
    AddData((OpcodeSize)instructionWord.opcodeSize, ConvertStringToInteger(sp.right));
}

AsmA65k::OperandTypes AsmA65k::DetectOperandType(const string& operandStr)
{
    const static regex_constants::syntax_option_type icase = regex_constants::icase;
    const static regex rx_matchDoubleOperands(R"((.*)\s*,\s*(.*))", icase);
    const static regex rx_constant(R"((^-?\d+$)|(^\$[0-9a-f]+)$|(^%[01]+$))", icase);                                             // 64
    const static regex rx_label(R"([a-z][a-z_0-9]*)", icase);                                                                     // names
    const static regex rx_register(R"((r[0-9]{1,2})|(PC|SP))", icase);                                                            // r0
    const static regex rx_indirectConstant(R"(\[\s*[$%]?[0-9a-f]+\s*\])", icase);                                                 // [$5000]
    const static regex rx_indirectLabel(R"(\[[a-z][a-z_0-9]*\])", icase);                                                         // [names]
    const static regex rx_indirectRegister(R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\][\+\-]?)", icase);                                 // [r0]
    const static regex rx_indirectRegisterPlusConst(R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\+\s*[$%]?[0-9]+\s*\][\+\-]?)", icase);     // [r0 + $1000]
    const static regex rx_indirectConstPlusRegister(R"(\[\s*[$%]?[0-9a-f]+\s*\+\s*((r[0-9]{1,2})|(pc))\s*\][\+\-]?)", icase);     // [$1000 + r0]
    const static regex rx_indirectRegisterPlusLabel(R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\+\s*[a-z][a-z_0-9]*\s*\][\+\-]?)", icase); // [r0 + names]
    const static regex rx_indirectLabelPlusRegister(R"(\[\s*[a-z][a-z_0-9]*\s*\+\s*((r[0-9]{1,2})|(PC|SP))\s*\][\+\-]?)", icase); // [names + r0]

    if (operandStr == "")
        return OT_NONE;

    smatch match;
    if (regex_match(operandStr, match, rx_matchDoubleOperands)) // double operands
    {
        string left = match[1];
        string right = match[2];

        // log("double operands detected. left = '%s', right = '%s'\n", left.c_str(), right.c_str());

        if (regex_match(left, rx_register) && regex_match(right, rx_register))
            return OT_REGISTER__REGISTER;
        if (regex_match(left, rx_register) && regex_match(right, rx_constant))
            return OT_REGISTER__CONSTANT;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectConstant))
            return OT_REGISTER__INDIRECT_CONSTANT;
        if (regex_match(left, rx_indirectRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER__REGISTER;
        if (regex_match(left, rx_indirectLabel) && regex_match(right, rx_register))
            return OT_INDIRECT_LABEL__REGISTER;
        if (regex_match(left, rx_indirectConstant) && regex_match(right, rx_register))
            return OT_INDIRECT_CONSTANT__REGISTER;
        if (regex_match(left, rx_indirectRegisterPlusConst) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER;
        if (regex_match(left, rx_indirectRegisterPlusLabel) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER;
        if (regex_match(left, rx_indirectLabelPlusRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER;
        if (regex_match(left, rx_indirectConstPlusRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectRegister))
            return OT_REGISTER__INDIRECT_REGISTER;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectRegisterPlusConst))
            return OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectRegisterPlusLabel))
            return OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectConstPlusRegister))
            return OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectLabelPlusRegister))
            return OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER;
        if (regex_match(left, rx_register) && regex_match(right, rx_label))
            return OT_REGISTER__LABEL;
        if (regex_match(left, rx_register) && regex_match(right, rx_indirectLabel))
            return OT_REGISTER__INDIRECT_LABEL;
        if (regex_match(left, rx_constant) && regex_match(right, rx_label))
            return OT_CONSTANT__LABEL;
        if (regex_match(left, rx_constant) && regex_match(right, rx_constant))
            return OT_CONSTANT__CONSTANT;
        if (regex_match(left, rx_label) && regex_match(right, rx_constant))
            return OT_LABEL__CONSTANT;
        if (regex_match(left, rx_label) && regex_match(right, rx_label))
            return OT_LABEL__LABEL;
        if (regex_match(left, rx_indirectRegister) && regex_match(right, rx_constant))
            return OT_INDIRECT_REGISTER__CONSTANT;
        if (regex_match(left, rx_indirectLabel) && regex_match(right, rx_constant))
            return OT_INDIRECT_LABEL__CONSTANT;
        if (regex_match(left, rx_indirectConstant) && regex_match(right, rx_constant))
            return OT_INDIRECT_CONSTANT__CONSTANT;
        if (regex_match(left, rx_indirectRegisterPlusLabel) && regex_match(right, rx_constant))
            return OT_INDIRECT_REGISTER_PLUS_LABEL__CONSTANT;
        if (regex_match(left, rx_indirectRegisterPlusConst) && regex_match(right, rx_constant))
            return OT_INDIRECT_REGISTER_PLUS_CONSTANT__CONSTANT;
        if (regex_match(left, rx_indirectLabelPlusRegister) && regex_match(right, rx_constant))
            return OT_INDIRECT_LABEL_PLUS_REGISTER__CONSTANT;
        if (regex_match(left, rx_indirectConstPlusRegister) && regex_match(right, rx_constant))
            return OT_INDIRECT_CONSTANT_PLUS_REGISTER__CONSTANT;
    }
    else // single operand
    {
        // printf("DetectOperandType(): single operand detected. operand = '%s'\n", operandStr.c_str());

        if (regex_match(operandStr, rx_register))
            return OT_REGISTER;
        if (regex_match(operandStr, rx_constant))
            return OT_CONSTANT;
        if (regex_match(operandStr, rx_indirectConstant))
            return OT_INDIRECT_CONSTANT;
        if (regex_match(operandStr, rx_indirectRegister))
            return OT_INDIRECT_REGISTER;
        if (regex_match(operandStr, rx_indirectRegisterPlusConst))
            return OT_INDIRECT_REGISTER_PLUS_CONSTANT;
        if (regex_match(operandStr, rx_indirectConstPlusRegister))
            return OT_INDIRECT_CONSTANT_PLUS_REGISTER;
        if (regex_match(operandStr, rx_indirectRegisterPlusLabel))
            return OT_INDIRECT_REGISTER_PLUS_LABEL;
        if (regex_match(operandStr, rx_indirectLabelPlusRegister))
            return OT_INDIRECT_LABEL_PLUS_REGISTER;
        if (regex_match(operandStr, rx_label))
            return OT_LABEL;
        if (regex_match(operandStr, rx_indirectLabel))
            return OT_INDIRECT_LABEL;
    }

    ThrowException_InvalidOperands();

    return OT_NONE; // fake line to silence warning
}
