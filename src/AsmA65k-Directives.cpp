//
//  AsmA65k-Directives.cpp
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

bool AsmA65k::ProcessDirectives(const string& line)
{
    const int directiveType = DetectDirective(line); // explicit declaration is intentional
    switch (directiveType)
    {
    case DIRECTIVE_NONE:
        return false;

    case DIRECTIVE_SETPC:
        HandleDirective_SetPC(line);
        break;

    case DIRECTIVE_TEXT:
    case DIRECTIVE_TEXTZ:
        HandleDirective_Text(line, directiveType);
        break;

    case DIRECTIVE_BYTE:
    case DIRECTIVE_WORD:
    case DIRECTIVE_DWORD:
        HandleDirective_ByteWordDword(line, directiveType);
        break;

    case DIRECTIVE_DEFINE:
        HandleDirective_Define(line);
        break;

    default:
    {
        AsmError error(actLineNumber, line, "Internal error: directive handler not implemented");
        throw error;
    }
    }

    return true;
}

int AsmA65k::DetectDirective(const string& line)
{
    const static regex rx_detectDirective(R"(^(\s*|.*:\s*)\.([a-z]+).*)", regex_constants::icase);
    smatch directiveCandidate;

    if (regex_search(line, directiveCandidate, rx_detectDirective) == 0)
        return DIRECTIVE_NONE;

    if (directiveCandidate[2].str() == "pc")
        return DIRECTIVE_SETPC;

    if (directiveCandidate[2].str() == "text")
        return DIRECTIVE_TEXT;

    if (directiveCandidate[2].str() == "textz")
        return DIRECTIVE_TEXTZ;

    if (directiveCandidate[2].str() == "def")
        return DIRECTIVE_DEFINE;

    if (directiveCandidate[2].str() == "byte")
        return DIRECTIVE_BYTE;

    if (directiveCandidate[2].str() == "word")
        return DIRECTIVE_WORD;

    if (directiveCandidate[2].str() == "dword")
        return DIRECTIVE_DWORD;

    AsmError error(actLineNumber, line, "Unrecognized directive");
    throw error;
}

void AsmA65k::HandleDirective_SetPC(const string& line)
{
    static const regex rx_matchNumberInSetPCDirectiveLine(R"(\s*.pc\s*=\s*(([$%]?[0-9a-f]+$)|(([$%]?[0-9a-f]+)[\s|;])).*)", regex_constants::icase);
    smatch value;

    if (regex_search(line, value, rx_matchNumberInSetPCDirectiveLine) == false || regex_match(line, rx_matchNumberInSetPCDirectiveLine) == false)
    {
        AsmError error(actLineNumber, line, "No valid value found for .pc directive");
        throw error;
    }

    PC = ConvertStringToInteger(value[1].str());

    // create a new segment, store it in 'segments' vector
    segments.push_back(Segment());
    segments.back().address = PC;
}

void AsmA65k::HandleDirective_Text(const string& line, const int directiveType)
{
    static const regex rx_matchTextInTextDirective(R"(.*\.text\s*\"(.*)\".*)", regex_constants::icase);
    smatch textMatch;

    if (regex_search(line, textMatch, rx_matchTextInTextDirective) == false)
    {
        AsmError error(actLineNumber, line, "No valid data found after .text directive");
        throw error;
    }

    if (segments.empty())
    {
        AsmError error(actLineNumber, line, "A .pc directive must precede a .text directive");
        throw error;
    }

    unsigned i = 0;
    const size_t textLength = textMatch[1].str().length();

    do
    {
        segments.back().data.push_back(textMatch[1].str()[i]); // store text into current segment
    } while (++i < textLength);

    if (directiveType == DIRECTIVE_TEXTZ) // add additional '0' for textz directive
        segments.back().data.push_back('0');

    PC += textLength;
}

