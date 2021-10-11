#include "Process.h"
#include <unistd.h>

Process::Process(char* executable,char** argv)
{
    this->executable = executable;
    this->argv = argv;
}

int Process::run()
{
    return execvp(executable,argv);
}