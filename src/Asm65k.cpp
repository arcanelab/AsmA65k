//
//  AsmA65k.cpp
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

std::vector<Segment>* AsmA65k::assemble(stringstream &source)
{
    initializeOpcodetable();
    
    while(getline(source, actLine))
    {
        if(isCommentLine(actLine))
        {
            actLineNumber++;
            continue;
        }
//        printf("-------------- line %d -------------- '%s'\n", actLineNumber, actLine.c_str());
        processLabelDefinition(actLine);
        if(!processDirectives(actLine))
            processAsmLine(actLine);
        actLineNumber++;
    }
    
    // iterate through all unresolved labels map
    for(auto unresolvedLabel : unresolvedLabels)
    {
        // extract vector of LabelLocations from map (as a reference)
        vector<LabelLocation> &locations = unresolvedLabel.second;
        string label = unresolvedLabel.first;

        // iterate through LabelLocations in vector
        for(auto &actLocation : locations)
        {
            // check if label had been resolved later in assembly file
            if(labels.find(label) == labels.end())
            {
                AsmError error(actLocation.lineNumber, actLocation.lineContent, "Undefined label: " + label);
                throw error;
            }
            
            // now we must find which segment this value must be written, so we iterate thorugh each segments
            for(auto &actSegment : segments)
            {
                // make boundary check for given segment
                if((actSegment.address <= actLocation.address) && ((actSegment.data.size()+actSegment.address) > actLocation.address))
                {
                    // suitable segment found, write value to stored address in that segment in the right size
                    dword value = labels[label];
                    
                    if(getOpcodeSizeFromUnsigedInteger(value) < actLocation.opcodeSize)
                    {
                        AsmError error(actLocation.lineNumber, actLocation.lineContent, "Symbol out of range for specified size");
                        throw error;
                    }
                    
                    switch (actLocation.opcodeSize)
                    {
                        case OS_8BIT:
                            actSegment.writeByte(actLocation.address, (byte)value);
                            break;
                        case OS_16BIT:
                            actSegment.writeWord(actLocation.address, (word)value);
                            break;
                        case OS_32BIT:
                            actSegment.writeDword(actLocation.address, (dword)value);
                            break;
                        default:
                            throwException_InternalError();
                    }
                    break;
                }
            }
        }
    }
    
    return &segments;
}

// ============================================================================

void AsmA65k::processLabelDefinition(const string line)
{
    const static regex rx_detectLabel(R"(\s*([a-z][a-z_0-9]*):.*)", regex_constants::icase);
    smatch labelMatch;
    if(regex_match(line, labelMatch, rx_detectLabel))
    {
        string label = labelMatch[1].str();
        std::transform(label.begin(), label.end(), label.begin(), ::tolower);

        if(labels.find(label) != labels.end()) // check if already contains
        {
            AsmError error(actLineNumber, line);
            error.errorMessage = "Label '";
            error.errorMessage += label;
            error.errorMessage += "' already defined";

            throw error;
        }
        labels[label] = PC;
    }
}

// ============================================================================

