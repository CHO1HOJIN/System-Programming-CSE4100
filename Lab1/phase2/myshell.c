/* $begin shellmain */
#include "csapp.h"
#include<errno.h>
#define MAXARGS   128
#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* Function prototypes */
void eval(char* cmdline);
int parseline(char* buf, char** argv);
int builtin_command(char** argv, int bg);
char pwd[MAXLINE * 5] = "\0";
char* replace_bar_and_with_space_bar_and_space(char* buf);
int count_pipe(char* buf);
void gethome(char** argv);
void parse_with_pipe(char** argv, char*** newargv);
void no_pipe_command(pid_t pid, int status, int bg, char* cmdline, char** argv);
void pipe_command(pid_t pid, int status, int bg, char* cmdline, char** _argv, char*** argv, int pipe_count);
void wait_with_sleep(pid_t pid);
void insert_job(pid_t pid, int state, char** argv);
/*signal handler*/
void sigint_handler(int sig);
void sigtstp_handler(int sig);
void sigchld_handler(int sig);

volatile int pipe_flag = -1;
volatile sig_atomic_t ctrl_c = -1;
typedef struct jobs {
    pid_t pid;
    int jobid;
    int state;
    char cmd[MAXLINE];
} Jobs;

Jobs fg_job[MAXARGS];
Jobs bg_job[MAXARGS];
int ps_cnt = -1;
int bg_cnt = 1;

//state //-1 = terminated, 0 = stopped, 1 = running 2 = foreground
int main()
{
    Signal(SIGINT, sigint_handler);
    Signal(SIGTSTP, sigtstp_handler);
    Signal(SIGCHLD, sigchld_handler);
    for (int i = 0; i < MAXARGS; i++) {
        fg_job[i].state = -1;
        bg_job[i].state = -1;
    }
    char cmdline[MAXLINE]; /* Command line */

    do {
        /// Debuging
        // for(int i = 0; i <= ps_cnt; i++){
        //     printf("[%d] pid: %d, state: %d %s\n",bg_job[i].jobid, bg_job[i].pid, bg_job[i].state, bg_job[i].cmd);
        // }
        /*Shell Prompt: print the prompt*/
        printf("CSE4100-SP-P2> ");

        /*Reading: Read the command from standard input*/
        fgets(cmdline, MAXLINE, stdin);

        if (feof(stdin)) exit(0);
        eval(cmdline);
    } while (1);

    return 0;
}
/* $end shellmain */

