#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<sys/wait.h>
#include <ctype.h>

#include<assert.h>


char* read_input(FILE* file);
char** extract_args(char* line);
void print_error(int flag);
int shell_execute(char** shell_args, FILE* file);
int num_of_built_in();
int shell_exit(char** shell_args);
int shell_cd(char** shell_args);
int shell_path(char** shell_args);
int shell_external(char** shell_args, FILE* file);
void command_mode(void);
void batch_mode(char** argv);
void debug(char** args);
int find_symbol(char* line);
char** check_redir_symbol(char* line);
FILE * check_redir_file(char** parsed_args);
char* trim_string(char* src);
char* process_if(char* src);
char* process_then(char* src);
char** parse_if(char* line);
char* if_mode(char* line);
char* if_check_operator(char* line); 
int check_contain_if(char* line);
int count_str(const char* src, const char* target);
char* extract_final_exec(char* line);
int check_tail(char* line); 
int is_in_if = 0; 

const size_t BUFFER_SIZE = 128;
const char error_message[30] = "An error has occurred\n";
char* built_in[] = {
        "exit",
        "cd",
        "path"
};
char* curr_path[64] = {"/bin"}; // default size 64, include "/bin" at iniitialization
int (*builtin_func[]) (char **) = {
        &shell_exit,
        &shell_cd,
        &shell_path
};

char* read_input(FILE *file){
    char* buffer; size_t size = BUFFER_SIZE;
    buffer = (char* )malloc(BUFFER_SIZE * sizeof (char));
    if(getline(&buffer, &size, file) == -1){
        if(feof(file)) exit(0);
        else(print_error(1)); //exit here since encounter EOF before reading finished
    }
    int index = 0;
    while(buffer[index] != '\0' && buffer[index] != '\n'){index++;}
    buffer[index] = '\0';
    return buffer;
}
char** extract_args(char* line){
    int index = 0;
    char* token;
    char* temp = strdup(line);
    char** tokens = malloc(BUFFER_SIZE * sizeof(char *));
    for(int i = 0; i < BUFFER_SIZE; i++){
        tokens[i]= (char* )malloc(BUFFER_SIZE * sizeof (char));
    }
    if(!tokens) print_error(1); // exit here since memory has not been allocated
    while((token = strsep(&temp, " "))){
        strcpy(tokens[index++], token);
    }
    tokens[index] = NULL;
    return tokens;
}
void print_error(int flag){
    write(STDERR_FILENO, error_message, strlen(error_message));
    if(flag) exit(1);
}
int shell_execute(char** shell_args, FILE* file){
    int i = 0;
    if(shell_args[i] == NULL) return 1;
    for(int i = 0; i < num_of_built_in(); i++){
        if(strcmp(shell_args[0], built_in[i]) == 0){
            return (*builtin_func[i])(shell_args);
        }
    }
    return shell_external(shell_args, file);
}
int shell_exit(char** shell_args){
    int index = 0;
    if(shell_args[1] != NULL) print_error(0);
    else exit(0);
}
int shell_cd(char** shell_args){
    char* dir = shell_args[1];
    if(chdir(dir) != 0) print_error(0);
    return 1;
}
int shell_path(char** shell_args){
    for(int i = 0; i < BUFFER_SIZE && curr_path[i] != NULL; i++){
        curr_path[i] = NULL;
    }
    for(int i = 0; i < BUFFER_SIZE && shell_args[i + 1] != NULL; i++){
        curr_path[i] = shell_args[i + 1];

    }
    return 1;
}

int num_of_built_in(){
    return sizeof(built_in) / sizeof (char* );
}

