#ifndef PROC_HELPER_H
#define PROC_HELPER_H

#include <stdlib.h>
// Header of functions for managing processes and related operations

// Get the full path of the directory where the main executable is found
int get_current_exec_path(char *exec_path, size_t path_buff_len);

// This function blocks until a list of processes terminate or timeout
// process_ids is a pointer to the PID list of length n_processes
// Returns 0 on success (all child processes have finished) or an errno
// code if on error or timeout
// Warning: This function stops the system timer
int wait_processes(pid_t *process_ids, size_t n_processes, int wait_timeout);

// Sends the SIGTERM signal to a list processes
// The list of PIDs is pointed by process_ids and its length in n_processes
void kill_processes(pid_t *process_ids, size_t n_processes);

// Executes a program in a child process
// The PID of the created process is returned after successful execution. new_proc_id must point to
// a var when the new PID will be stored.
// exec_filename must point to a \0 terminated string containing the program filename
// exec_argv is an array of pointers. Each poining to a string containing a program argument.
// The first argument is the program filename and the last one must be a NULL pointer
int run_background_command(pid_t *new_proc_id, const char *exec_filename, char *const exec_argv[]);

// Configure the system real-time timer to send a SIGALRM signal to the current process
// SIGALRM must be handled before calling this function
// this signal will be send each interval_sec seconds
// If interval_sec is negative, the timer is stopped
// The function returns 0 on success, or a errno error code on error
int configure_timer(float interval_sec);

int daemonize(char *working_dir);

#endif // PROC_HELPER