/* $begin eval */
/* eval - Evaluate a command line */
void eval(char* cmdline)
{
    int pipe_count = count_pipe(cmdline); /*count the pipe('|')*/
    char* argv[MAXARGS]; /* Argument list execve() */
    char*** newargv = (char***)malloc(sizeof(char**) * (pipe_count)+1); /* pipe 단위로 자른 argument list*/
    char buf[MAXLINE * 3];   /* Holds modified command line */
    int bg;              /* Should the job run in bg or fg? */
    pid_t pid;           /* Process id */
    int status;
    strcpy(buf, replace_bar_and_with_space_bar_and_space(cmdline));
    bg = parseline(buf, argv);
    if (argv[0] == NULL) return;   /* Ignore empty lines */

    parse_with_pipe(argv, newargv); //pipe 단위로 잘라 명령어 저장

    if (pipe_count == 0) { //no pipe
        no_pipe_command(pid, status, bg, cmdline, newargv[0]);
        return;
    }
    else {
        pipe_flag = pipe_count;
        pipe_command(pid, status, bg, cmdline, argv, newargv, pipe_count);
        return;
    }
}
/* If first arg is a builtin command, run it and return true */
int builtin_command(char** argv, int bg)
{
    if(bg){
        if(!strcmp(argv[0],"cd") || !strcmp(argv[0],"&") || !strcmp(argv[0],"jobs") || !strcmp(argv[0], "bg") || !strcmp(argv[0], "fg") || !strcmp(argv[0], "kill") || !strcmp(argv[0], "exit")){
            pid_t pid;
            int status;
            if((pid = Fork()) == 0){
                exit(0);
            }
            Waitpid(pid,&status,WUNTRACED);
            printf("[%d] %d\n",bg_cnt,pid);
            printf("[%d] Done           %s\n",bg_cnt, argv[0]);
            return 1;
        }
        return 0;
    }

    char lastpwd[MAXLINE * 5] = "\0";
    strcpy(lastpwd, pwd);
    getcwd(pwd, MAXLINE * 5);
    
    if (!strcmp(argv[0], "exit")) {
        exit(0);
    } /* exit command */
    
    if (!strcmp(argv[0], "&")) return 1;   /* Ignore singleton & */
    
    if (!strcmp(argv[0], "cd")) {
        char* home_dir = getenv("HOME");
        char from_home[MAXLINE];
        sprintf(from_home,"%s/%s",home_dir,argv[1]);
        
        if (argv[1] == NULL){
            char *h;
            h = getenv("HOME");
            chdir(h);
        }
        else if (!strcmp(argv[1], "&")){
            return 1;
        }
        else if (!strcmp(argv[1], "-")) { //last directory
            if (!strcmp(lastpwd, "\0")) {
                printf("bash: cd: OLDPWD not set\n");
                strcpy(pwd, lastpwd);
            }
            else {
                printf("%s\n", lastpwd);
                chdir(lastpwd);
            }
            return 1;
        }
        else if (argv[1]) {
            if (chdir(argv[1])) {
                if(chdir(from_home)){
                    DIR* dir_info = Opendir(".");
                    struct dirent* entry;
                    while (entry = Readdir(dir_info)) {
                        if (!strcmp(argv[1], entry->d_name)) {
                            printf("bash: cd: %s: Not a directory\n", argv[1]);
                            strcpy(pwd, lastpwd);
                            return 1;
                        }
                    }
                    char* home_dir = getenv("HOME");
                    printf("bash: cd: %s: No such file or directory\n", argv[1]);
                    strcpy(pwd, lastpwd);
                }
            }
        }
        return 1;
    }
    sigset_t mask, prev;
    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, &prev);

    if (!strcmp(argv[0], "jobs")){
        for (int i = 0; i <= MAXARGS; i++){
            if(bg_job[i].pid > 0)
                switch (bg_job[i].state)
                {
                case 0: //stopped
                    printf("[%d] Suspended         %s", bg_job[i].jobid, bg_job[i].cmd);
                    break;
                case 1: //running
                    printf("[%d] Running         %s", bg_job[i].jobid, bg_job[i].cmd);
                    break;
                default:
                    break;
                }
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }



    if (!strcmp(argv[0], "fg")) {
        sigset_t sigint_mask;
        Sigemptyset(&sigint_mask);
        Sigaddset(&sigint_mask, SIGINT);
        Sigprocmask(SIG_UNBLOCK, &sigint_mask, &prev);
        int index = -1;
        
        if (argv[1] == NULL) {
            int i;
            for (i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid > 0) break;
            }
            index = bg_job[i].jobid;
            //bg만 입력되었을 때엔 가장 앞 jobid에 적용
        }
        else if (argv[1][0] == '%') {
            int key = atoi(argv[1] + 1);
            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].jobid == key && (bg_job[i].state == 1 || bg_job[i].state == 0) ) {
                    index = i; break;
                }
            }
        }
        else if (isdigit(argv[1])) {
            pid_t pid = atoi(argv[1]);
            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid == pid ) {
                    index = i;
                    break;
                }
            }
        }
        if (index >= 0) {
            int status;
            pid_t pid;
            switch (bg_job[index].state)
            {
            case 1: case 0:
                printf("%s\n", bg_job[index].cmd);

                bg_job[index].state = 2;
                Kill(bg_job[index].pid, SIGCONT);
                
                Waitpid(pid, &status, WUNTRACED);
                
                if(!status){
                    for (int i = 0; i < MAXARGS; i++) {
                        if (bg_job[i].pid == pid) {
                            if(bg_job[i].state != 0) bg_job[i].state = -1;
                            bg_job[i].pid = 0;
                            break;
                        }
                    }
                }

                break;

            default:
                printf("bash: fg: %%%d: no such job\n", atoi(argv[1] + 1));
                break;
            }

        }
        else{
           printf("bash: bg: no such job\n", index);
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;

    }
    if (!strcmp(argv[0], "bg")) {
        int index = -1;

        if (argv[1] == NULL) {
            int i;
            for (i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid > 0) {
                    index = i;
                    break;
                }
            }
        
        }
        else if (argv[1][0] == '%') {
            int key = atoi(argv[1] + 1);
            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].jobid == key && (bg_job[i].state == 1 || bg_job[i].state == 0)) {
                    index = i; break;
                }
            }
        }//%제외
        else if (isdigit(argv[1])) {
            pid_t pid = atoi(argv[1]);
            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid == pid) {
                    index = i;
                    break;
                }
            }
        }
        if (index >= 0) {
            switch (bg_job[index].state)
            {
            case -1: //terminated
                printf("bash: bg: %%%d: no such job\n", atoi(argv[1] + 1));
                break;
            case 1: //running
                if (index <= ps_cnt) printf("bash: bg: job %d already in background\n", bg_job[index].jobid);
                else printf("bash: bg: %%%d: no such job\n",bg_job[index].jobid);
                break;
            default: //stopped
                printf("[%d] %s\n", bg_job[index].jobid, bg_job[index].cmd);
                Kill(bg_job[index].pid, SIGCONT); // kill process
                bg_job[index].state = 1;

            }

        }
        else{
           printf("bash: bg: no such job\n", index);
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }

    if (!strcmp(argv[0], "kill")) {
        int index = -1;

        if (argv[1] == NULL) {
            printf("kill: usage: kill[-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill - l[sigspec]\n");
            return 1;
            //bg만 입력되었을 때엔 가장 앞 jobid에 적용
        }
        else if (argv[1][0] == '%'){
            index = atoi(argv[1] + 1); //%제외
            for(int i = 0; i < MAXARGS; i++){
                if(bg_job[i].jobid == index && (bg_job[i].state == 1 || bg_job[i].state == 0)){
                    index = i;
                    break;
                }
            }
        }
        else if (isdigit(argv[1])) {
            pid_t pid = atoi(argv[1]);
            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid == pid) {
                    index = i;
                    break;
                }
            }
        }
        if (index >= 0) {
            switch (bg_job[index].state)
            {
            case -1:
                printf("bash: kill: %%%d: no such job\n", atoi(argv[1] + 1));
                break;
            default:
                Kill(bg_job[index].pid, SIGKILL);
                bg_job[index].state = -1;
                bg_job[index].pid = 0;
                printf("[%d] Terminated          %s \n", bg_job[index].jobid, bg_job[index].cmd);
                break;
            }
        }
        else {
            printf("bash: kill: %%%d: no such job\n", atoi(argv[1] + 1));
        }
        Sigprocmask(SIG_SETMASK, &prev, NULL);
        return 1;
    }
    // Sigprocmask(SIG_SETMASK, &prev, NULL);
    strcpy(pwd, lastpwd);
    Sigprocmask(SIG_SETMASK, &prev, NULL);
    return 0;                     /* Not a builtin command */
}
/* $end eval */

