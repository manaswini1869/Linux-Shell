#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <pwd.h>
#include <glob.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "wsh.h"

struct shell_info *wsh_shell;

// Getting the job id by process id

int get_job_id_by_pid(int pid) {
    int i;
    struct process *proc;

    for (i = 1; i <= MAX_JOBS; i++) {
        if (wsh_shell->jobs[i] != NULL) {
            for (proc = wsh_shell->jobs[i]->root; proc != NULL; proc = proc->next) {
                if (proc->pid == pid) {
                    return i;
                }
            }
        }
    }

    return -1;
}

// Getting the job by id

struct job* get_job_by_id(int id) {
    if (id > MAX_JOBS) {
        return NULL;
    }

    return wsh_shell->jobs[id];
}

// Getting the process id by job id

int get_pgid_by_job_id(int id) {
    struct job *job = get_job_by_id(id);

    if (job == NULL) {
        return -1;
    }

    return job->pgid;
}

// Releasing the job

int release_job(int id) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    struct job *job = wsh_shell->jobs[id];
    struct process *proc, *tmp;
    for (proc = job->root; proc != NULL; ) {
        tmp = proc->next;        
        free(proc->output_path);
        free(proc->command);
        free(proc->argv);
        free(proc->input_path);
        free(proc);
        proc = tmp;
    }

    free(job->command);
    free(job);

    return 0;
}

// Getting the process count

int get_proc_count(int id, int filter) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    int count = 0;
    struct process *proc;
    for (proc = wsh_shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (filter == PROC_FILTER_ALL ||
            (filter == PROC_FILTER_DONE && proc->status == STATUS_DONE) ||
            (filter == PROC_FILTER_REMAINING && proc->status != STATUS_DONE)) {
            count++;
        }
    }

    return count;
}

// Getting the next job id
int get_next_job_id() {
    int i;
    int used[MAX_JOBS + 1]; // An array to keep track of used job IDs

    for (i = 1; i <= MAX_JOBS; i++) {
        used[i] = 0; // Initialize all IDs as unused
        if (wsh_shell->jobs[i] != NULL) {
            used[wsh_shell->jobs[i]->id] = 1; // Mark used IDs
        }
    }

    for (i = 1; i <= MAX_JOBS; i++) {
        if (!used[i]) {
            return i; // Return the first unused ID
        }
    }

    return -1; // No available IDs
}

// Getting the job id

int small_job_id(){
    int id;
    int found = 0;

    while (!found && id <= MAX_JOBS)
    {
        found = 1;
        for (int i=0; i<MAX_JOBS; i++)
        {
            if (wsh_shell->jobs[i]->job_id == id)
            {
                found = 0;
                break;
            }
        }
        if (!found)
        {
            id++;
        }
    }

    if (id > MAX_JOBS)
    {
        return -1;
    }
    else
    {
        return id;
    }
    
}

// Removing the job

int remove_job(int id) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    release_job(id);
    wsh_shell->jobs[id] = NULL;

    return 0;
}

// Inserting the job

int insert_job(struct job *job) {
    int id = get_next_job_id();
    //int job_id = -1;

    if (id < 0) {
        return -1;
    }
    
    job->id = id;
    wsh_shell->jobs[id] = job;
    return id;
}

// Checking if the job is completed

int is_job_completed(int id) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return 0;
    }

    struct process *proc;
    for (proc = wsh_shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            return 0;
        }
    }

    return 1;
}

// Setting the status of the process

int set_process_status(int pid, int status) {
    int i;
    struct process *proc;

    for (i = 1; i <= MAX_JOBS; i++) {
        if (wsh_shell->jobs[i] == NULL) {
            continue;
        }
        for (proc = wsh_shell->jobs[i]->root; proc != NULL; proc = proc->next) {
            if (proc->pid == pid) {
                proc->status = status;
                return 0;
            }
        }
    }

    return -1;
}

// Waiting for the process to complete

int wait_for_pid(int pid) {
    int status = 0;

    waitpid(pid, &status, WUNTRACED);
    if (WIFEXITED(status)) {
        set_process_status(pid, STATUS_DONE);
    } else if (WIFSIGNALED(status)) {
        set_process_status(pid, STATUS_TERMINATED);
    } else if (WSTOPSIG(status)) {
        status = -1;
        set_process_status(pid, STATUS_SUSPENDED);
    }

    return status;
}

// Setting the status of the jobs

int set_job_status(int id, int status) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    //int i;
    struct process *proc;

    for (proc = wsh_shell->jobs[id]->root; proc != NULL; proc = proc->next) {
        if (proc->status != STATUS_DONE) {
            proc->status = status;
        }
    }

    return 0;
}

// Printing the status of the jobs the print response when jobs command is run

