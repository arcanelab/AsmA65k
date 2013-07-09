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
    
    string mnemonic = asmMatches[1].str();
    string modifier = asmMatches[2].str();
    string operand  = asmMatches[3].str();
    
    // remove comment from operand
    const static regex rx_detectComment(R"((.*\S)\s*;.*)", regex_constants::icase);
    smatch cleanOperand;
    if(regex_match(operand, cleanOperand, rx_detectComment))
        operand = cleanOperand[1].str();
    
    std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::tolower);
    std::transform(modifier.begin(), modifier.end(), modifier.begin(), ::tolower);
    std::transform(operand.begin(), operand.end(), operand.begin(), ::tolower);
    assembleInstruction(mnemonic, modifier, operand);
}

// ============================================================================

void AsmA65k::assembleInstruction(const string mnemonic, const string modifier, const string operand)
{
    
    //printf("assembleInstruction(): instr. = '%s',\t\t.b/w = '%1s',\t\t\toperands = '%s'\n", mnemonic.c_str(), modifier.c_str(), operand.c_str());
    //printf("OT_NONE: %d\nOT_REGISTER: %d\nOT_LABEL: %d\nOT_CONSTANT: %d\n\n", OT_NONE, OT_REGISTER, OT_LABEL, OT_CONSTANT);
    //printf("assembleInstruction(): operandType = %d\n", detectOperandType(operand));
    
    InstructionWord instructionWord;
    
    instructionWord.opcodeSize = setOpcodeSize(modifier);
    instructionWord.instructionCode = opcodes[mnemonic].instructionCode;
    
    dword effectiveAddress = 0;
    OperandTypes operandType = detectOperandType(operand);
    log("operandType = %d\n", operandType);
    switch (operandType)
    {
        case OT_NONE:                                        // SEI
            instructionWord.addressingMode = AM_NONE;
            instructionWord.registerConfiguration = RC_NOREGISTER;
            addInstructionWord(instructionWord);
            PC += 2;
            break;
        case OT_REGISTER:                                    // INC r0
            handleOperand_Register(operand, instructionWord);
            break;
        case OT_LABEL:                                       // INC label
            effectiveAddress = resolveLabel(operand);        // NOTE: break omitted on purpose!
        case OT_CONSTANT:                                    // BNE 40 or INC $f000
            handleOperand_Constant(operand, instructionWord, effectiveAddress);
            break;
        case OT_INDIRECT_REGISTER:                           // INC [r0]
            handleOperand_IndirectRegister(operand, instructionWord);
            break;
        case OT_INDIRECT_LABEL:                              // INC [label]
            effectiveAddress = resolveLabel(removeSquaredBrackets(operand));
            handleOperand_IndirectConstant(effectiveAddress, instructionWord);
            break;
        case OT_INDIRECT_CONSTANT:                           // INC.w [$ffff]
            try { effectiveAddress = convertStringToInteger(removeSquaredBrackets(operand)); }
            catch (...) { throwException_InvalidNumberFormat(); }
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
        case OT_REGISTER__INDIRECT_REGISTER_PLUS_CONSTANT:   // MOV r0, [r1 + 10]
            break;
        case OT_INDIRECT_REGISTER__REGISTER:                 // MOV [r0], r1
            break;
        case OT_INDIRECT_REGISTER_PLUS_LABEL__REGISTER:      // MOV [r0 + label], r1
        case OT_INDIRECT_REGISTER_PLUS_CONSTANT__REGISTER:   // MOV [r0 + 10], r1
            break;
        case OT_INDIRECT_LABEL_PLUS_REGISTER__REGISTER:      // MOV [label + r0], r1
        case OT_INDIRECT_CONSTANT_PLUS_REGISTER__REGISTER:   // MOV [1234 + r0], r1
            break;
        case OT_INDIRECT_LABEL__REGISTER:
        case OT_INDIRECT_CONSTANT__REGISTER:
            break;
        case OT_REGISTER__INDIRECT_LABEL:
        case OT_REGISTER__INDIRECT_CONSTANT:
            break;
    }
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectConstantPlusRegister(const string operand, InstructionWord instructionWord)// MOV r0, [$f000 + r1]
{
    StringPair sp = splitStringByComma(operand);
    sp.right = removeSquaredBrackets(sp.right);
    StringPair indexedOperandPair = splitStringByPlusSign(sp.right);
    
    instructionWord.addressingMode = AM_INDEXED_SRC;
    handleDoubleRegisters(StringPair(sp.left, indexedOperandPair.right), instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_Register_IndirectLabelPlusRegister(const string operand, InstructionWord instructionWord)
{
    
}

// ============================================================================

void AsmA65k::handleDoubleRegisters(StringPair sp, InstructionWord instructionWord)
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
    
    
    addInstructionWord(instructionWord);
    addData(OS_8BIT, registerSelector);
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
    instructionWord.addressingMode = AM_REGISTER2;
    StringPair sp = splitStringByComma(operand);
    handleDoubleRegisters(sp, instructionWord);
}

// ============================================================================

void AsmA65k::handleOperand_Register_Constant(const string operand, InstructionWord instructionWord) // MOV.b r0, 1234
{
    StringPair sp = splitStringByComma(operand);
    
    instructionWord.addressingMode = AM_IMMEDIATE;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    try { addData((OpcodeSize)instructionWord.opcodeSize, convertStringToInteger(sp.right)); }
    catch(...) { throwException_InvalidNumberFormat(); }
    PC += 3;
    switch(instructionWord.opcodeSize)
    {
        case OS_32BIT:
            PC += 4;
            break;
        case OS_16BIT:
            PC += 2;
            break;
        case OS_8BIT:
            PC++;
        default:
            break;
    }
}

// ============================================================================

void AsmA65k::handleOperand_Register_Label(const string operand, InstructionWord instructionWord) // MOV.b r0, label
{
    StringPair sp = splitStringByComma(operand);
    
    instructionWord.addressingMode = AM_IMMEDIATE;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord);
    addRegisterConfigurationByte(sp.left);
    addData((OpcodeSize)instructionWord.opcodeSize, resolveLabel(sp.right));
    PC += 3;
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
    try { addData(OS_32BIT, convertStringToInteger(sp.right)); }
    catch(...) { throwException_InvalidNumberFormat(); }
    PC += 7;
}

// ============================================================================

void AsmA65k::handleOperand_IndirectRegisterPlusLabel(const string operand, InstructionWord instructionWord) // INC.w [r0 + label]
{
    StringPair sp = splitStringByPlusSign(operand);

    // fill in rest of the instruction word
    instructionWord.addressingMode = AM_INDEXED1;
    instructionWord.registerConfiguration = RC_REGISTER;
    addInstructionWord(instructionWord); // 2 byte
    addRegisterConfigurationByte(sp.left); // 1 byte
    // add constant after i.w.
    addData(OS_32BIT, resolveLabel(sp.right)); // 4 bytes
    PC += 7;
}

// ============================================================================

void AsmA65k::handleOperand_IndirectConstant(const dword constant, InstructionWord instructionWord) // INC.w [$ffff]
{
    instructionWord.addressingMode = AM_ABSOLUTE1;
    instructionWord.registerConfiguration = RC_NOREGISTER;
    addInstructionWord(instructionWord);
    PC++;
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
    PC += 3;
}

// ============================================================================

void AsmA65k::addRegisterConfigurationByte(string registerString)
{
    // check for valid register specification in operand
    RegisterType registerIndex = detectRegisterType(registerString);
    
    log("register = %s\n", registerString.c_str());
    
    // check for special registers
    if(registerIndex == REG_PC || registerIndex == REG_SP)
    {
        segments.back().addByte(registerIndex); // code for PC or SP
        return;
    }
    
    // asserting correct range for index
    if(registerIndex < 0 || registerIndex > 15)
        throwException_InvalidRegister();
    
    addData(OS_8BIT, registerIndex);
//    segments.back().addByte(registerIndex);
}

// ============================================================================

void AsmA65k::handleOperand_Constant(const string operand, InstructionWord instructionWord, const dword effectiveAddress)
{
    const byte in = instructionWord.instructionCode;
    
    // if branching instruction, eg.: BNE 363
    if(in >= I_BRA && in <= I_BGE)
    {
        //instructionWord.addressingMode = AM_RELATIVE;
        dword diff = effectiveAddress - PC;
        *(dword *)&instructionWord = diff & 0xff; // we store the low byte of the relative address in the high byte of the instruction word
        addInstructionWord(instructionWord);
        segments.back().addByte((diff & 0xff00) >> 8);
    }
    else // non-branching instruction, eg.: DEC.w $ff32
    {
        // absolute?
        if (in == I_CLR || in == I_PSH || in == I_POP || in == I_INC || in == I_DEC )
        {
            instructionWord.addressingMode = AM_ABSOLUTE1;
        }
        else // direct?
        {
            if (in == I_JMP || in == I_JSR)
                instructionWord.addressingMode = AM_DIRECT;
            else // neither -> error
            {
                AsmError error(actLineNumber, actLine, "Invalid addressing mode");
                throw error;
            }
        }
        
        // either with absolute1 or direct, the registerConfiguration is empty
        instructionWord.registerConfiguration = RC_NOREGISTER;
        addInstructionWord(instructionWord); // add finished instruction word
        
        // depending on the size modifier, add effective address as operand
        addData((OpcodeSize)instructionWord.opcodeSize, effectiveAddress);

        // TODO: write a method that decides if an opcode size specifier is valid for a given instruction.
        if(instructionWord.opcodeSize == OS_NONE)
        {
            AsmError error(actLineNumber, actLine, "Invalid size specifier");
            throw error;
        }
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
    PC += 3;
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

byte AsmA65k::setOpcodeSize(const string modifierCharacter)
{
    if(modifierCharacter == "b")
        return OS_8BIT;
    if(modifierCharacter == "w")
        return OS_16BIT;
    if(modifierCharacter == "")
        return OS_32BIT;
    
    if(modifierCharacter == "u" || modifierCharacter == "s")
        return OS_NONE;
    
    AsmError error(actLineNumber, actLine);
    error.errorMessage = "Invalid size specifier";
    throw error;
    
    return OS_NONE; // never reached. Just to silence warning.
}

// ============================================================================

void AsmA65k::addInstructionWord(InstructionWord instructionWord)
{
    segments.back().addWord(*(word *)&instructionWord);
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
        case OS_NONE:
            log("Internal error");
            throw;
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

dword AsmA65k::resolveLabel(const string label)
{
    dword effectiveAddress = 0;
    
    if(labels.find(label) == labels.end())
    {
        unresolvedLabels[label].push_back(PC+2);
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
    const static regex rx_constant                  (R"([$%]?[0-9a-f]+)", icase);                                          // 64
    const static regex rx_label                     (R"([a-z][a-z_0-9]*)", icase);                                      // names
    const static regex rx_register                  (R"((r[0-9]{1,2})|(PC|SP))", icase);                                  // r0
    const static regex rx_indirectConstant          (R"(\[\s*[$%]?[0-9a-f]+\s*\])", icase);                             // [$5000]
    const static regex rx_indirectLabel             (R"(\[[a-z][a-z_0-9]*\])", icase);                                  // [names]
    const static regex rx_indirectRegister          (R"(\[\s*((r[0-9])|(PC|SP))\s*\])", icase);                        // [r0]
    const static regex rx_indirectRegisterPlusConst (R"(\[\s*((r[0-9])|(PC|SP))\s*\+\s*[$%]?[0-9]+\s*\])", icase);      // [r0 + $1000]
    const static regex rx_indirectConstPlusRegister (R"(\[\s*[$%]?[0-9a-f]+\s*\+\s*((r[0-9]{1,2})|(pc))\s*\])", icase);      // [$1000 + r0]
    const static regex rx_indirectRegisterPlusLabel (R"(\[\s*((r[0-9])|(PC|SP))\s*\+\s*[a-z][a-z_0-9]*\s*\])", icase);  // [r0 + names]
    const static regex rx_indirectLabelPlusRegister (R"(\[\s*[a-z][a-z_0-9]*\s*\+\s*((r[0-9])|(PC|SP))\s*\])", icase);  // [names + r0]
    
    if(operandStr == "")
        return OT_NONE;
    
    smatch match;
    if(regex_match(operandStr, match, rx_matchDoubleOperands)) // double operands
    {
        string left = match[1];
        string right = match[2];
        
        log("double operands detected. left = '%s', right = '%s'\n", left.c_str(), right.c_str());
        
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
