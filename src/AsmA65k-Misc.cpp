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
    static const regex rx_checkIfBin(R"(^%([0-1]+))", regex_constants::icase);
    static const regex rx_checkIfHex(R"(^\$([0-9a-z]+))", regex_constants::icase);
    static const regex rx_checkIfDec(R"(^([0-9]+))", regex_constants::icase);
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

void AsmA65k::throwException_SymbolOutOfRange()
{
    AsmError error(actLineNumber, actLine, "Symbol out of range for specified size");
    throw error;
}

// ============================================================================

void AsmA65k::checkIntegerRange(const uint64_t result)
{
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

void AsmA65k::throwException_InvalidOperands()
{
    AsmError error(actLineNumber, actLine, "Invalid operand");
    throw error;
}

// ============================================================================

string AsmA65k::removeSquaredBrackets(const string operand)
{
    const static regex rx_removeSquareBrackets(R"(\[\s*(.*)\s*\])");
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
    if(registerStr == "pc") // TODO: verify that the input string is in lower case
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
    
    if(registerIndex < REG_R0 || registerIndex > REG_R15)
        throwException_InvalidRegister();
    
    return (RegisterType)registerIndex;
}
