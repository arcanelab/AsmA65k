//
//  AsmA65k-Assembly.cpp
//  AsmA65k - The assembler for the A65000 microprocessor
//
//  Created by Zoltán Majoros on 2013.05.16
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//
//  C++11
//

#include "Asm65k.h"
#include <sstream>
#include <regex>
#include <iostream>

using namespace std;

// ============================================================================

void AsmA65k::processAsmLine(string line)
{
    line = detectAndRemoveLabelDefinition(line);
    if (line.size() == 0)
        return;
    
    // ==== extract mnemonic and operands ====    
    // $1 = instruction, $2 = .b/w, $3 = operands
    const static regex rx_matchInstructionAndOperands(R"(\s*([a-z]{3,4})\.?([bw]?)\s*(.*))", regex_constants::icase);
    smatch asmMatches;
    if(regex_match(line, asmMatches, rx_matchInstructionAndOperands) == false)
        throwException_SyntaxError(line);
    
    // assign matches to individual variables
    string mnemonic = asmMatches[1].str();
    string modifier = asmMatches[2].str();
    string operand  = asmMatches[3].str();
    
    // remove comment from operand
    const static regex rx_detectComment(R"((.*\S)\s*;.*)", regex_constants::icase);
    smatch cleanOperand;
    if(regex_match(operand, cleanOperand, rx_detectComment))
        operand = cleanOperand[1].str();
    
    // convert strings to lower case
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);
    std::transform(modifier.begin(), modifier.end(), modifier.begin(), ::tolower);
    std::transform(operand.begin(), operand.end(), operand.begin(), ::tolower);
    
    assembleInstruction(mnemonic, modifier, operand);
}

// ============================================================================