int shell_external(char** shell_args, FILE* file){
    int ch_pid, status;
    ch_pid = fork();
    int fd = -1;
    if(file != NULL) fileno(file);
    if(ch_pid == 0){
        if(file != NULL){
            dup2(fd, fileno(stdout));
            close(fd);
        }
        for(int i = 0; i < BUFFER_SIZE && curr_path[i] != NULL; i++){
            char* dir_path = strdup(curr_path[i]);
            strcat(dir_path, "/");
            strcat(dir_path, shell_args[0]);
            if(access(dir_path, X_OK) == 0){
                execv(dir_path, shell_args);// execv will not execute anything below when success
            }
        }
        exit(1); //command not execute successfully, exit with 1
    }
    else{
        if(file != NULL)close(fd);
        waitpid(ch_pid, &status, WUNTRACED);
        status = WEXITSTATUS(status);
        if(status == 1) print_error(0);
        return status; // get the return value from child
    }
}
void command_mode(void){
    char* curr_line;
    char** shell_args;
    int status;
    do{
        printf("> ");
        curr_line = read_input(stdin);
        char** redir_args;
        FILE* file;
        if(find_symbol(curr_line)){
            redir_args = check_redir_symbol(curr_line); //separate line by ">", only one allowed
            file = check_redir_file(redir_args); //check and extract the correct file name
        }
        shell_args = extract_args(curr_line);
        status = shell_execute(shell_args, file);
        free(curr_line);
        free(shell_args);
    }while(status != -1);
}
void batch_mode(char** argv){
    FILE* fp;
    int status = 1;
    fp = fopen(argv[1], "r");
    if(fp == NULL){
        print_error(1);
        return;
    }
    while(status != -1){
        char* line = read_input(fp);
        char* new_line = trim_string(line);
        if(new_line == NULL) continue;
        char* temp = strdup(new_line);
        if(check_contain_if(temp) == 1) {
            if(count_str(temp, "if") != count_str(temp, "fi")){
                print_error(0); 
                continue; // invalid/mismatch number of keyword
            }
            if(check_tail(new_line) == 0){
                print_error(0); 
                continue;
            }
            if(if_check_operator(new_line) == NULL){
                print_error(0); 
                continue;
            }
            while (check_contain_if(temp) == 1) {// 1. 已经无法再提取嵌套(没有多的if和then了)
                                                // 2. if开头,但字符串不合法
                                                //注意: 满足条件必须是if和then都要有
                temp = if_mode(temp);
                new_line = temp;
                if(new_line != NULL && check_contain_if(new_line) == 0){//已经没有
                    new_line = extract_final_exec(new_line);
                }
            }//temp有两种情况. 1. 之前返回的temp只剩then(...) 2.
            // 【大致不考虑】if和then都有, 但是if部分中间的字符串如operator无效
        }
        new_line = trim_string(new_line);
        if(new_line == NULL) continue;
        fflush(fp);// extremely confused, do some search online and add this line 非常神奇
        char** redir_args;
        FILE* file;
        if(find_symbol(new_line)){
            redir_args = check_redir_symbol(new_line); //separate line by ">", only one allowed
            file = check_redir_file(redir_args); //check and extract the correct file name
            if(file == NULL){
                print_error(0);
                continue; // redirection contains invalid argument or file cannot be opened
            }
            new_line = check_redir_symbol(new_line)[0];
            new_line = trim_string(new_line);
            if(new_line == NULL){
                print_error(0); 
                continue;
            }
        }//------------> This if block check for redirect
        char** shell_args = extract_args(new_line);
        status = shell_execute(shell_args, file);
        free(new_line);
        free(shell_args);
    }
    fclose(fp);
}
int find_symbol(char* line){
    int index = 0;
    while(line[index]){
        if(line[index] == '>') return 1;
        index++;
    }
    return 0;
}
char** check_redir_symbol(char* line){
    //fprintf(stderr, "%s\n", line);
    char* extract; char* temp = strdup(line);
    //fprintf(stderr, "%s\n", temp);
    char** parsed_args = malloc(BUFFER_SIZE * sizeof (char*));
    int i = 0;
    while(i < 2){
        parsed_args[i++] = (char*)malloc(BUFFER_SIZE * sizeof (char ));
    }
    int index = 0;
    while((extract = strsep(&temp, ">"))){
        if(index == 2) return NULL; // more than 2 ">" symbols
        strcpy(parsed_args[index++], extract);
    }
    return parsed_args;
}
FILE* check_redir_file(char** parsed_args){
    FILE file;
    if(parsed_args == NULL) return NULL;
    char* operand = strdup(parsed_args[1]); // process right operand
    char* temp; char* str_file = (char*) malloc(sizeof (char));
    int index = 0;
    while((temp = strsep(&operand, " \t\r\n\a"))){
        if(index++ > 1) return NULL;  // has multiple files on the right side of opertaor
        str_file = temp;
    }
    return fopen(str_file, "w");
}

