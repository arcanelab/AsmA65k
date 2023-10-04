//
//  AsmA65k-Misc.cpp
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

int AsmA65k::convertStringToInteger(const string valueStr)
{
    static const regex rx_checkIfBin(R"(^%([01]+))", regex_constants::icase);
    static const regex rx_checkIfHex(R"(^\$([0-9a-z]+))", regex_constants::icase);
    static const regex rx_checkIfDec(R"(^(-?[0-9]+))", regex_constants::icase);
    static const string table = "0123456789ABCDEF";
    smatch rxResult;
    int i;
    uint64_t result = 0;
    char actChar;
    int length;
    uint64_t baseValue = 1;
    
    if(regex_search(valueStr, rxResult, rx_checkIfBin))
    {
        const string tmpStr = rxResult[1];
        length = (int)tmpStr.length();
        for(i = length-1; i >= 0; i--)
        {
            actChar = tmpStr[i];
            if(actChar=='1')
                result += baseValue;
            baseValue *= 2;
        }
        
        checkIntegerRange(result);
        return (int)result;
    }
    
    if(regex_search(valueStr, rxResult, rx_checkIfHex))
    {
        string tmpStr = rxResult[1];
        length = (int)tmpStr.length();
        for(i = length-1; i >= 0; i--)
        {
            actChar = tmpStr[i];
            result += findChar(table, actChar) * baseValue;
            baseValue *= 16;
        }
        
        checkIntegerRange(result);
        return (int)result;
    }
    
    if(regex_search(valueStr, rxResult, rx_checkIfDec))
    {
        string tmpStr = rxResult[1];
        length = (int)tmpStr.length();
        for(i = length-1; i >= 0; i--)
        {
            actChar = tmpStr[i];
            int charIndex = findChar(table, actChar);
            if(charIndex == -1)
            {
                result *= -1;
                continue;
            }
            result += findChar(table, actChar) * baseValue;
            baseValue *= 10;
        }
        
        checkIntegerRange(result);
        return (int)result;
    }
    
    throwException_InvalidNumberFormat(); // invalid number format
    return 0;
}

// ============================================================================

int AsmA65k::findChar(const string text, char c)
{
	const int size = (int)text.size();
    c = toupper(c);
	
    if(c=='-')
        return -1;

	for(int i = 0; i < size; i++)
		if(text[i] == c)
			return i;
	
	return 0;
}

// ============================================================================