void AsmA65k::assembleInstruction(const string mnemonic, const string modifier, const string operand)
{
    InstructionWord instructionWord;
    
    instructionWord.opcodeSize = getOpcodeSize(modifier);
    instructionWord.instructionCode = opcodes[mnemonic].instructionCode;
    
    checkIfSizeSpecifierIsAllowed(mnemonic, (OpcodeSize)instructionWord.opcodeSize);

    OperandTypes operandType = detectOperandType(operand);
    checkIfAddressingModeIsLegalForThisInstruction(mnemonic, operandType);
    
    dword effectiveAddress = 0;
    
    switch (operandType)
    {
        case OT_NONE:                                        // SEI
            instructionWord.addressingMode = AM_NONE;
            instructionWord.registerConfiguration = RC_NOREGISTER;
            addInstructionWord(instructionWord);
            break;
        case OT_REGISTER:                                    // INC r0
            handleOperand_Register(operand, instructionWord);
            break;
        case OT_LABEL:                                       // BEQ label
            effectiveAddress = resolveLabel(operand, PC+2);        // NOTE: break omitted on purpose!
        case OT_CONSTANT:                                    // BNE $4000 or PSH $f000
            handleOperand_Constant(operand, instructionWord, effectiveAddress);
            break;
        case OT_INDIRECT_REGISTER:                           // INC [r0]
            handleOperand_IndirectRegister(operand, instructionWord);
            break;
        case OT_INDIRECT_LABEL:                              // INC [label]
            effectiveAddress = resolveLabel(removeSquaredBrackets(operand), PC+2);
            handleOperand_IndirectConstant(effectiveAddress, instructionWord);
            break;
        case OT_INDIRECT_CONSTANT:                           // INC.w [$ffff]
            effectiveAddress = convertStringToInteger(removeSquaredBrackets(operand));
            handleOperand_IndirectConstant(effectiveAddress, instructionWord);
            break;
        case OT_INDIRECT_REGISTER_PLUS_LABEL:                // INC.b [r0 + label]
            handleOperand_IndirectRegisterPlusLabel(removeSquaredBrackets(operand), instructionWord);
            break;
        case OT_INDIRECT_REGISTER_PLUS_CONSTANT:             // INC [r0 + 10]
            handleOperand_IndirectRegisterPlusConstant(removeSquaredBrackets(operand), instructionWord);
            break;
        case OT_INDIRECT_LABEL_PLUS_REGISTER:                // INC [label + r0]
            handleOperand_IndirectLabelPlusRegister(removeSquaredBrackets(operand), instructionWord);
            break;
        case OT_INDIRECT_CONSTANT_PLUS_REGISTER:             // INC [$1000 + r0]
            handleOperand_IndirectConstantPlusRegister(removeSquaredBrackets(operand), instructionWord);
            break;
        case OT_REGISTER__LABEL:                             // MOV r0, label
            handleOperand_Register_Label(operand, instructionWord);
            break;
        case OT_REGISTER__CONSTANT:                          // MOV r0, 1234
            handleOperand_Register_Constant(operand, instructionWord);
            break;
        case OT_REGISTER__REGISTER:                          // MOV r0, r1
            handleOperand_Register_Register(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_REGISTER:                 // MOV r0, [r1]
            handleOperand_Register_IndirectRegister(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER:   // MOV r0, [$f000 + r1]
            handleOperand_Register_IndirectConstantPlusRegister(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER:      // MOV r0, [label + r1]
            handleOperand_Register_IndirectLabelPlusRegister(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL:      // MOV r0, [r1 + label]
            handleOperand_Register_IndirectRegisterPlusLabel(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT:   // MOV r0, [r1 + 10]
            handleOperand_Register_IndirectRegisterPlusConstant(operand, instructionWord);
            break;
        case OT_INDIRECT_REGISTER__REGISTER:                 // MOV [r0], r1
            handleOperand_IndirectRegister_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER:      // MOV [r0 + label], r1
            handleOperand_IndirectRegisterPlusLabel_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER:   // MOV [r0 + 10], r1
            handleOperand_IndirectRegisterPlusConstant_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER:      // MOV [label + r0], r1
            handleOperand_IndirectLabelPlusRegister_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER:   // MOV [1234 + r0], r1
            handleOperand_IndirectConstantPlusRegister_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_LABEL__REGISTER:                   // MOV [kacsa], r0
            handleOperand_IndirectLabel_Register(operand, instructionWord);
            break;
        case OT_INDIRECT_CONSTANT__REGISTER:                // MOV [$6660], r0
            handleOperand_IndirectConstant_Register(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_LABEL:                   // MOV r0, [kacsa]
            handleOperand_Register_IndirectLabel(operand, instructionWord);
            break;
        case OT_REGISTER__INDIRECT_CONSTANT:                // MOV r0, [$4434]
            handleOperand_Register_IndirectConstant(operand, instructionWord);
            break;
    }
}

// ============================================================================

AsmA65k::AddressingModes AsmA65k::getAddressingModeFromOperand(const OperandTypes operandType)
{
    switch (operandType)
    {
        case OT_NONE:                                        // SEI
            return AM_NONE;
        
        case OT_REGISTER:                                    // INC r0
            return AM_REGISTER1;
        
        case OT_LABEL:                                       // INC label
        case OT_CONSTANT:                                    // BNE 40 or INC $f000 or PSH.w 0
            return AM_AMBIGOUS; // AM_RELATIVE or AM_DIRECT or AM_CONST_IMMEDIATE

        case OT_INDIRECT_REGISTER:                           // INC [r0]
            return AM_REGISTER1;
        
        case OT_INDIRECT_LABEL:                              // INC [label]
        case OT_INDIRECT_CONSTANT:                           // INC.w [$ffff]
            return AM_REGISTER_INDIRECT1;
        
        case OT_INDIRECT_REGISTER_PLUS_LABEL:                // INC.b [r0 + label]
        case OT_INDIRECT_REGISTER_PLUS_CONSTANT:             // INC [r0 + 10]
        case OT_INDIRECT_LABEL_PLUS_REGISTER:                // INC [label + r0]
        case OT_INDIRECT_CONSTANT_PLUS_REGISTER:             // INC [$1000 + r0]
            return AM_INDEXED1;
        
        case OT_REGISTER__LABEL:                             // MOV r0, label
        case OT_REGISTER__CONSTANT:                          // MOV r0, 1234
            return AM_REG_IMMEDIATE;
        
        case OT_REGISTER__REGISTER:                          // MOV r0, r1
            return AM_REGISTER2;

        case OT_REGISTER__INDIRECT_REGISTER:                 // MOV r0, [r1]
            return AM_REGISTER_INDIRECT_SRC;

        case OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER:   // MOV r0, [$f000 + r1]
        case OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER:      // MOV r0, [label + r1]
        case OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL:      // MOV r0, [r1 + label]
        case OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT:   // MOV r0, [r1 + 10]
            return AM_INDEXED_SRC;

        case OT_INDIRECT_REGISTER__REGISTER:                 // MOV [r0], r1
            return AM_REGISTER_INDIRECT_DEST;
        
        case OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER:      // MOV [r0 + label], r1
        case OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER:   // MOV [r0 + 10], r1
        case OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER:      // MOV [label + r0], r1
        case OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER:   // MOV [1234 + r0], r1
            return AM_INDEXED_DEST;

        case OT_INDIRECT_LABEL__REGISTER:                   // MOV [kacsa], r0
        case OT_INDIRECT_CONSTANT__REGISTER:                // MOV [$6660], r0
            return AM_ABSOLUTE_DEST;
        
        case OT_REGISTER__INDIRECT_LABEL:                   // MOV r0, [kacsa]
        case OT_REGISTER__INDIRECT_CONSTANT:                // MOV r0, [$4434]
            return AM_ABSOLUTE_SRC;
        default:
            throwException_InvalidOperands();
    }

    throwException_InternalError();
    
    return AM_NONE; // will never get here
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectConstant(const string operand, InstructionWord instructionWord) // MOV r0, [$4434]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);
    
    instructionWord.addressingMode = AM_ABSOLUTE_SRC;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    addData(OS_32BIT, convertStringToInteger(sp.right));
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectLabel(const string operand, InstructionWord instructionWord) // MOV r0, [kacsa]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);
    
    instructionWord.addressingMode = AM_ABSOLUTE_SRC;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    addData(OS_32BIT, resolveLabel(sp.right, PC));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectConstant_Register(const string operand, InstructionWord instructionWord) // MOV [$6660], r0
{
    StringPair sp = splitStringByComma(operand);
    sp.left = removeSquaredBrackets(sp.left);
    
    instructionWord.addressingMode = AM_ABSOLUTE_DEST;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.right);
    addData(OS_32BIT, convertStringToInteger(sp.left));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectLabel_Register(const string operand, InstructionWord instructionWord) // MOV [kacsa], r0
{
    StringPair sp = splitStringByComma(operand);
    sp.left = removeSquaredBrackets(sp.left);
    
    instructionWord.addressingMode = AM_ABSOLUTE_DEST;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.right);
    addData(OS_32BIT, resolveLabel(sp.left, PC));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectConstantPlusRegister_Register(const string operand, InstructionWord instructionWord) // MOV [1234 + r0], r1
{
    StringPair sp = splitStringByComma(operand);
    sp.left = removeSquaredBrackets(sp.left);
    StringPair indexedOperandPair = splitStringByPlusSign(sp.left);
    
    instructionWord.addressingMode = AM_INDEXED_DEST; // this must precede the handleDoubleRegisters() method!
    handleDoubleRegisters(StringPair(indexedOperandPair.right, sp.right), instructionWord);
    addData(OS_32BIT, convertStringToInteger(indexedOperandPair.left));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectLabelPlusRegister_Register(const string operand, InstructionWord instructionWord) // MOV [label + r0], r1
{
    static const regex rx_indirectLabelPlusRegister(R"(\[\s*([a-z][a-z_0-9]*)\s*\+\s*(r[0-9]{1,2}|PC|SP)\s*\]\s*,\s*(r[0-9]{1,2}|PC|SP))", regex_constants::icase);
    smatch match;
    
    if(regex_match(operand, match, rx_indirectLabelPlusRegister) == false)
        throwException_InvalidOperands();
    
    instructionWord.addressingMode = AM_INDEXED_DEST;
    handleDoubleRegisters(StringPair(match[2], match[3]), instructionWord);
    addData(OS_32BIT, resolveLabel(match[1], PC));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegisterPlusConstant_Register(const string operand, InstructionWord instructionWord) // MOV [r0 + 10], r1
{
    StringPair sp = splitStringByComma(operand);
    sp.left = removeSquaredBrackets(sp.left);
    StringPair indexedOperandPair = splitStringByPlusSign(sp.left);

    instructionWord.addressingMode = AM_INDEXED_DEST;
    handleDoubleRegisters(StringPair(indexedOperandPair.left, sp.right), instructionWord);
    addData(OS_32BIT, convertStringToInteger(indexedOperandPair.right));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegisterPlusLabel_Register(const string operand, InstructionWord instructionWord) // MOV [r0 + label], r1
{
    static const regex rx_indirectLabelPlusRegister(R"(\[\s*(r[0-9]{1,2}|PC|SP)\s*\+\s*([a-z][a-z_0-9]*)\s*\]\s*,\s*(r[0-9]{1,2}|PC|SP))", regex_constants::icase);
    smatch match;
    
    if(regex_match(operand, match, rx_indirectLabelPlusRegister) == false)
        throwException_InvalidOperands();
    
    instructionWord.addressingMode = AM_INDEXED_DEST;
    handleDoubleRegisters(StringPair(match[1], match[3]), instructionWord);
    addData(OS_32BIT, resolveLabel(match[2], PC));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegister_Register(const string operand, InstructionWord instructionWord) // MOV [r0], r1
{
    StringPair sp = splitStringByComma(operand);
    instructionWord.addressingMode = AM_REGISTER_INDIRECT_DEST;
    handleDoubleRegisters(StringPair(removeSquaredBrackets(sp.left), sp.right), instructionWord);
}

// ============================================================================
// TODO: this and the next methods are 99% the same, you should refactor them
void AsmA65k::handleOperand_Register_IndirectRegisterPlusConstant(const string operand, InstructionWord instructionWord) // MOV r0, [r1 + 10]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);
    StringPair indexedOperandPair = splitStringByPlusSign(sp.right);
    
    instructionWord.addressingMode = AM_INDEXED_SRC;
    handleDoubleRegisters(StringPair(sp.left, indexedOperandPair.left), instructionWord); // T1, T2, T3
    addData(OS_32BIT, convertStringToInteger(indexedOperandPair.right)); // T4
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectConstantPlusRegister(const string operand, InstructionWord instructionWord) // MOV r0, [$f000 + r1]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);
    StringPair indexedOperandPair = splitStringByPlusSign(sp.right);
    
    instructionWord.addressingMode = AM_INDEXED_SRC;
    handleDoubleRegisters(StringPair(sp.left, indexedOperandPair.right), instructionWord); // T1, T2, T3
    addData(OS_32BIT, convertStringToInteger(indexedOperandPair.left)); // T4
}

// ============================================================================
// TODO: this and the next methods are 99% the same, you should refactor them
void AsmA65k::handleOperand_Register_IndirectRegisterPlusLabel(const string operand, InstructionWord instructionWord) // MOV r0, [r1 + label]
{
    static const regex rx_indirectLabelPlusRegister(R"((r[0-9]{1,2}|PC|SP)\s*,\s*\[\s*(r[0-9]{1,2}|PC|SP)\s*\+\s*([a-z][a-z_0-9]*)\s*\])", regex_constants::icase);
    smatch match;
    
    if(regex_match(operand, match, rx_indirectLabelPlusRegister) == false)
        throwException_InvalidOperands();
    
    instructionWord.addressingMode = AM_INDEXED_SRC;
    handleDoubleRegisters(StringPair(match[1], match[2]), instructionWord);
    addData(OS_32BIT, resolveLabel(match[3], PC));
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectLabelPlusRegister(const string operand, InstructionWord instructionWord) // MOV r0, [csoki + r1]
{
    static const regex rx_indirectLabelPlusRegister(R"((r[0-9]{1,2}|PC|SP)\s*,\s*\[\s*([a-z][a-z_0-9]*)\s*\+\s*(r[0-9]{1,2}|PC|SP)\s*\])", regex_constants::icase);
    smatch match;
    
    if(regex_match(operand, match, rx_indirectLabelPlusRegister) == false)
        throwException_InvalidOperands();

    instructionWord.addressingMode = AM_INDEXED_SRC;
    handleDoubleRegisters(StringPair(match[1], match[3]), instructionWord);
    addData(OS_32BIT, resolveLabel(match[2], PC));
}

// ============================================================================

void AsmA65k::handleDoubleRegisters(const StringPair sp, InstructionWord instructionWord)
{
    RegisterType regLeft = detectRegisterType(sp.left);
    RegisterType regRight = detectRegisterType(sp.right);
    if((regLeft == REG_PC || regLeft == REG_SP) && (regRight == REG_PC || regRight == REG_SP))
    {
        AsmError error(actLineNumber, actLine, "Invalid combination of registers");
        throw error;
    }
    
    byte registerSelector = 0;
    
    instructionWord.registerConfiguration = RC_NOREGISTER; // to verify if this would be changed after the following conditions
    
    if(regLeft == REG_PC || regLeft == REG_SP) // MOV PC, r4
        instructionWord.registerConfiguration = RC_SPECIAL_GENERAL;
    
    if(regRight == REG_PC || regRight == REG_SP) // MOV r4, PC
        instructionWord.registerConfiguration = RC_GENERAL_SPECIAL;
    
    if((regLeft != REG_PC) && (regLeft != REG_SP) && (regRight != REG_PC) && (regRight != REG_SP))
        instructionWord.registerConfiguration = RC_2_GENERAL_REGISTERS;
    
    if(instructionWord.registerConfiguration == RC_NOREGISTER) // check if any of the prev. conditions were met
        throwException_InternalError();
    
    registerSelector = ((regLeft & 15) << 4) | (regRight & 15);
    
    addInstructionWord(instructionWord); // 2 bytes
    addData(OS_8BIT, registerSelector); // 1 byte
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectRegister(const string operand, InstructionWord instructionWord) // mov r0, [r1]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);

    instructionWord.addressingMode = AM_REGISTER_INDIRECT_SRC;
    handleDoubleRegisters(sp, instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_Register_Register(const string operand, InstructionWord instructionWord) // MOV.b r0, r1
{
    StringPair sp = splitStringByComma(operand);

    instructionWord.addressingMode = AM_REGISTER2;
    handleDoubleRegisters(sp, instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_Register_Constant(const string operand, InstructionWord instructionWord) // MOV.b r0, 1234
{
    StringPair sp = splitStringByComma(operand);
    
    instructionWord.addressingMode = AM_REG_IMMEDIATE;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    verifyRangeForConstant(sp.right, (OpcodeSize)instructionWord.opcodeSize);
    addData((OpcodeSize)instructionWord.opcodeSize, convertStringToInteger(sp.right));
}

// ============================================================================

void AsmA65k::handleOperand_Register_Label(const string operand, InstructionWord instructionWord) // MOV.b r0, label
{
    StringPair sp = splitStringByComma(operand);
    
    instructionWord.addressingMode = AM_REG_IMMEDIATE;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    
    dword address = resolveLabel(sp.right, PC, (OpcodeSize)instructionWord.opcodeSize);
    verifyRangeForConstant(std::to_string(address), (OpcodeSize)instructionWord.opcodeSize);
    addData((OpcodeSize)instructionWord.opcodeSize, address);
}

// ============================================================================

void AsmA65k::handleOperand_IndirectConstantPlusRegister(const string operand, InstructionWord instructionWord) // INC.b [1233 + r0]
{
    StringPair sp = splitStringByPlusSign(operand);
    string newOperand = sp.right + "+" + sp.left;
    handleOperand_IndirectRegisterPlusConstant(newOperand, instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_IndirectLabelPlusRegister(const string operand, InstructionWord instructionWord) // INC.b [label + r0]
{
    StringPair sp = splitStringByPlusSign(operand);
    string reversedOperands = sp.right + "+" + sp.left;
    handleOperand_IndirectRegisterPlusLabel(reversedOperands, instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegisterPlusConstant(const string operand, InstructionWord instructionWord) // INC.w [r0 + 1234]
{
    StringPair sp = splitStringByPlusSign(operand);
    
    // fill in rest of the instruction word
    instructionWord.addressingMode = AM_INDEXED1;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    // add constant after i.w.
    addData(OS_32BIT, convertStringToInteger(sp.right));
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegisterPlusLabel(const string operand, InstructionWord instructionWord) // INC.w [r0 + label]
{
    string operandString = removeSquaredBrackets(operand);
    StringPair sp = splitStringByPlusSign(operandString);

    // fill in rest of the instruction word
    instructionWord.addressingMode = AM_INDEXED1;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord); // 2 byte
    addRegisterConfigurationByte(sp.left); // 1 byte
    // add constant after RCB
    addData(OS_32BIT, resolveLabel(sp.right, PC)); // 4 bytes
}

// ============================================================================

void AsmA65k::handleOperand_IndirectConstant(const dword constant, InstructionWord instructionWord) // INC.w [$ffff]
{
    instructionWord.addressingMode = AM_ABSOLUTE1;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    addInstructionWord(instructionWord);
    addData(OS_32BIT, constant);
}

// ============================================================================

void AsmA65k::handleOperand_Register(const string operand, InstructionWord instructionWord)
{
    // finish creating the instruction word, then add it to the current segment
    instructionWord.addressingMode = AM_REGISTER1;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(operand);
}

// ============================================================================

void AsmA65k::addRegisterConfigurationByte(const string registerString)
{
    // check for valid register specification in operand
    RegisterType registerIndex = detectRegisterType(registerString);
    
    addData(OS_8BIT, registerIndex);
}

// ============================================================================

void AsmA65k::handleOperand_Constant(const string operand, InstructionWord instructionWord, const dword effectiveAddress)
{
    const byte instruction = instructionWord.instructionCode;
    // if branching instruction, eg.: BNE 363
    if(instruction >= I_BRA && instruction <= I_BGE)
    {
        instructionWord.addressingMode = AM_RELATIVE;
        instructionWord.registerConfiguration = RC_NOREGISTER;

        int32_t diff = effectiveAddress - PC;
        
        instructionWord.opcodeSize = getOpcodeSizeFromSignedInteger(diff);
        addInstructionWord(instructionWord);
        addData((OpcodeSize)instructionWord.opcodeSize, diff);
    }
    else
        switch (instruction)
        {
            case I_PSH: // psh $ff
                verifyRangeForConstant(operand, (OpcodeSize)instructionWord.opcodeSize);
                instructionWord.addressingMode = AM_CONST_IMMEDIATE;
                instructionWord.registerConfiguration = RC_NOREGISTER;
                addInstructionWord(instructionWord);
                addData((OpcodeSize)instructionWord.opcodeSize, convertStringToInteger(operand));
                break;
            case I_JMP: // jmp $43434
            case I_JSR: // jsr $3434
                // these always use 32 bit address/constant
                instructionWord.addressingMode = AM_DIRECT;
                instructionWord.registerConfiguration = RC_NOREGISTER;
                addInstructionWord(instructionWord);
                addData(OS_32BIT, convertStringToInteger(operand));
                break;
        }
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegister(const string operand, InstructionWord instructionWord)
{
    // verify register correctness and extract register name from square brackets
    static const regex rx_extractRegister(R"(\[\s*((r[0-9]{1,2})|(pc)|(sp))\s*\])", regex_constants::icase);
    smatch registerMatch;
    if(regex_match(operand, registerMatch, rx_extractRegister) == false)
        throwException_InvalidRegister();

    // fill in the needed records in the instruction word
    instructionWord.addressingMode = AM_REGISTER_INDIRECT1;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(registerMatch[1]);
}

// ============================================================================

string AsmA65k::detectAndRemoveLabelDefinition(string line)
{
    const static regex rx_detectLabel(R"(^(\s*[a-z][a-z_0-9]*:\s*)(.*))", regex_constants::icase);
    // check is there's a label definition at the beginning of the line
    if(regex_match(line, rx_detectLabel))
        line = regex_replace(line, rx_detectLabel, "$2");
    
    return line;
}

// ============================================================================

byte AsmA65k::getOpcodeSize(const string modifierCharacter)
{
    if(modifierCharacter == "b")
        return OS_8BIT;
    if(modifierCharacter == "w")
        return OS_16BIT;
    if(modifierCharacter == "")
        return OS_NONE;
    
    if(modifierCharacter == "u" || modifierCharacter == "s")
        return OS_DIVSIGN;
    
    AsmError error(actLineNumber, actLine);
    error.errorMessage = "Invalid size specifier";
    throw error;
    
    return OS_NONE; // never reached. Just to silence warning.
}

// ============================================================================

void AsmA65k::addInstructionWord(const InstructionWord instructionWord)
{
    segments.back().addWord(*(word *)&instructionWord);
    PC += 2;
}

// ============================================================================

void AsmA65k::addData(const OpcodeSize size, const dword data)
{
    switch(size)
    {
        case OS_32BIT:
            segments.back().addDword(data);
            PC += 4;
            break;
        case OS_16BIT:
            segments.back().addWord(data);
            PC += 2;
            break;
        case OS_8BIT:
            segments.back().addByte(data);
            PC++;
            break;
        case OS_DIVSIGN:
            throwException_InternalError();
    }
}

// ============================================================================

void AsmA65k::addData(const string sizeSpecifier, const dword data)
{
    if(sizeSpecifier == "b")
    {
        addData(OS_8BIT, data);
        return;
    }
    if(sizeSpecifier == "w")
    {
        addData(OS_16BIT, data);
        return;
    }
    if(sizeSpecifier == "")
    {
        addData(OS_32BIT, data);
        return;
    }
    
    throw AsmError(actLineNumber, actLine, "Invalid size specifier");
}

// ============================================================================

dword AsmA65k::resolveLabel(const string label, const dword address, const OpcodeSize size)
{
//    log("resolveLabel: '%s'\n", label.c_str());
    dword effectiveAddress = 0;
    
    if(labels.find(label) == labels.end())
    {
        LabelLocation labelLocation;
        labelLocation.address = address;
        labelLocation.opcodeSize = size;
        labelLocation.lineContent = actLine;
        labelLocation.lineNumber = actLineNumber;
        unresolvedLabels[label].push_back(labelLocation);
        
        return 0;
    }
    else
    {
        effectiveAddress = labels[label];
    }
    
    return effectiveAddress;
}

// ============================================================================

AsmA65k::OperandTypes AsmA65k::detectOperandType(const string operandStr)
{
    const static regex_constants::syntax_option_type icase = regex_constants::icase;
    const static regex rx_matchDoubleOperands       (R"((.*)\s*,\s*(.*))", icase);
    const static regex rx_constant                  (R"([$%]?[0-9a-f]+)", icase);                                            // 64
    const static regex rx_label                     (R"([a-z][a-z_0-9]*)", icase);                                           // names
    const static regex rx_register                  (R"((r[0-9]{1,2})|(PC|SP))", icase);                                     // r0
    const static regex rx_indirectConstant          (R"(\[\s*[$%]?[0-9a-f]+\s*\])", icase);                                  // [$5000]
    const static regex rx_indirectLabel             (R"(\[[a-z][a-z_0-9]*\])", icase);                                       // [names]
    const static regex rx_indirectRegister          (R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\])", icase);                         // [r0]
    const static regex rx_indirectRegisterPlusConst (R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\+\s*[$%]?[0-9]+\s*\])", icase);      // [r0 + $1000]
    const static regex rx_indirectConstPlusRegister (R"(\[\s*[$%]?[0-9a-f]+\s*\+\s*((r[0-9]{1,2})|(pc))\s*\])", icase);      // [$1000 + r0]
    const static regex rx_indirectRegisterPlusLabel (R"(\[\s*((r[0-9]{1,2})|(PC|SP))\s*\+\s*[a-z][a-z_0-9]*\s*\])", icase);  // [r0 + names]
    const static regex rx_indirectLabelPlusRegister (R"(\[\s*[a-z][a-z_0-9]*\s*\+\s*((r[0-9]{1,2})|(PC|SP))\s*\])", icase);  // [names + r0]
    
    if(operandStr == "")
        return OT_NONE;
    
    smatch match;
    if(regex_match(operandStr, match, rx_matchDoubleOperands)) // double operands
    {
        string left = match[1];
        string right = match[2];
        
        //log("double operands detected. left = '%s', right = '%s'\n", left.c_str(), right.c_str());
        
        if(regex_match(left, rx_register) && regex_match(right, rx_register))
            return OT_REGISTER__REGISTER;
        if(regex_match(left, rx_register) && regex_match(right, rx_constant))
            return OT_REGISTER__CONSTANT;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectConstant))
            return OT_REGISTER__INDIRECT_CONSTANT;
        if(regex_match(left, rx_indirectRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER__REGISTER;
        if(regex_match(left, rx_indirectLabel) && regex_match(right, rx_register))
            return OT_INDIRECT_LABEL__REGISTER;
        if(regex_match(left, rx_indirectConstant) && regex_match(right, rx_register))
            return OT_INDIRECT_CONSTANT__REGISTER;
        if(regex_match(left, rx_indirectRegisterPlusConst) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER;
        if(regex_match(left, rx_indirectRegisterPlusLabel) && regex_match(right, rx_register))
            return OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER;
        if(regex_match(left, rx_indirectLabelPlusRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER;
        if(regex_match(left, rx_indirectConstPlusRegister) && regex_match(right, rx_register))
            return OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectRegister))
            return OT_REGISTER__INDIRECT_REGISTER;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectRegisterPlusConst))
            return OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectRegisterPlusLabel))
            return OT_REGISTER__INDIRECT_REGISTER_PLUS_LABEL;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectConstPlusRegister))
            return OT_REGISTER__INDIRECT_CONSTANT_PLUS_REGISTER;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectLabelPlusRegister))
            return OT_REGISTER__INDIRECT_LABEL_PLUS_REGISTER;
        if(regex_match(left, rx_register) && regex_match(right, rx_label))
            return OT_REGISTER__LABEL;
        if(regex_match(left, rx_register) && regex_match(right, rx_indirectLabel))
            return OT_REGISTER__INDIRECT_LABEL;
    }
    else // single operand
    {
        //printf("detectOperandType(): single operand detected. operand = '%s'\n", operandStr.c_str());
        
        if(regex_match(operandStr, rx_register))
            return OT_REGISTER;
        if(regex_match(operandStr, rx_constant))
            return OT_CONSTANT;
        if(regex_match(operandStr, rx_indirectConstant))
            return OT_INDIRECT_CONSTANT;
        if(regex_match(operandStr, rx_indirectRegister))
            return OT_INDIRECT_REGISTER;
        if(regex_match(operandStr, rx_indirectRegisterPlusConst))
            return OT_INDIRECT_REGISTER_PLUS_CONSTANT;
        if(regex_match(operandStr, rx_indirectConstPlusRegister))
            return OT_INDIRECT_CONSTANT_PLUS_REGISTER;
        if(regex_match(operandStr, rx_indirectRegisterPlusLabel))
            return OT_INDIRECT_REGISTER_PLUS_LABEL;
        if(regex_match(operandStr, rx_indirectLabelPlusRegister))
            return OT_INDIRECT_LABEL_PLUS_REGISTER;
        if(regex_match(operandStr, rx_label))
            return OT_LABEL;
        if(regex_match(operandStr, rx_indirectLabel))
            return OT_INDIRECT_LABEL;
    }

    throwException_InvalidOperands();
    
    return OT_NONE; // fake line to silence warning
}