void AsmA65k::HandleDirective_ByteWordDword(const string& line, const int directiveType)
{ // trim line back to the raw data after the directive
    static const regex rx_matchDataInByteDirective(R"(.*\.(byte|word|dword)\s+(.+)\s*.*)", regex_constants::icase);
    smatch dataMatch;

    // check if line is valid
    if (regex_search(line, dataMatch, rx_matchDataInByteDirective) == false)
    {
        AsmError error(actLineNumber, line);
        error.errorMessage = "Invalid data found after .";
        error.errorMessage += dataMatch[1];
        error.errorMessage += " directive";

        throw error;
    }

    // check if there's an existing segment already
    if (segments.empty())
    {
        AsmError error(actLineNumber, line);
        error.errorMessage = "A .pc directive must precede a .";
        error.errorMessage += dataMatch[1];
        error.errorMessage += " directive";

        throw error;
    }

    // match the individual numbers
    static const regex rx_matchNumber(R"(([%$]?([0-9a-f]+|\s*[a-z][a-z_0-9]*))\s*,?\s*)");
    const string matchStr = dataMatch[2].str(); // dataMatch[1] = (byte|word|dword); dataMatch[2] = "%1, $2, 3, 4", etc
    sregex_token_iterator iter(matchStr.begin(), matchStr.end(), rx_matchNumber, 1);
    const sregex_token_iterator end;
    while (iter != end) // iterate through sub-matches, that is, each data element after the directive, skipping ',' and white space
    {
        uint32_t value;
        const string token = iter->str();
        // check if the string is a label
        if (token[0] >= 'a' && token[0] <= 'z')
            value = ResolveLabel(token, PC, OS_32BIT);
        else
            value = ConvertStringToInteger(token);

        // handle data size
        switch (directiveType)
        {
        case DIRECTIVE_BYTE:
            if (value < 0 || value > 255)
                ThrowException_ValueOutOfRange();

            segments.back().AddByte(value); // store data as byte
            PC++;
            break;

        case DIRECTIVE_WORD:
            if (value < 0 || value > 65535)
                ThrowException_ValueOutOfRange();

            segments.back().AddWord(value); // store data as word
            PC += 2;
            break;

        case DIRECTIVE_DWORD:
            if (value < 0)
                ThrowException_ValueOutOfRange();

            segments.back().AddDword(value); // store data as dword
            PC += 4;
            break;

        default: // can never ever get here
            break;
        } // switch

        ++iter; // step the iterator
    }           // while
}

void AsmA65k::HandleDirective_Define(const string& line)
{
    const static regex rx_matchDefintionLine(R"(\s*\.def\s+([a-z][a-z_0-9]*)\s*=\s*([^;]+)\s*;?.*)", regex_constants::icase);
    smatch matches;

    if (regex_match(line, matches, rx_matchDefintionLine) == false)
    {
        AsmError error(actLineNumber, line, "Invalid definition");
        throw error;
    }

    // store label in its own variable and convert it into lower case
    string label = matches[1].str();
    std::transform(label.begin(), label.end(), label.begin(), ::tolower);

    // check if rvalue is a single constant
    const static regex rx_matchConstant(R"(([%$]?[0-9a-f]+)\s*)", regex_constants::icase);
    smatch valueMatch;
    string match2 = matches[2].str();
    if (regex_match(match2, valueMatch, rx_matchConstant))
    { // if yes, convert it into decimal and add it into the symbol table
        labels[label] = ConvertStringToInteger(valueMatch[1].str());

        return;
    }

    // check if rvalue is an expression (only the simple 'label + const' format is allowed)
    const static regex rx_matchExpression(R"(([a-z][a-z_0-9]*)\s*\+\s*([%$]?[0-9a-f]+).*)", regex_constants::icase);
    smatch expressionOperands;
    match2 = matches[2].str();
    if (regex_match(match2, expressionOperands, rx_matchExpression))
    {
        // read lvalue and convert it into lower case
        string lvalue = expressionOperands[1].str();
        std::transform(lvalue.begin(), lvalue.end(), lvalue.begin(), ::tolower);

        // check if lvalue symbol exists
        if (labels.find(lvalue) == labels.end())
        {
            AsmError error(actLineNumber, line);
            error.errorMessage = "Symbol not defined: ";
            error.errorMessage += expressionOperands[1].str();

            throw error;
        }

        // read rvalue string
        string rvalue = expressionOperands[2].str();

        // look up symbol (lvalue) and add the decimal value of the rvalue to it, then add the result as a new symbol
        labels[label] = (uint32_t)labels[lvalue] + (uint32_t)ConvertStringToInteger(rvalue);

        return;
    }

    AsmError error(actLineNumber, line, "Invalid defintion expression");
    throw error;
}
