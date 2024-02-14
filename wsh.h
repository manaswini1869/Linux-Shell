#ifndef WSH_H
#define WSH_H

#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX_JOBS 256
#define PATH_BUFSIZE 1024
#define COMMAND_BUFSIZE 1024
#define TOKEN_BUFSIZE 64
#define TOKEN_DELIMITERS " "

#define BACKGROUND_EXECUTION 0
#define FOREGROUND_EXECUTION 1
#define PIPELINE_EXECUTION 2

#define COMMAND_EXTERNAL 0
#define COMMAND_EXIT 1
#define COMMAND_CD 2
#define COMMAND_JOBS 3
#define COMMAND_FG 4
#define COMMAND_BG 5

#define STATUS_RUNNING 0
#define STATUS_DONE 1
#define STATUS_SUSPENDED 2
#define STATUS_CONTINUED 3
#define STATUS_TERMINATED 4

#define PROC_FILTER_ALL 0
#define PROC_FILTER_DONE 1
#define PROC_FILTER_REMAINING 2

//Declaring all the required structures

struct process {
    char *command;
    int argc;
    char **argv;
    char *input_path;
    char *output_path;
    pid_t pid;
    int type;
    int status;
    struct process *next;
};

struct job {
    int job_id;
    int id;
    struct process *root;
    char *command;
    pid_t pgid;
    int mode;
};

struct shell_info {
    struct job *jobs[MAX_JOBS + 1];
};

extern const char *STATUS_STRING[];

//Declaring all the required functions

int get_job_id_by_pid(int pid);
struct job *get_job_by_id(int id);
int get_pgid_by_job_id(int id);
int get_proc_count(int id, int filter);
int get_next_job_id();
int release_job(int id);
int small_job_id();
int insert_job(struct job *job);
int remove_job(int id);
int is_job_completed(int id);
int set_process_status(int pid, int status);
int set_job_status(int id, int status);
int wait_for_pid(int pid);
int wait_for_job(int id);
int get_command_type(char *command);
char *helper_strtrim(char *line);
int wsh_cd(int argc, char **argv);
int print_job_status(int id);
int wsh_jobs(int argc, char **argv);
int wsh_fg(int argc, char **argv);
int wsh_bg(int argc, char **argv);
int wsh_exit();
void check_zombie();
int wsh_execute_builtin_command(struct process *proc);
int wsh_launch_process(struct job *job, struct process *proc, int in_fd, int out_fd, int mode);
int wsh_launch_job(struct job *job);
struct process *wsh_parse_command_segment(char *segment);
struct job *wsh_parse_command(char *line);
char *wsh_read_line();
void wsh_loop();
void signal_handler(int signo);
void wsh_init();

#endif /* WSH_H */