int print_job_status(int id) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    printf("%d: ", id);

    struct process *proc;
    for (proc = wsh_shell ->jobs[id]->root; proc != NULL; proc = proc->next) {
        printf("%s\n",proc->command);
        if (proc->next != NULL) {
            printf("|");
        }
    }

    return 0;
}

// Waiting for jobs

int wait_for_job(int id) {
    if (id > MAX_JOBS || wsh_shell->jobs[id] == NULL) {
        return -1;
    }

    int proc_count = get_proc_count(id, PROC_FILTER_REMAINING);
    int wait_pid = -1, wait_count = 0;
    int status = 0;

    do {
        wait_pid = waitpid(-wsh_shell->jobs[id]->pgid, &status, WUNTRACED);
        wait_count++;

        if (WIFEXITED(status)) {
            set_process_status(wait_pid, STATUS_DONE);
        } else if (WIFSIGNALED(status)) {
            set_process_status(wait_pid, STATUS_TERMINATED);
        } else if (WSTOPSIG(status)) {
            status = -1;
            set_process_status(wait_pid, STATUS_SUSPENDED);
        }
    } while (wait_count < proc_count);

    return status;
}

// Extracting the type of commands from the command line

int get_command_type(char *command) {
    if (strcmp(command, "exit") == 0) {
        return COMMAND_EXIT;
    } else if (strcmp(command, "cd") == 0) {
        return COMMAND_CD;
    } else if (strcmp(command, "jobs") == 0) {
        return COMMAND_JOBS;
    } else if (strcmp(command, "fg") == 0) {
        return COMMAND_FG;
    } else if (strcmp(command, "bg") == 0) {
        return COMMAND_BG;
    } else {
        return COMMAND_EXTERNAL;
    }
}

// Trim the leading and trailing spaces of the command line

char* helper_strtrim(char* line) {
    char *head = line, *tail = line + strlen(line);

    while (*head == ' ') {
        head++;
    }
    while (*tail == ' ') {
        tail--;
    }
    *(tail + 1) = '\0';

    return head;
}

// Built-in commands

int wsh_cd(int argc, char** argv) {
    if (argc == 2) {
        if (chdir(argv[1]) < 0) {
            return -1;
        }
    }
    return 0;
}


int wsh_jobs(int argc, char **argv) {
    int i;

    for (i = 0; i < MAX_JOBS; i++) {
        if (wsh_shell->jobs[i] != NULL) {
           print_job_status(i);
        }
    }

    return 0;
}

int wsh_fg(int argc, char **argv) {
    if (argc > 2) {
        return -1;
    }

    //int status;
    pid_t pid;
    int job_id = -1;

    if (argc > 1) {
        if (argv[1][0] == '%') {
            job_id = atoi(argv[1] + 1);
        } else {
            job_id = atoi(argv[1]);
        }
    } else {
        for (int i = MAX_JOBS; i >= 1; i--) {
            if (wsh_shell->jobs[i] != NULL) {
                job_id = i;
                break;
            }
        }
    }

    pid = get_pgid_by_job_id(job_id);

    if (kill(-pid, SIGCONT) < 0) {
        return -1;
    }

    tcsetpgrp(0, pid);

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
        if (wait_for_job(job_id) >= 0) {
            remove_job(job_id);
        }
    } else {
        wait_for_pid(pid);
    }

    signal(SIGTTOU, SIG_IGN);
    tcsetpgrp(0, getpid());
    signal(SIGTTOU, SIG_DFL);

    return 0;
}

int wsh_bg(int argc, char **argv) {
    if (argc > 2) {
        return -1;
    }

    pid_t pid;
    int job_id = -1;

    if (argc > 1) {
        if (argv[1][0] == '%') {
            job_id = atoi(argv[1] + 1);
        } else {
            job_id = atoi(argv[1]);
        }
    } else {
        for (int i = MAX_JOBS; i >= 1; i--) {
            if (wsh_shell->jobs[i] != NULL) {
                job_id = i;
                break;
            }
        }
    }

    pid = get_pgid_by_job_id(job_id);

    if (kill(-pid, SIGCONT) < 0) {
        return -1;
    }

    if (job_id > 0) {
        set_job_status(job_id, STATUS_CONTINUED);
    }

    return 0;
}

int wsh_exit() {
    exit(0);
}

// Launching jobs

void check_zombie() {
    int status, pid;
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0) {
        if (WIFEXITED(status)) {
            set_process_status(pid, STATUS_DONE);
        } else if (WIFSTOPPED(status)) {
            set_process_status(pid, STATUS_SUSPENDED);
        } else if (WIFCONTINUED(status)) {
            set_process_status(pid, STATUS_CONTINUED);
        }

        int job_id = get_job_id_by_pid(pid);
        if (job_id > 0 && is_job_completed(job_id)) {
            remove_job(job_id);
        }
    }
}

// Execute built-in commands