/* $begin parseline */
/* parseline - Parse the command line and build the argv array */
int parseline(char* buf, char** argv)
{
    char* delim;         /* Points to first space delimiter */
    int argc;            /* Number of args */
    int bg;              /* Background job? */

    buf[strlen(buf) - 1] = ' ';  /* Replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* Ignore leading spaces */
        buf++;

    /* Build the argv list */
    argc = 0;
    while ((delim = strchr(buf, ' '))) {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) /* Ignore spaces */
            buf++;
    }
    argv[argc] = NULL;

    if (argc == 0)  /* Ignore blank line */
        return 1;

    /* Should the job run in the background? */
    if ((bg = (*argv[argc - 1] == '&')) != 0)
        argv[--argc] = NULL;

    return bg;
}
/* $end parseline */
void gethome(char** argv) {
    char home[MAXLINE * 2];
    int i = 0;
    while (argv[i] != NULL) {
        if (argv[i] == NULL && i == 0) return;
        else if (argv[i][0] == '~') { // "~/" == $HOME
            if (!strcmp(argv[i], "~") || !strcmp(argv[i], "~/")) {
                argv[i] = getenv("HOME");
            }
            else if (argv[i][1] != '/') {
                i++;
                continue;
            }
            else {
                char* temp = strtok(argv[i], "/");
                temp = strtok(NULL, " ");
                char* t2 = getenv("HOME");
                strcpy(home, t2);
                strcat(home, "/");
                strcat(home, temp);

                strcpy(argv[i], home);
            }
        }
        i++;
    }
}
char* replace_bar_and_with_space_bar_and_space(char* buf) {
    char* ret = (char*)malloc(MAXLINE * 3);
    int i = 0;
    int j = 0;
    while (buf[i]) {
        if (buf[i] != '|' && buf[i] != '&' && buf[i] != '"' && buf[i] != '\'') ret[j++] = buf[i++];
        else if (buf[i] == '|') {
            strcpy(&ret[j], " | ");
            j += 3;
            i++;
        }
        else if (buf[i] == '&') {
            strcpy(&ret[j], " & ");
            j += 3;
            i++;
        }
        else if (buf[i] == '"' || buf[i] == '\''){
            strcpy(&ret[j], " ");
            j++;
            i++;
        }
    }
    ret[j] = '\0';
    return ret;
}

