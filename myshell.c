#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h> 
#include <fcntl.h>
#include <dirent.h>
#include <stdbool.h>

char error_message[30] = "An error has occurred\n";

void myPrint(char *msg)
{
    write(STDOUT_FILENO, msg, strlen(msg));
}

void execute_line (char* pinput, int saved_fd){
    /////// checking for long input ////////
    DIR *dir;

    ///// creating an array of jobs /////
    char** jobs = (char**)malloc(514*sizeof(char *));
    int i = 0; 
    char *job;
    job = strtok(pinput, ";");
    while (job != NULL){
        jobs[i] = job;
        job = strtok(NULL, ";");
        i++;
    }

    ///// executing each job ////////
    char** args;
    for (int j = 0; j < i; j++){ 
        if (strchr(jobs[j], '\n')){ 
            jobs[j][strcspn(jobs[j], "\n")] = 0;
        }
        int redir = -2;

        ///// split up arguments /////
        int idx;
        char* token; 
        char* filename;
        char* real_name;
        int advanced = 1;
        args = (char**)malloc(12*sizeof(char *));
        for (int i = 0; i < 12; i++) {
            args[i] = NULL;
        }
        int fd;

        token = strtok(jobs[j], " \t");
        idx = 0; 
        while (token != NULL){ 
            char* symb = strchr(token, '>');

            if (redir != -2){
                redir = -1; 
                break;
            }else if (symb){
                redir = idx; 
                char* splitter = ">";
                if (strchr(symb+1, '+')){
                    advanced = 2;
                    splitter = ">+";
                }
                if (strlen(token) == advanced){ // if just > or >+
                    filename = strtok(NULL, " \t");
                }else{ 
                    if (!strcmp(symb, splitter)){ // for the scenario ls > out                        
                        token[strlen(token)-advanced] = '\0';
                        args[idx] = token;
                        filename = strtok(NULL, " \t");
                    }else if (token[0] == '>'){ // for ls >out
                        args[idx] = strtok(token, splitter); 
                        filename = args[idx];
                        args[idx] = NULL;
                    }else{
                        args[idx] = strtok(token, splitter);
                        filename = strtok(NULL, " \t");
                        if (advanced == 2) filename++;
                    }
                }
                token = strtok(NULL, " \t");            
            }else{
                // normal stuff
                args[idx] = token;
                token = strtok(NULL, " \t");
                idx++;
            }
            
        }
        if (advanced == 2){
            if (filename == NULL){
                myPrint(error_message);
                free(args);
                continue;
            }
            real_name = filename;
            filename = "temp.txt";
        }
        int last_idx = idx != 0 ? idx-1 : 0;
        
        /////// redirecting! ////////
        if ((redir == -1)){
            myPrint(error_message);
            free(args);
            continue; 
        }
        if ((args[0] == NULL)){
            if (strchr(jobs[j], '>')){
                myPrint(error_message);
            }
            free(args);
            continue;
        }

        if (!strcmp(args[last_idx], "")){
            if (last_idx == 0) {
                free(args);
                continue;
            }
            else args[last_idx] = NULL;
        }
        
        if (redir != -2){
            if (!strcmp(args[0],"exit") || !strcmp(args[0],"cd") || !strcmp(args[0],"pwd")){
                myPrint(error_message);
                free(args);
                continue;
            }
            int output_fd;
            if (advanced == 2){
                output_fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, S_IRUSR|S_IWUSR);
            }else{
                output_fd = open(filename, O_CREAT|O_TRUNC|O_RDWR|O_EXCL, S_IRUSR|S_IWUSR);
            }

            fd = dup2(output_fd, STDOUT_FILENO);
            if (fd == -1){
                myPrint(error_message);
                free(args);
                continue; 
            }
        }

        ////// built in functions //////
        if (!strcmp(args[0], "exit")){
            if (args[1] != NULL){
                myPrint(error_message);
                free(args);
                continue; 
            }
            else exit(0);
        }
        else if (!strcmp(args[0], "pwd")){ 
            if (args[1] != NULL){
                myPrint(error_message);
                free(args);
                args = NULL;
                continue; 
            }else{
                char* cwd; 
                char buff[150]; 
                cwd = getcwd(buff, 150);
                strcat(cwd, "\n"); 
                myPrint(cwd);
                free(args);
                continue;
            }
        }else if (!strcmp(args[0], "cd")){
            if (args[1] != NULL){
                dir = opendir(args[1]);
                if (dir && (args[2] == NULL)){
                    chdir(args[1]);
                }else{
                    myPrint(error_message);
                    free(args);
                    closedir(dir);
                    continue;
                }
                closedir(dir);
            }else{
                chdir(getenv("HOME"));
            }
        }
        ////// create a new process ///////
        else{
            int status;
            int pid = fork();
            if (!pid){
                if(execvp(args[0], args) == -1){
                    myPrint(error_message);
                    exit(0);
                }             
                exit(0);
            }else{
                waitpid(pid, &status, 0);
            }
        }
        free(args); 
        args = NULL;

        if (advanced == 2){
            FILE* og_file;
            og_file = fopen(real_name, "r");
            if (og_file != NULL){ 
                char* one_line;
                char line[4096];
                while((one_line = fgets(line, 4096, og_file))){
                    myPrint(one_line);
                }
                remove(real_name);
                fclose(og_file);
            }
            rename(filename, real_name);
        }
        close(fd);
        dup2(saved_fd, STDOUT_FILENO);
        if (access("temp.txt", F_OK) == 0){
            remove("temp.txt");
            myPrint(error_message);
        }
    }
    free(jobs); 
    jobs = NULL;
    return;
}

int main(int argc, char *argv[]) 
{
    char* whole_line;
    char buf[4096];
    
    int saved_fd = dup(STDOUT_FILENO);
    
    if (argc > 1){ 
        ////// batchfile ////////
        char* name = argv[1];
        FILE* batch_file;

        batch_file = fopen(name, "r");

        if (batch_file == NULL){
            myPrint(error_message);
            exit(0);
        }
        while((whole_line = fgets(buf, 4096, batch_file))){
            if ((whole_line[strspn(whole_line, " \r\n\t")] == '\0')){
                continue;
            }
            else{
                myPrint(whole_line);
                
                if (strlen(whole_line) > 512){
                    myPrint(error_message);
                    whole_line = "";
                    continue;
                }
                else{
                    execute_line (whole_line, saved_fd);
                }
            }
        }
        fclose(batch_file);
    }else{
        ////// interactive ////////
        while (1) {
            myPrint("myshell> ");
            whole_line = fgets(buf, 514, stdin);
            if (!whole_line) {
                myPrint("\n");
                exit(0);
            }
            if (strlen(whole_line) > 512){
                myPrint(whole_line);
                myPrint(error_message);
                whole_line = "";
                continue;
            }
            execute_line (whole_line, saved_fd);
        }
    }
    close(saved_fd);
}
