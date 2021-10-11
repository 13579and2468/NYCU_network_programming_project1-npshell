#include "Process.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <exception>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

Process::Process(char* executable,char** argv)
{
    this->executable = executable;
    this->argv = argv;
    this->input = -1;  // -1 means no change
    this->output = -1;
    this->err = -1;
}

int Process::run()
{
    if(this->input!=-1)
    {
        dup2(this->input,STDIN_FILENO);
        close(this->input);
    }
    if(this->output!=-1)
    {
        dup2(this->output,STDOUT_FILENO);
        close(this->output);
    }
    if(this->err!=-1)
    {
        dup2(this->err,STDERR_FILENO);
        close(this->err);
    }

    for (int i=0;argv[i]!=NULL;i++)
    {
        if(strcmp(argv[i],">")==0)
        {
            std::cout<<"file\n";
            int fd;
            try{
                fd = open( argv[i+1],O_CREAT|O_RDWR|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO );
            }
            catch(...){
                std::cerr<< "open file failed\n";
                exit(0);
            }
            dup2(fd,STDOUT_FILENO);
            close(fd);
            argv[i]=NULL;
            break;
        }
    }
    int r = 0;
    if((r = execvp(executable,argv))==-1)
    {
        std::cerr<<"Unknown command: ["<<executable<<"].\n";
        exit(0);
    }
    return r;
}