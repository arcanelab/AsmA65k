//
//  Segment.h
//  Bassember65k
//
//  Created by Zoltán Majoros on 2013.05.16.
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//

#ifndef __Bassember65k__Segment__
#define __Bassember65k__Segment__

#include "types.h"
#include <iostream>
#include <vector>

class Segment
{
public:
    void addByte(byte byteToBeAdded)
    {
        data.push_back(byteToBeAdded);
    }
    
    void addWord(word wordToBeAdded)
    {
        data.push_back(wordToBeAdded & 0xff);
        data.push_back((wordToBeAdded & 0xff00) >> 8);
    }

    void addDword(dword dwordToBeAdded)
    {
        data.push_back(dwordToBeAdded & 0xff);
        data.push_back((dwordToBeAdded & 0xff00) >> 8);
        data.push_back((dwordToBeAdded & 0xff0000) >> 16);
        data.push_back((dwordToBeAdded & 0xff000000) >> 24);
    }
    
    std::vector<byte> data;
    dword address;
};

#endif /* defined(__Bassember65k__Segment__) */
