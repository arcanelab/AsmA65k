//
//  main.cpp
//  Bassember65k
//
//  Created by Zoltán Majoros on 2013.05.16.
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//

#include "Asm65k.h"
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

int main(int argc, const char * argv[])
{
    if(argc != 2)
    {
        printf("Please specify an argument.\n");
        return -1;
    }
    printf("AsmA65K development version. Copyright (c) 2013 Zoltán Majoros. (zoltan@arcanelab.com)\n\n");
    
    // load source file into 'buffer'
    ifstream fs(argv[1]);
    stringstream buffer;
    buffer << fs.rdbuf();
    
    if(buffer.str().empty())
    {
        printf("Could not load file '%s'\n", argv[1]);
        return -1;
    }

    // instantiate assembler and pass source code for processing
    AsmA65k asm65k;
    std::vector<Segment> *segments;
    try { segments = asm65k.assemble(buffer); }
    catch(AsmError error)
    {
        log("Assembly error in line %d: \"%s\"\n", error.lineNumber, error.errorMessage.c_str());
        return 1;
    }

    // dump machine code
    /*
    for(int i = 0; i < segments->size(); i++)
    {
        Segment actSegment = (*segments)[i];
        for(int j = 0; j < actSegment.data.size(); j++)
            printf("%.8X %.2X\n", actSegment.address+j, actSegment.data[j]);
        
        cout << endl;
    }
    */
    return 0;
}