int wsh_execute_builtin_command(struct process *proc) {
    int status = 1;

    switch (proc->type) {
        case COMMAND_CD:
            wsh_cd(proc->argc, proc->argv);
            break;
        case COMMAND_JOBS:
            wsh_jobs(proc->argc, proc->argv);
            break;
        case COMMAND_FG:
            wsh_fg(proc->argc, proc->argv);
            break;
        case COMMAND_BG:
            wsh_bg(proc->argc, proc->argv);
            break;        
        case COMMAND_EXIT:
            wsh_exit();
            break;
        default:
            status = 0;
            break;
    }

    return status;
}

// Launching external commands

int wsh_launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode) {
    proc->status = STATUS_RUNNING;
    if (proc->type != COMMAND_EXTERNAL && wsh_execute_builtin_command(proc)) {
        return 0;
    }

    pid_t childpid;
    int status = 0;

    childpid = fork();

    if (childpid < 0) {
        return -1;
    } else if (childpid == 0) {         // child process
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        proc->pid = getpid();
        if (job->pgid > 0) {
            setpgid(0, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(0, job->pgid);
        }

        if (in_fd != 0) {
            dup2(in_fd, 0);
            close(in_fd);
        }

        if (out_fd != 1) {
            dup2(out_fd, 1);
            close(out_fd);
        }

        if (execvp(proc->argv[0], proc->argv) < 0) {
            printf("wsh: %s: command not found", proc->argv[0]);
            exit(0);
        }

        exit(0);
    } else {   // parent process
        proc->pid = childpid;
        if (job->pgid > 0) {
            setpgid(childpid, job->pgid);
        } else {
            job->pgid = proc->pid;
            setpgid(childpid, job->pgid);
        }

        if (mode == FOREGROUND_EXECUTION) {
            tcsetpgrp(0, job->pgid);
            status = wait_for_job(job->id);
            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
    }

    return status;
}

// Launching jobs

int wsh_launch_job(struct job *job) {
    struct process *proc;
    int status = 0, in_fd = 0, fd[2], job_id = -1;

    check_zombie();
    if (job->root->type == COMMAND_EXTERNAL) {
        job_id = insert_job(job);
    }

    for (proc = job->root; proc != NULL; proc = proc->next) {
        if (proc == job->root && proc->input_path != NULL) {
            in_fd = open(proc->input_path, O_RDONLY);
            if (in_fd < 0) {
                printf("wsh: no such file or directory: %s", proc->input_path);
                remove_job(job_id);
                return -1;
            }
        }
        if (proc->next != NULL) {
            pipe(fd);
            status = wsh_launch_process(job, proc, in_fd, fd[1], PIPELINE_EXECUTION);
            close(fd[1]);
            in_fd = fd[0];
        } else {
            int out_fd = 1;
            if (proc->output_path != NULL) {
                out_fd = open(proc->output_path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
                if (out_fd < 0) {
                    out_fd = 1;
                }
            }
            status = wsh_launch_process(job, proc, in_fd, out_fd, job->mode);
        }
    }

    if (job->root->type == COMMAND_EXTERNAL) {
        if (status >= 0 && job->mode == FOREGROUND_EXECUTION) {
            remove_job(job_id);
        }
    }

    return status;
}

// Parsing the command line

struct process* wsh_parse_command_segment(char *segment) {
    int bufsize = TOKEN_BUFSIZE;
    int position = 0;
    char *command = strdup(segment);
    char *token;
    char **tokens = (char**) malloc(bufsize * sizeof(char*));

    if (!tokens) {
        fprintf(stderr, "wsh: allocation error");
        exit(EXIT_FAILURE);
    }

    token = strtok(segment, TOKEN_DELIMITERS);
    while (token != NULL) {
        glob_t glob_buffer;
        int glob_count = 0;
        if (strchr(token, '*') != NULL || strchr(token, '?') != NULL) {
            glob(token, 0, NULL, &glob_buffer);
            glob_count = glob_buffer.gl_pathc;
        }

        if (position + glob_count >= bufsize) {
            bufsize += TOKEN_BUFSIZE;
            bufsize += glob_count;
            tokens = (char**) realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "wsh: allocation error");
                exit(EXIT_FAILURE);
            }
        }

        if (glob_count > 0) {
            int i;
            for (i = 0; i < glob_count; i++) {
                tokens[position++] = strdup(glob_buffer.gl_pathv[i]);
            }
            globfree(&glob_buffer);
        } else {
            tokens[position] = token;
            position++;
        }

        token = strtok(NULL, TOKEN_DELIMITERS);
    }

    int i = 0, argc = 0;
    char *input_path = NULL, *output_path = NULL;
    while (i < position) {
        if (tokens[i][0] == '<' || tokens[i][0] == '>') {
            break;
        }
        i++;
    }
    argc = i;

    for (; i < position; i++) {
        if (tokens[i][0] == '<') {
            if (strlen(tokens[i]) == 1) {
                input_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(input_path, tokens[i + 1]);
                i++;
            } else {
                input_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(input_path, tokens[i] + 1);
            }
        } else if (tokens[i][0] == '>') {
            if (strlen(tokens[i]) == 1) {
                output_path = (char *) malloc((strlen(tokens[i + 1]) + 1) * sizeof(char));
                strcpy(output_path, tokens[i + 1]);
                i++;
            } else {
                output_path = (char *) malloc(strlen(tokens[i]) * sizeof(char));
                strcpy(output_path, tokens[i] + 1);
            }
        } else {
            break;
        }
    }

    for (i = argc; i <= position; i++) {
        tokens[i] = NULL;
    }

    // Initializing the process with the job details

    struct process *new_proc = (struct process*) malloc(sizeof(struct process));
    new_proc->command = command;
    new_proc->argv = tokens;
    new_proc->argc = argc;
    new_proc->input_path = input_path;
    new_proc->output_path = output_path;
    new_proc->pid = -1;
    new_proc->type = get_command_type(tokens[0]);
    new_proc->next = NULL;
    return new_proc;
}

// Parsing the command line

struct job* wsh_parse_command(char *line) {
    line = helper_strtrim(line);
    char *command = strdup(line);

    struct process *root_proc = NULL, *proc = NULL;
    char *line_cursor = line, *c = line, *seg;
    int seg_len = 0, mode = FOREGROUND_EXECUTION;

    if (line[strlen(line) - 1] == '&') {
        mode = BACKGROUND_EXECUTION;
        line[strlen(line) - 1] = '\0';
    }

    while (1) {
        if (*c == '\0' || *c == '|') {
            seg = (char*) malloc((seg_len + 1) * sizeof(char));
            strncpy(seg, line_cursor, seg_len);
            seg[seg_len] = '\0';

            struct process* new_proc = wsh_parse_command_segment(seg);
            if (!root_proc) {
                root_proc = new_proc;
                proc = root_proc;
            } else {
                proc->next = new_proc;
                proc = new_proc;
            }

            if (*c != '\0') {
                line_cursor = c;
                while (*(++line_cursor) == ' ');
                c = line_cursor;
                seg_len = 0;
                continue;
            } else {
                break;
            }
        } else {
            seg_len++;
            c++;
        }
    }

    struct job *new_job = (struct job*) malloc(sizeof(struct job));
    new_job->root = root_proc;
    new_job->command = command;
    new_job->pgid = -1;
    new_job->mode = mode;
    return new_job;
}

// Reading the command line

char* wsh_read_line() {
    int bufsize = COMMAND_BUFSIZE;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    if (!buffer) {
        fprintf(stderr, "wsh: allocation error");
        exit(EXIT_FAILURE);
    }

    while (1) {
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position] = c;
        }
        position++;

        if (position >= bufsize) {
            bufsize += COMMAND_BUFSIZE;
            buffer = realloc(buffer, bufsize);
            if (!buffer) {
                fprintf(stderr, "wsh: allocation error");
                exit(EXIT_FAILURE);
            }
        }
    }
}

