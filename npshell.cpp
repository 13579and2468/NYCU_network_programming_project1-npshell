#include <iostream>
#include <vector>
#include <sstream>
#include <algorithm>
#include <unistd.h>
#include <csignal>
#include <sys/types.h> 
#include <sys/wait.h>
#include "Process.h"
using namespace std;

// split input into tokens
vector<string> split(string s){
  stringstream ss(s);
  string token;
  vector<string> v;
  while(ss>>token){
    v.push_back(token);
  }
  return v;
}

int myprintenv(string s){
  char *tmp = getenv(s.c_str());
  string env_var(tmp ? tmp : "");
  if (env_var.empty()) {
      return 0;
  }
  cout << env_var << endl;
  return 1;
}

int mysetenv(string var,string value){
  return setenv(var.c_str(),value.c_str(),1);
}

void init(){
  mysetenv("PATH","bin:.");
  //setting SIGCHILD tp ignore will give zombie to init process
  signal(SIGCHLD,SIG_IGN);
}

int main() {
  init();
  string input; 
  int line=0;
  while(true)
  {
    cout<<"% ";
    getline(cin,input);
    vector<string> tokens = split(input);
    if(tokens.size() == 0)continue;  // continue when empty line
    line++;

    // built-in commands
    if(tokens[0] == "printenv")
    {
      if(tokens.size()==1)continue; //no environment var
      myprintenv(tokens[1]);
      continue;

    }else if(tokens[0]=="setenv")
    {
      if(tokens.size()<=2)continue; //no enough argument
      mysetenv(tokens[1],tokens[2]);
      continue;

    }else if(tokens[0]=="exit")
    {
      exit(0);
    }

    //run commands
    auto it = tokens.begin();
    auto previt = it;
    vector<pid_t> childs;
    int prevpipe[2]={-1,-1};
    int thispipe[2]={-1,-1};
    while(true)
    {
      // all commands between pipe
      if((it = find(it,tokens.end(),"|"))!=tokens.end())
      {
        // convert partial string vector into char**
        char **argv = new char* [distance(previt,it)+1];
        int i=0;
        for(auto theit=previt;theit!=it;theit++)
        {
          argv[i++] = (char *)(*theit).c_str();
        }
        argv[i] = NULL;
        
        // prepare pipe
        close(prevpipe[0]);
        close(prevpipe[1]);
        prevpipe[0] = thispipe[0];
        prevpipe[1] = thispipe[1];
        if (pipe(thispipe)){
          fprintf (stderr, "Pipe failed.\n");
          return EXIT_FAILURE;
        }
        // fork child and execute 
        pid_t pid = fork();

        if(pid == 0)
        {
          Process proc((char *)(*previt).c_str(),(char **)argv);
          close(prevpipe[1]);
          close(thispipe[0]);
          proc.input = prevpipe[0];
          proc.output = thispipe[1];
          proc.run();
        }else if(pid > 0)
        {
          childs.push_back(pid);
          delete argv;
        }
        it++;
        previt = it;
      }else  // the last command of a line
      {
        char **argv = new char* [distance(previt,it)+1];
        int i=0;
        for(auto theit=previt;theit!=it;theit++)
        {
          argv[i++] = (char *)(*theit).c_str();
        }
        argv[i] = NULL;
        
        close(prevpipe[0]);
        close(prevpipe[1]);
        prevpipe[0] = thispipe[0];
        prevpipe[1] = thispipe[1];
        // fork child and execute 
        pid_t pid = fork();
        if(pid == 0)
        {
          
          Process proc((char *)(*previt).c_str(),(char **)argv);
          close(prevpipe[1]);
          proc.input = prevpipe[0];
          proc.run();
        }else if(pid > 0)
        {
          close(thispipe[0]);
          close(thispipe[1]);
          delete argv;
          childs.push_back(pid);
        }

        //wait all process which needed
        for (auto p : childs)
        {
          waitpid(p,NULL,0);
        }

        // next line
        break;
      }
    }
    
    

  }
  return 0;
}