//This function return a char ptr to the original string
char* trim_string(char* src){
    if(src == NULL) return NULL; 
    while(isspace((unsigned char)*src)) src++;
    if(*src == '\0') return NULL;
    char* dest = (char*) malloc(BUFFER_SIZE * sizeof (char ));
    char* temp = dest;
    char* end = src + strlen(src) - 1;
    while(end > src && isspace((unsigned char)*end)) end--;
    for(char* c = src; c <= end; c++){
        *temp++ = *src++;
    }
    return dest;
}
char* process_if(char* src){
    char* first_if = strstr(src, "if");
    char* first_then = strstr(src, "then"); //Todo: if 和 then 之间 不能有其他关键词
    if(first_then == NULL) return NULL; // return immediately if not contain "if"
    if(strlen(first_if) < strlen(first_then)) return NULL; // "then" appears before "if"
    char* end_of_first_if = first_if + strlen(first_if) - 1;
    end_of_first_if -= strlen(first_then);
    char* condition_args = (char*) malloc(BUFFER_SIZE * sizeof (char));
    char* temp1 = condition_args;
    for(char* c = src + 2; c <= end_of_first_if; c++){          // remove the first two characters "if"
        *temp1++ = *c;
    }
    condition_args = trim_string(condition_args);
    return condition_args;
}
char* process_then(char* src){
    char* first_then = strstr(src, "then");
    if(first_then == NULL) return NULL; // return immediately if not contain "then"
    char* execute_args = (char*) malloc(BUFFER_SIZE * sizeof (char));// remove the first "then"
    char* temp2 = execute_args;
    for(char* c = first_then + 4; *c != '\0'; c++){
        *temp2++ = *c;
    }
    execute_args = trim_string(execute_args);
    return execute_args;
}
char** parse_if(char* line){
    char** if_args = malloc(BUFFER_SIZE * sizeof (char*));
    char** args_temps = if_args; int index = 0;
    while(index < BUFFER_SIZE){
        args_temps[index] = (char*)malloc(BUFFER_SIZE * sizeof (char ));
        index++;
    }
    index = 0;
    char* firstOperator;
    char op1[BUFFER_SIZE], op2[BUFFER_SIZE], op3[BUFFER_SIZE];
    memset(op1, '\0', sizeof op1);
    memset(op2, '\0', sizeof op2);
    memset(op3, '\0', sizeof op3);
    if((firstOperator = strstr(line, "==")) != NULL ||
    (firstOperator = strstr(line, "!=")) != NULL){
        strncpy(op1, line, firstOperator - line);
        args_temps[0] = trim_string(op1);  // copy left operand to index 0;
        strncpy(op2, firstOperator, 2);
        args_temps[1] = trim_string(op2); // copy operand to index 1
        firstOperator += 2;
        strcpy(op3, firstOperator);
        args_temps[2] = trim_string(op3); // copy right operand to index 2
        return args_temps;
    }
    return NULL; // no operator find
}
//检查是否含有if, then 两个关键词
int check_contain_if(char* line){
    if(line == NULL) return 0;
    char* cond_line = strstr(line, "if"); //Todo: Should check the first if two characters equals "if
    char* exec_line = strstr(line, "then");
    if(cond_line == NULL || exec_line == NULL) return 0;
    return 1;
}
// 只会return有效的then部分, 对if进行parse, 如果为NUll则说明此字符串无效
char* if_mode(char* line){
    char* cond_line = process_if(line);
    char* exec_line = process_then(line);
    char** parsed_cond = parse_if(cond_line); // argument stored at index 0
    int expected = atoi(parsed_cond[2]); // expected value stored at index 1
    int status = 0;
    if(strcmp(parsed_cond[1], "==") == 0){
        char** cond_args = extract_args(parsed_cond[0]);
        status = shell_execute(cond_args, NULL);
        if(status == expected) {
            return exec_line;
        }
    }
    else{
        char** cond_args = extract_args(parsed_cond[0]);
        is_in_if = 1; 
        status = shell_execute(cond_args, NULL);
        if(status != expected){
            return exec_line;
        }
    }
    return NULL; 
}
char* if_check_operator(char* line){
    char* cond_line = process_if(line);
    char** parsed_cond = parse_if(cond_line); 
    if(parsed_cond == NULL) return NULL; 
    return line; 
}   
int count_str(const char* src, const char* target){
    int count = 0;
    const char* temp = src;
    while(temp = strstr(temp, target)){
        count++; temp++;
    }
    return count;
}
char* extract_final_exec(char* line){
    const char* temp = line;
    char* fi = strstr(temp, "fi");
    char* str = malloc(BUFFER_SIZE * sizeof (char ));
    strncpy(str, temp, fi - temp);
    return str;
}
int check_tail(char* line){
    const char* temp = strstr(line, "fi"); 
    while(*temp != '\0'){
        if(*temp != 'f' && *temp != 'i' && *temp != ' ') return 0; 
        temp++; 
    }
    return 1; 
}
int main(int argc,  char** argv){
    if(argc == 1) command_mode();
    else if(argc == 2)(batch_mode(argv));
    else print_error(1);
}