int count_pipe(char* buf) {
    int count = 0;
    for (int i = 0; buf[i] != '\0'; i++)
        if (buf[i] == '|') count++;

    return count;
}

void parse_with_pipe(char** argv, char*** newargv) {

    int i = 0;
    int j = 0;
    int k = 0;
    newargv[0] = (char**)malloc(sizeof(char*) * MAXARGS);
    while (argv[k] != NULL) {
        // printf("argv[%d](%d): %s\n",k,strlen(argv[k]),argv[k]);
        if (!strcmp(argv[k], "|")) {
            if (i == 0) {
                printf("bash: syntax error near unexpected token '|'\n");
                exit(0);
            }
            newargv[j][i] = NULL;
            j++;
            newargv[j] = (char**)malloc(sizeof(char*) * MAXARGS);
            i = 0;
        }
        else {
            newargv[j][i] = (char*)malloc(strlen(argv[k]) + 1);
            if (!strcmp(argv[k], "&") && i == 0) {
                if (i == 0) {
                    printf("bash: syntax error near unexpected token '|'\n");
                    exit(0);
                }
            }
            if ((argv[k][0] == '"' && argv[k][strlen(argv[k]) - 1] == '"') || argv[k][0] == '\'' && argv[k][strlen(argv[k]) - 1] == '\'') {
                argv[k][strlen(argv[k]) - 1] = '\0';
                strcpy(newargv[j][i], &argv[k][1]);
            }
            else strcpy(newargv[j][i], argv[k]);
            i++;
        }
        k++;

    }
    argv[j][i] = NULL;
}
void no_pipe_command(pid_t pid, int status, int bg, char* cmdline, char** argv) {
    gethome(argv);

    sigset_t mask_chld, mask_all, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_chld);
    Sigaddset(&mask_chld, SIGCHLD);

    if (!builtin_command(argv, bg)) {
        Sigprocmask(SIG_BLOCK, &mask_chld, &prev_one);
        if ((pid = Fork()) == 0) {
            Sigprocmask(SIG_SETMASK, &prev_one, NULL);
            if (execvp(argv[0], argv) < 0) {
                char* str = "Command not found.\n";
                printf("%s: %s", argv[0], str);
                exit(1);
            }
        }

/* Parent waits for foreground job to terminate */
        //if foreground
        if (!bg) {
            bg_job[++ps_cnt].pid = pid;
            insert_job(pid, 2, argv);
            Waitpid(pid, &status, WUNTRACED);

            for (int i = 0; i < MAXARGS; i++) {
                if (bg_job[i].pid == pid) {
                    if(bg_job[i].state > 0 && !status){
                        bg_job[i].state = -1;
                        bg_job[i].pid = 0;
                    }
                    break;
                }
            }
        }
        else {
            printf("[%d] %d\n", bg_cnt, pid);
            insert_job(pid, 1, argv);
        }
        
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);

}

void pipe_command(pid_t pid, int status, int bg, char* cmdline, char** _argv, char*** argv, int pipe_count) {
    int** fd = (int**)malloc(sizeof(int*) * pipe_count);
    for (int i = 0; i < pipe_count; i++) fd[i] = (int*)malloc(2 * sizeof(int));
    for (int i = 0; i < pipe_count; i++) pipe(fd[i]);
    for (int i = 0; i < pipe_count + 1; i++) gethome(argv[i]);

    sigset_t mask_chld, mask_all, prev_one;
    Sigfillset(&mask_all);
    Sigemptyset(&mask_chld);
    Sigaddset(&mask_chld, SIGCHLD);

    for (int i = 0; i <= pipe_count; i++) {
        if ((pid = Fork()) == 0) {
            if (i > 0) {
                Dup2(fd[i - 1][0], STDIN_FILENO);
                close(fd[i - 1][0]);
                close(fd[i - 1][1]);
            }
            if (i < pipe_count) {
                Dup2(fd[i][1], STDOUT_FILENO);
                close(fd[i][0]);
                close(fd[i][1]);
            }
            if (!builtin_command(argv[i], bg)) {
                if (execvp(argv[i][0], argv[i]) < 0) {
                    printf("%s: Command not found. \n", argv[i][0]);
                    exit(0);
                }
            }
            exit(0);
        }
        if (i > 0) close(fd[i - 1][0]);
        if (i < pipe_count) close(fd[i][1]);

        // pid = Waitpid(pid, &status, WUNTRACED);
        if (!bg) {

            Waitpid(pid, &status, WUNTRACED);
            
            if(!status){
                //if(i == 0)
                bg_job[++ps_cnt].pid = pid;
                insert_job(pid, 2, _argv);
                for (int i = 0; i < MAXARGS; i++) {
                    if (bg_job[i].pid == pid) {
                        if(bg_job[i].state != 0) 
                        {   
                            bg_job[i].state = -1;
                            bg_job[i].pid = 0;
                        }
                        break;
                    }
                }
            }
        }
        else {
            if (i == pipe_count) {
                printf("[%d] %d\n", bg_cnt, pid);
                insert_job(pid, 1, _argv);
            }

        }
        
    }
    for (int i = 0; i < pipe_count; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }
    Sigprocmask(SIG_SETMASK, &prev_one, NULL);
}

