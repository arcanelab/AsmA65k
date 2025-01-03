//
//  AsmA65k.h
//  AsmA65k - The assembler for the A65000 microprocessor
//
//  Created by Zoltán Majoros on 2013.05.16
//  Copyright (c) 2013 Zoltán Majoros. All rights reserved.
//
//  C++11
//

#include "Asm65k.h"
#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;

void logger(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void WriteFile(std::vector<Segment> *segments, const char *filename)
{
    std::string outfilename = filename;
    size_t lastindex = outfilename.find_last_of(".");
    outfilename = outfilename.substr(0, lastindex);
    outfilename += ".rsb"; // RetroSim binary
    std::ofstream outfile(outfilename, std::ofstream::binary);

    outfile.write("RSX0", 4);

    for (int i = 0; i < segments->size(); i++)
    {
        Segment actSegment = (*segments)[i];
        uint32_t address = actSegment.address;
        uint32_t length = actSegment.data.size();
        outfile.write((char *)&address, 4);
        outfile.write((char *)&length, 4);
        outfile.write((char *)&actSegment.data[0], actSegment.data.size());
    }

    outfile.close();

    printf("Output: '%s'\n", outfilename.c_str());
}

int main(int argc, const char *argv[])
{
    if (argc != 2)
    {
        printf("Please specify an argument.\n");
        return -1;
    }
    printf("AsmA65K alpha version. Copyright (c) 2013 Zoltán Majoros. (zoltan@arcanelab.com)\n\n");

    // load source file into 'buffer'
    ifstream fs(argv[1]);
    stringstream buffer;
    buffer << fs.rdbuf();

    if (buffer.str().empty())
    {
        printf("Could not load file '%s'\n", argv[1]);
        return -1;
    }

    // instantiate assembler and pass source code for processing
    AsmA65k asm65k;
    std::vector<Segment> *segments;
    try
    {
        segments = asm65k.Assemble(buffer);
    }
    catch (AsmError error)
    {
        logger("Assembly error in line %d: \"%s\"\n", error.lineNumber, error.errorMessage.c_str());
        logger("in line: %s\n", error.lineContent.c_str());
        return 1;
    }

    WriteFile(segments, argv[1]);

    // dump machine code
    for (int i = 0; i < segments->size(); i++)
    {
        Segment actSegment = (*segments)[i];

        printf("\n$%.8X:\n", actSegment.address);

        for (int j = 0; j < actSegment.data.size(); j++)
        {
            printf("%.2X ", actSegment.data[j]);
            //            printf("%.8X %.2X  ", actSegment.address+j, actSegment.data[j]);
            if (!((j + 1) % 16))
                printf("\n");
        }
    }
    printf("\n");

    return 0;
}
