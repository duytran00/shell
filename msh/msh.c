// The MIT License (MIT)
// 
// Copyright (c) 2024 Trevor Bakker 
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
                                // so we need to define what delimits our tokens.
                                // In this case  white space
                                // will separate the tokens on our command line

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 32    


int main( int argc, char * argv[] ){

  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );
  char error_message[30] = "An error has occurred\n";
  FILE *text;
  int batchMode = 0;

  //Detect files for batch
  if(argc == 2){
    batchMode = 1;
    text = fopen(argv[1],"r");
    if(text == NULL){
      write(STDERR_FILENO, error_message, strlen(error_message));
      exit(1);
    }
  }else if (argc > 2){
    // invoked with either no arguments or a single argument; anything else is an error."
    write(STDERR_FILENO, error_message, strlen(error_message));
    exit(1);
  }
  
  while(1){

    // Read the command from the commandi line.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user
    // inputs something.
    if(batchMode){
      //In either mode, if you hit the end-of-file marker (EOF), you should call exit(0) and exit gracefully.
      if (fgets (command_string, MAX_COMMAND_SIZE, text)==NULL){
        exit(0);
      }
    }else{
      //inteactive mode
      //it repeatedly prints a prompt msh> 
      printf ("msh> ");
      if( !fgets (command_string, MAX_COMMAND_SIZE, stdin) ){
        exit(0);
      }

    }
    
    //for repeated enter keys
    if(strcmp(command_string, "\n") == 0) {
      continue;
    }

    //handles test #15 excessive whitespace
    char *start = command_string;
    while (isspace(*start)) {
      start++;
    }
    if (strlen(start) == 0) {
      continue;
    }

    /* Parse input */
    char *token[MAX_NUM_ARGUMENTS];

    int token_count = 0;  

    // Pointer to point to the token
    // parsed by strsep
    char *argument_pointer;                                
    char *working_string  = strdup( start ); 
    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end

    char *head_ptr = working_string;
    int redirect = 0;
    char *outputFile = NULL;
    
    // Tokenize the input with whitespace used as the delimiter
    while ( ( (argument_pointer = strsep(&working_string, WHITESPACE ) ) != NULL) &&
              (token_count<MAX_NUM_ARGUMENTS))
    {
      token[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      } else {// To mangage redirections
        if (strcmp(token[token_count], ">") == 0) {

          //to guard no input before redirect
          if (token_count == 0 || redirect) {
            write(STDERR_FILENO, error_message, strlen(error_message));
            redirect = -1;
            break;
          }
          //To guard no input after redirect
          argument_pointer = strsep(&working_string, WHITESPACE);
          if (argument_pointer == NULL || strlen(argument_pointer) == 0) {
              write(STDERR_FILENO, error_message, strlen(error_message));
              redirect = -1;
              break;
          }
          // Set output File and only one token after >
          outputFile = strndup(argument_pointer, MAX_COMMAND_SIZE);
          // Check tokens after output file
          argument_pointer = strsep(&working_string, WHITESPACE);
          if (argument_pointer != NULL && strlen(argument_pointer) > 0) {
              write(STDERR_FILENO, error_message, strlen(error_message));
              redirect = -1;
              break;
          }

          redirect = 1;
          free(token[token_count]);
          token[token_count] = NULL;
          break;
        }
        token_count++;
      }
    }

    //Clean memeory when unwanted redirect syntax for next loop
    if (redirect == -1) {
      free(head_ptr);
      for (int i = 0; i < token_count; i++) {
        free(token[i]);
      }
      free(outputFile);
      continue;
    }

    free( head_ptr );
    int check = 0;
    char *pathDir[] = {"/bin/","/usr/bin/","/usr/local/bin/","./"};
    char path[MAX_COMMAND_SIZE];

    //built in commands
    //When the user types exit, your shell should simply call the exit
    if(strcmp(token[0],"exit") == 0 || strcmp(token[0],"quit") == 0){
      if (token[1] != NULL) {
      write(STDERR_FILENO, error_message, strlen(error_message));
      } else {
          exit(0);
      }
    }else if (strcmp(token[0],"cd") == 0) { 
      //cd always take one argument
      if (token[1] == NULL || token[2] != NULL) {
        write(STDERR_FILENO, error_message, strlen(error_message));
      } else if (chdir(token[1]) != 0) {
        //if chdir fails, that is also an error.
        write(STDERR_FILENO, error_message, strlen(error_message));
      }
    }else{
      
      //search four directories: /bin /usr/bin /usr/local/bin and ./
      for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "%s%s", pathDir[i], token[0]);
        if (access(path, X_OK) == 0 ) {
            check = 1;
            break;
        }
      }

      //checks access
      if (!check) {
        write(STDERR_FILENO, error_message, strlen(error_message));
      }else {//executes commands
        pid_t pid = fork();
        if (pid == 0) {
          if (redirect) {
            
            // Setup File
            int fd = open(outputFile, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd == -1) {
              write(STDERR_FILENO, error_message, strlen(error_message));
              redirect = -1;
              free(outputFile);
            }
            
            // duplicates the standard output file
            dup2(fd, STDOUT_FILENO);
            //standard error output of the program should be rerouted to the file output
            dup2(fd, STDERR_FILENO);
            close(fd);
          }
          execv(path, token);
          write(STDERR_FILENO, error_message, strlen(error_message));
          exit(1);
        }else if (pid > 0){
          int status;
          waitpid(pid, &status, 0);
        }else{
          write(STDERR_FILENO, error_message, strlen(error_message));
        }
      } 
    }

    //Clean Memory
    for (int i = 0; i < token_count; i++) {
      if (token[i] != NULL) {
        free(token[i]);
      }
    }
    free(outputFile);
  }

  free(command_string);
  return 0;
}