void AsmA65k::throwException_ValueOutOfRange()
{
    AsmError error(actLineNumber, actLine, "Value out of range");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_InternalError()
{
    AsmError error(actLineNumber, actLine, "Internal Error");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_InvalidNumberFormat()
{
    AsmError error(actLineNumber, actLine, "Invalid number format");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_SyntaxError(const string line)
{
    AsmError error(actLineNumber, line, "Syntax error");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_InvalidRegister()
{
    AsmError error(actLineNumber, actLine, "Invalid register specified");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_SymbolOutOfRange()
{
    AsmError error(actLineNumber, actLine, "Symbol out of range for specified size");
    throw error;
}

// ============================================================================

void AsmA65k::checkIntegerRange(const int64_t result)
{
    if(result < 0)
    {
        if(result < -2147483648)
            throwException_ValueOutOfRange();
        if(result > 2147483647)
            throwException_ValueOutOfRange();

        return;
    }

    const uint64_t maxInt = 0xffffffff;
    
    if((uint64_t)result > maxInt)
    {
        AsmError error(actLineNumber, actLine, "Value exceeding 32 bit range");
        throw error;
    }
}

// ============================================================================

bool AsmA65k::isCommentLine(const string line)
{
    const static regex rx_isLineComment(R"((^\s*$)|(\s*;.*))"); // empty or comment line
    if(regex_match(line, rx_isLineComment))
        return true;
    
    return false;
}

// ============================================================================

void AsmA65k::throwException_InvalidMnemonic()
{
    AsmError error(actLineNumber, actLine, "Invalid opcode");
    throw error;
}

// ============================================================================

void AsmA65k::throwException_InvalidOperands()
{
    AsmError error(actLineNumber, actLine, "Invalid operand");
    throw error;
}

// ============================================================================

string AsmA65k::removeSquaredBrackets(const string operand)
{
    const static regex rx_removeSquareBrackets(R"(\[\s*(.*)\s*\][\+\-]?)");
    smatch result;
    if(regex_match(operand, result, rx_removeSquareBrackets) == false)
    {
        throwException_InternalError();
    }
    
    return result[1].str();
}

// ============================================================================

AsmA65k::StringPair AsmA65k::splitStringByPlusSign(const string operand)
{
    // extract the two parts
    static const regex rx_matchOperands(R"((.*)\s*\+\s*(.*))");
    smatch operandsMatch;
    if(regex_match(operand, operandsMatch, rx_matchOperands) == false)
        throwException_InvalidOperands();
    
    StringPair sp;
    // put them into their respective strings
    sp.left = operandsMatch[1].str(); // "r0"
    sp.right = operandsMatch[2].str(); // "1234"

    return sp;
}

// ============================================================================

AsmA65k::StringPair AsmA65k::splitStringByComma(const string operand)
{
    // extract the two parts
    static const regex rx_matchOperands(R"((.*)\s*,\s*(.*))");
    smatch operandsMatch;
    if(regex_match(operand, operandsMatch, rx_matchOperands) == false)
        throwException_InvalidOperands();
    
    StringPair sp;
    // put them into their respective strings
    sp.left = operandsMatch[1].str(); // "r0"
    sp.right = operandsMatch[2].str(); // "1234"
    
    return sp;
}

// ============================================================================

AsmA65k::RegisterType AsmA65k::detectRegisterType(const string registerStr)
{
    if(registerStr == "pc")
        return REG_PC;
    if(registerStr == "sp")
        return REG_SP;
    
    static const regex rx_detectRegisterIndex(R"(r([0-9]{1,2}))");
    smatch registerIndexMatch;
    if(regex_match(registerStr, registerIndexMatch, rx_detectRegisterIndex) == false)
        throwException_InvalidRegister();

    int registerIndex;
    
    try {
        registerIndex = std::stoi(registerIndexMatch[1].str());
    } catch (...) {
        throwException_InvalidRegister();
    }
    
    if(registerIndex < REG_R0 || registerIndex > REG_R13)
        throwException_InvalidRegister();
    
    return (RegisterType)registerIndex;
}

// ============================================================================

bool AsmA65k::findAddressingMode(const string mnemonic, AddressingModes am)
{
    return (std::find(opcodes[mnemonic].addressingModesAllowed.begin(), opcodes[mnemonic].addressingModesAllowed.end(), am)) != opcodes[mnemonic].addressingModesAllowed.end();
}

// ============================================================================

void AsmA65k::checkIfAddressingModeIsLegalForThisInstruction(const string mnemonic, const OperandTypes operandType)
{
    AddressingModes addressingMode = getAddressingModeFromOperand(operandType);
    
    if(addressingMode == AM_AMBIGOUS) // AM_RELATIVE or AM_DIRECT or AM_CONST_IMMEDIATE
    {
        if (findAddressingMode(mnemonic, AM_RELATIVE)) return;
        if (findAddressingMode(mnemonic, AM_DIRECT)) return;
        if (findAddressingMode(mnemonic, AM_CONST_IMMEDIATE)) return;
    }
    else if( findAddressingMode(mnemonic, addressingMode) )
        return;
    
    AsmError error(actLineNumber, actLine, "Invalid addressing mode");
    throw error;
}

// ============================================================================

void AsmA65k::checkIfSizeSpecifierIsAllowed(const string mnemonic, const OpcodeSize opcodeSize)
{
    if((opcodes[mnemonic].isSizeSpecifierAllowed == false) && (opcodeSize != OS_NONE))
    {
        AsmError error(actLineNumber, actLine, "Size specifier is not allowed for this instruction");
        throw error;
    }
}

// ============================================================================

AsmA65k::OpcodeSize AsmA65k::getOpcodeSizeFromSignedInteger(int32_t value)
{
    if(value >= -128 && value <= 127)
        return OS_8BIT;
    if(value >= -32768 && value <= 32767)
        return OS_16BIT;
    
    return OS_32BIT;
}

// ============================================================================

AsmA65k::OpcodeSize AsmA65k::getOpcodeSizeFromUnsigedInteger(uint64_t value)
{
    uint32_t unisgnedValue = (uint32_t)value;

    if(unisgnedValue <= 0xff)
        return OS_8BIT;
    
    if(unisgnedValue <= 0xffff)
        return OS_16BIT;
    
    return OS_32BIT;
}

// ============================================================================

void AsmA65k::verifyRangeForConstant(const string constant, OpcodeSize opcodeSize)
{
    if(getOpcodeSizeFromUnsigedInteger(convertStringToInteger(constant)) < opcodeSize)
        throwException_SymbolOutOfRange();
}

// ============================================================================

void AsmA65k::verifyRangeForConstant(const uint32_t constant, OpcodeSize opcodeSize)
{
    if(getOpcodeSizeFromUnsigedInteger(constant) < opcodeSize)
        throwException_SymbolOutOfRange();
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

uint8_t AsmA65k::getOpcodeSize(const string modifierCharacter)
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
    segments.back().addWord(*(uint16_t *)&instructionWord);
    PC += 2;
}

// ============================================================================

void AsmA65k::addData(const OpcodeSize size, const uint32_t data)
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

void AsmA65k::addData(const string sizeSpecifier, const uint32_t data)
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

uint32_t AsmA65k::resolveLabel(const string label, const uint32_t address, const OpcodeSize size, bool isRelative)
{
    static const regex rx_removeSurroundingWhiteSpace(R"(\s*(\S*)\s*)");
    smatch result;
    if(regex_match(label, result, rx_removeSurroundingWhiteSpace)==false)
        throwException_InternalError();
    
    //    log("resolveLabel: '%s'\n", label.c_str());
    uint32_t effectiveAddress = 0;
    string cleanLabel = result[1].str();
    
    if(labels.find(cleanLabel) == labels.end())
    {
        LabelLocation labelLocation;
        labelLocation.address = address;
        labelLocation.opcodeSize = size;
        labelLocation.lineContent = actLine;
        labelLocation.lineNumber = actLineNumber;
        labelLocation.isRelative = isRelative;
        unresolvedLabels[cleanLabel].push_back(labelLocation);
        
        return 0;
    }
    else
    {
        effectiveAddress = labels[cleanLabel];
    }
    
    return effectiveAddress;
}

// ============================================================================

void AsmA65k::handleDoubleRegisters(const StringPair sp, InstructionWord instructionWord, PostfixType postFix)
{
    RegisterType regLeft = detectRegisterType(sp.left);
    RegisterType regRight = detectRegisterType(sp.right);
    
    uint8_t registerSelector = ((regLeft & 15) << 4) | (regRight & 15);
    
    switch (postFix) {
        case PF_INC:
            instructionWord.registerConfiguration = RC_2REGISTERS_POSTINCREMENT;
            break;
        case PF_DEC:
            instructionWord.registerConfiguration = RC_2REGISTERS_PREDECREMENT;
            break;
        default:
            instructionWord.registerConfiguration = RC_2REGISTERS;
            break;
    }
    addInstructionWord(instructionWord); // 2 bytes
    addData(OS_8BIT, registerSelector); // 1 byte
}

void AsmA65k::addRegisterConfigurationByte(const string registerString, InstructionWord instructionWord, const PostfixType postFix)
{
    switch (postFix)
    {
        case PF_INC:
            instructionWord.registerConfiguration = RC_REGISTER_POSTINCREMENT;
            break;
        case PF_DEC:
            instructionWord.registerConfiguration = RC_REGISTER_PREDECREMENT;
            break;
        default:
            instructionWord.registerConfiguration = RC_REGISTER;
            break;
    }
    RegisterType registerIndex = detectRegisterType(registerString);

    addInstructionWord(instructionWord); // 2 bytes
    addData(OS_8BIT, registerIndex); // 1 byte
}

// ============================================================================

AsmA65k::PostfixType AsmA65k::getPostFixType(const string operand)
{
    static const regex rx_detectPostfixSign(R"((.*\])([\+\-])$)");
    string sign;
    smatch result;
    if(regex_match(operand, result, rx_detectPostfixSign) == true)
        sign = result[2].str();
    else
        return PF_NONE;
    
    if(sign=="+")
        return PF_INC;

    if(sign=="-")
        return PF_DEC;
    
    return PF_NONE; // "fake" return to silence warning
}