void AsmA65k::initializeOpcodetable()
{
    OpcodeAttribute oa;
    oa.isSizeSpecifierAllowed = true;
    oa.isPostfixEnabled = true;

    oa.addressingModesAllowed = { AM_DIRECT, AM_REGISTER1, AM_ABSOLUTE1, AM_REGISTER_INDIRECT1, AM_INDEXED1 };
    oa.instructionCode = I_JMP; opcodes["jmp"] = oa;
    oa.instructionCode = I_JSR; opcodes["jsr"] = oa;

    oa.addressingModesAllowed = { AM_REGISTER1, AM_REGISTER_INDIRECT1, AM_ABSOLUTE1, AM_INDEXED1 };
    oa.instructionCode = I_CLR; opcodes["clr"] = oa;
    oa.instructionCode = I_POP; opcodes["pop"] = oa;
    oa.instructionCode = I_INC; opcodes["inc"] = oa;
    oa.instructionCode = I_DEC; opcodes["dec"] = oa;

    oa.addressingModesAllowed = { AM_CONST_IMMEDIATE, AM_REGISTER1, AM_REGISTER_INDIRECT1, AM_ABSOLUTE1, AM_INDEXED1 };
    oa.instructionCode = I_PSH; opcodes["push"] = oa;

    oa.addressingModesAllowed = { AM_REG_IMMEDIATE,         // Rx, const
                                  AM_REGISTER2,             // Rx, Ry
                                  AM_ABSOLUTE_SRC,          // Rx, [$1234]
                                  AM_ABSOLUTE_DEST,         // [$1234], Rx
                                  AM_REGISTER_INDIRECT_SRC, // Rx, [Ry]
                                  AM_REGISTER_INDIRECT_DEST,// [Rx], Ry
                                  AM_INDEXED_SRC,           // Rx, [Ry + 123]
                                  AM_INDEXED_DEST };        // [Rx + 123], Ry
    
    oa.instructionCode = I_MOV; opcodes["mov"] = oa;
    oa.instructionCode = I_ADD; opcodes["add"] = oa;
    oa.instructionCode = I_SUB; opcodes["sub"] = oa;
    oa.instructionCode = I_AND; opcodes["and"] = oa;
    oa.instructionCode = I_BOR; opcodes["or"] = oa;
    oa.instructionCode = I_XOR; opcodes["xor"] = oa;
    oa.instructionCode = I_SHL; opcodes["shl"] = oa;
    oa.instructionCode = I_SHR; opcodes["shr"] = oa;
    oa.instructionCode = I_ROL; opcodes["rol"] = oa;
    oa.instructionCode = I_ROR; opcodes["ror"] = oa;
    oa.instructionCode = I_CMP; opcodes["cmp"] = oa;
    oa.isSizeSpecifierAllowed = false;
    oa.instructionCode = I_MUL; opcodes["mul"] = oa;
    oa.instructionCode = I_DIV; opcodes["div"] = oa;

    oa.isPostfixEnabled = false;
    oa.addressingModesAllowed = {AM_NONE};
    oa.instructionCode = I_SEC; opcodes["sec"] = oa;
    oa.instructionCode = I_CLC; opcodes["clc"] = oa;
    oa.instructionCode = I_SEV; opcodes["sev"] = oa;
    oa.instructionCode = I_CLV; opcodes["clv"] = oa;
    oa.instructionCode = I_SEI; opcodes["sei"] = oa;
    oa.instructionCode = I_CLI; opcodes["cli"] = oa;
    oa.instructionCode = I_PHA; opcodes["pusha"] = oa;
    oa.instructionCode = I_POA; opcodes["popa"] = oa;
    oa.instructionCode = I_NOP; opcodes["nop"] = oa;
    oa.instructionCode = I_BRK; opcodes["brk"] = oa;
    oa.instructionCode = I_RTS; opcodes["rts"] = oa;
    oa.instructionCode = I_RTI; opcodes["rti"] = oa;
    oa.instructionCode = I_SLP; opcodes["slp"] = oa;

    oa.addressingModesAllowed = {AM_RELATIVE};
    oa.instructionCode = I_BRA; opcodes["bra"] = oa;
    oa.instructionCode = I_BEQ; opcodes["beq"] = oa;
    oa.instructionCode = I_BNE; opcodes["bne"] = oa;
    oa.instructionCode = I_BCC; opcodes["bcc"] = oa;
    oa.instructionCode = I_BCS; opcodes["bcs"] = oa;
    oa.instructionCode = I_BPL; opcodes["bpl"] = oa;
    oa.instructionCode = I_BMI; opcodes["bmi"] = oa;
    oa.instructionCode = I_BVC; opcodes["bvc"] = oa;
    oa.instructionCode = I_BVS; opcodes["bvs"] = oa;
    oa.instructionCode = I_BLT; opcodes["blt"] = oa;
    oa.instructionCode = I_BGT; opcodes["bgt"] = oa;
    oa.instructionCode = I_BLE; opcodes["ble"] = oa;
    oa.instructionCode = I_BGE; opcodes["bge"] = oa;
}
