#ifndef Process_H
#define Process_H

class Process
{
    public:
        Process(char*,char**);
        char* executable;
        char** argv;
        int input;
        int output;
        int err;
        int run();
};

#endif