// Main loop of the shell

void wsh_loop() {
    char *line;
    struct job *job;
    int status = 1;

    do {
        printf("wsh> ");
        line = wsh_read_line();
        if (strlen(line) == 0) {
            check_zombie();
            continue;
        }
        job = wsh_parse_command(line);
        status = wsh_launch_job(job);
    }while (1);
    
    printf("%d", status);
}

// Ctrl+C signal handler

void signal_handler(int signo) {
    for(int i=0; i<MAX_JOBS; i++){
        kill(-wsh_shell->jobs[i]->pgid, signo);
        set_job_status(i, STATUS_TERMINATED);
    }
}

// Initializing the shell with the required signal handlers

void wsh_init() {
    struct sigaction sigint_action = {
        .sa_handler = SIG_IGN,
        .sa_flags = 0
    };
    sigemptyset(&sigint_action.sa_mask);
    sigaction(SIGINT, &sigint_action, NULL);

    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);

    wsh_shell = (struct shell_info*) malloc(sizeof(struct shell_info));

    int i;
    for (i = 0; i < MAX_JOBS; i++) {
        wsh_shell->jobs[i] = NULL;
    }
}

// Main function

int main(int argc, char **argv) {
    if (argc ==2) {
        char *line = NULL;
        size_t len = 0;
        ssize_t read;
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            exit(EXIT_FAILURE);
        }
        while ((read = getline(&line, &len, fp)) != -1) {
            struct job *job = wsh_parse_command(line);
            wsh_launch_job(job);
        }
        fclose(fp);
        
    }else{
        wsh_init();
        wsh_loop();
    }

    return EXIT_SUCCESS;
}