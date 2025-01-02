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

#include <iostream>
#include <vector>

class Segment
{
public:
    void AddByte(uint8_t byteToBeAdded)
    {
        data.push_back(byteToBeAdded);
    }

    void AddWord(uint16_t wordToBeAdded)
    {
        data.push_back(wordToBeAdded & 0xff);
        data.push_back((wordToBeAdded & 0xff00) >> 8);
    }

    void AddDword(uint32_t dwordToBeAdded)
    {
        data.push_back(dwordToBeAdded & 0xff);
        data.push_back((dwordToBeAdded & 0xff00) >> 8);
        data.push_back((dwordToBeAdded & 0xff0000) >> 16);
        data.push_back((dwordToBeAdded & 0xff000000) >> 24);
    }

    void WriteByte(uint32_t address, uint8_t value)
    {
        data[address - this->address] = value;
    }

    void WriteWord(uint32_t address, uint16_t value)
    {
        uint32_t index = address - this->address;
        data[index++] = (uint8_t)(value & 0xff);
        data[index] = (uint8_t)((value & 0xff00) >> 8);
    }

    void WriteDword(uint32_t address, uint32_t value)
    {
        uint32_t index = address - this->address;
        data[index++] = (uint8_t)(value & 0xff);
        data[index++] = (uint8_t)((value & 0xff00) >> 8);
        data[index++] = (uint8_t)((value & 0xff0000) >> 16);
        data[index] = (uint8_t)((value & 0xff000000) >> 24);
    }

    std::vector<uint8_t> data;
    uint32_t address;
};
