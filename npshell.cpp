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

struct Numberpipe{
  int afterline;
  int lastpipe[2];
  vector<int> proc_pids; // if no number pipe continue need to wait all of them

  operator int() const{
    return afterline;
  }
};

int fork_unitil_success(){
  int r;
  while( (r=fork())==-1 )usleep(1000);
  return r;
}

int main() {
  init();
  string input;
  vector<Numberpipe> numberpipes;
  while(true)
  {
    cout<<"% ";
    getline(cin,input);
    vector<string> tokens = split(input);
    if(tokens.size() == 0)continue;  // continue when empty line
    for (Numberpipe& p: numberpipes)p.afterline--;  // decrease all pipe line 

    // prepare if the line is piped
    Numberpipe pipe_to_this_line;
    bool linepiped = false;
    auto pipe_it = numberpipes.begin();
    if((pipe_it = find(numberpipes.begin(),numberpipes.end(),0))!=numberpipes.end())
    {
      linepiped = true;
      pipe_to_this_line = *pipe_it;
      numberpipes.erase(pipe_it);
    }
    
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
    //if numberpiped pipe to first command
    if(linepiped)
    {
      close(pipe_to_this_line.lastpipe[1]);
      thispipe[0]=pipe_to_this_line.lastpipe[0];
      childs = pipe_to_this_line.proc_pids;
    }

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

        auto dup_pipe = numberpipes.begin();
        bool new_pipe = false;
        // number pipe
        if((*--it)[0] == '|' || (*it)[0] == '!')
        {
          argv[i-1]=NULL;
          Numberpipe np;
          np.afterline = atoi( (*it).substr(1).c_str() );
          
          // merge pipe
          if((dup_pipe = find(numberpipes.begin(),numberpipes.end(),np.afterline))!=numberpipes.end())
          {
            thispipe[1] = dup_pipe->lastpipe[1];
            dup_pipe->proc_pids.insert(dup_pipe->proc_pids.end(),childs.begin(),childs.end());
          }else//new pipe
          {
            new_pipe = true;
            pipe(thispipe);
            np.lastpipe[0] = thispipe[0];
            np.lastpipe[1] = thispipe[1];
          }
          numberpipes.push_back(np);
        }
        // fork child and execute 
        pid_t pid = fork();
        if(pid == 0)
        {
          Process proc((char *)(*previt).c_str(),(char **)argv);
          if((*it)[0] == '|' || (*it)[0] == '!')proc.output = thispipe[1];
          if((*it)[0] == '!')proc.err = thispipe[1];
          
          close(prevpipe[1]);
          proc.input = prevpipe[0];
          proc.run();
        }else if(pid > 0)
        {
          close(prevpipe[0]);
          close(prevpipe[1]);
          delete argv;
          childs.push_back(pid);

          //if number pipe, no wait for processes, this code add wait list used when pipe to no number pipe
          if((*it)[0] == '|' || (*it)[0] == '!')
          {
            if(new_pipe)
            {
              numberpipes.back().proc_pids = childs;
            }else
            {
              dup_pipe->proc_pids.push_back(pid);
            }
            childs.clear();
          }
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