void insert_job(pid_t pid, int state, char** argv) {
    if (pid > 0) {

        memset(bg_job[(++ps_cnt)].cmd, 0, sizeof(bg_job[ps_cnt].cmd));
        bg_job[ps_cnt].pid = pid;
        bg_job[ps_cnt].state = state;
        int i = 0;
        while(argv[i] != NULL && argv[i][0] != '&'){
            strcat(bg_job[ps_cnt].cmd, argv[i]);
            strcat(bg_job[ps_cnt].cmd," ");
            i++;
        }
        if(!strchr(bg_job[ps_cnt].cmd, '\n')) strcat(bg_job[ps_cnt].cmd, "\n");
        //strcpy(bg_job[ps_cnt].cmd, cmd);

        if (state != 2 && state != -1) {
            bg_job[ps_cnt].jobid = bg_cnt;
            bg_cnt++;
        }
        else bg_job[ps_cnt].jobid = ps_cnt;

        return;
    }
    return;
}

void sigint_handler(int sig) {
    sigset_t mask, prev;
    int t_errno = errno;
    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, &prev);
    sio_puts("\n");
    
    for (int i = 0; i < MAXARGS; i++) {
        if (bg_job[i].state == 2) { /*foreground state*/
            ctrl_c = 1;
            bg_job[i].state = -1;
            Kill(bg_job[i].pid, SIGKILL);
            bg_job[i].pid = 0;
            break;
        }
    }
    Sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = t_errno;
}

void sigtstp_handler(int sig) {
    sigset_t mask, prev;
    int t_errno = errno;

    Sigfillset(&mask);
    Sigprocmask(SIG_BLOCK, &mask, &prev);

    
    Sio_puts("\n");
    for (int i = 0; i < MAXARGS; i++) {
        if (bg_job[i].state == 2 && bg_job[i].cmd != NULL && bg_job[i].pid > 0) {
            bg_job[i].state = 0;
            Kill(bg_job[i].pid, SIGTSTP);

            bg_job[i].jobid = bg_cnt;
            printf("[%d] Suspended        %s", bg_job[i].jobid, bg_job[i].cmd);
            bg_cnt++;
            break;
        }
    }

    Sigprocmask(SIG_SETMASK, &prev, NULL);
    errno = t_errno;
}

void sigchld_handler(int sig) {
    if(ctrl_c == 1){
        ctrl_c = -1;
        return;
    }
    sigset_t mask, prev;
    int t_errno = errno;

    Sigfillset(&mask);

    pid_t pid;
    int status;
    
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        Sigprocmask(SIG_BLOCK, &mask, &prev);
        if (!status) {

            for (int i = 0; i < MAXARGS; i++) {
                if (pid == bg_job[i].pid) {
                        printf("\n[%d] Done            %s", bg_job[i].jobid, bg_job[i].cmd);
                        bg_job[i].state = -1;
                        bg_job[i].pid = 0;
                        memset(bg_job[i].cmd, 0, MAXLINE);
                        break;
                }
            }

        }
        else {
            for (int i = 0; i < MAXARGS; i++) {
                if (pid == bg_job[i].pid && bg_job[i].state == 2) {
                    printf("\n[%d] Exit           %s",bg_job[i].jobid, bg_job[i].cmd);
                    bg_job[i].state = -1;
                    bg_job[i].pid = 0;
                    memset(bg_job[i].cmd, 0, MAXLINE);
                    break;
                }
            }
        }
        Sigprocmask(SIG_BLOCK, &prev, NULL);
    }
    int k = 0;
    for (int i = 0; i < MAXARGS; i++){
        if (bg_job[i].state == 2) bg_job[i].state = -1;
        if (bg_job[i].state == 0 || bg_job[i].state == 1) k = 1;
    }
    if (!k) bg_cnt = 1;
    errno = t_errno;
}