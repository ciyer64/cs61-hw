#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define TRUE 1
#define FALSE 0

// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
	int type;	   // command type (i.e. background or not)
	int ctype;	   // condition type (&&, ||)
	int tag;	   // keeping track of order for debugging
	command* next; // next command in list
	command* prev; // prev command in list
	command* up;   // yay conditionals
	command* down; // more conditionals yay
	int infd;	   // file descriptor for infile
	int outfd;	   // file descriptor for outfile
	int ptype;
};

void run_vert(command* c);
int accum_test(int acc, int ctype, int status);
int should_run_proc(int acc, int ctype);

command* head;
//command* tail;

// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->pid = -1;
	c->tag = 0;
	c->infd = 0;
	c->outfd = 1;
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
	command* tmp = c;
	while (tmp){
		c = tmp;
    	for (int i = 0; i != c->argc; ++i)
        	free(c->argv[i]);
    	free(c->argv);
		tmp = c->next;
    	free(c);
	}
}


// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.

pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
    // Your code here!
	int pipefd[2];
	int shouldrun=1;
	while(shouldrun==1){

		if (c->infd != 0) {
			close(pipefd[1]);
		}

		if (c->ptype == TOKEN_PIPE){
			int ret = pipe(pipefd);
			if(ret == -1){
				_exit(1);
			}
			c->outfd = pipefd[1];
			c->up->infd = pipefd[0];
		}
	
		c->pid = fork();
		switch (c->pid) {
			// child process: execute
			case 0:
				// for writing into pipe:
				if(c->outfd != 1){
					close(pipefd[0]);
					dup2(c->outfd, STDOUT_FILENO);
					close(c->outfd);
				}
				// for reading into pipe:
				if(c->infd != 0){
					dup2(c->infd, STDOUT_FILENO);
					close(c->infd);
				}
				execvp(c->argv[0],c->argv);
				_exit(1);
				break;

			// error case
			case -1:
				_exit(1);
				break;

			// parent process: do nothing, save child pid
			default:
				break;
		}
		if(c->up == NULL || c->up->ptype != TOKEN_PIPE) {
			shouldrun=0;
		}
		else{
			c = c->up;
		}
	}
	return c->pid;
}


// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `set_foreground(pgid)` before waiting for the pipeline.
//       - Call `set_foreground(0)` once the pipeline is complete.
//       - Cancel the list when you detect interruption.

void run_list(command* c) {
	while (c) {
		// if background process
		if (c->type == TOKEN_BACKGROUND) {
			pid_t f = fork();
			// run in child process
			if (f == 0) {
				run_vert(c);
				_exit(1);
			}
			// move to next column of commands
			// in parent process
			else if (f > 0) {
				c = c->next;
				continue;
			}
			else if (f == -1) {
				_exit(1);
			}
		}
		// if not background
		else {
			// run a column of commands (i.e. conditionals),
			// then move to next column
			run_vert(c);
			c = c->next;
		}
	}
    //fprintf(stderr, "run_command not done yet\n");
}

void run_vert(command* c) {
	int status = 0;
	int shouldrun = 1;
	int accum = 1;
	int prev_log = -2;
	//command* pipe_fin;

	while (c) {

		//pipe_fin = c;

		if (shouldrun) {
			pid_t cpr = start_command(c, 0);
			while(c->ptype == TOKEN_PIPE && c->up){
				c = c->up;
			}
			waitpid(cpr, &status, 0);
			accum = accum_test(accum, prev_log, status);
		}
		prev_log = c->ctype;
		shouldrun = should_run_proc(accum, prev_log);	
		c = c->up;
	}
}

int accum_test(int acc, int ctype, int status) {
	// first command
	if (ctype == -2)
		return (WEXITSTATUS(status) == 0);
	// command joined by an AND
	else if (ctype == TOKEN_AND)
		return (acc && (WEXITSTATUS(status) == 0));
	// command joined by an OR
	else if (ctype == TOKEN_OR)
		return (acc || (WEXITSTATUS(status) == 0));
	// error case
	else
		return -1;
}

int should_run_proc(int acc, int ctype) {
	// if prev process worked, run stuff after AND
	if ((ctype == TOKEN_AND) && (acc != 0))
		return 1;
	// if prev process didn't work, don't run stuff after AND
	else if ((ctype == TOKEN_AND) && (acc == 0))
		return 0;
	// if prev process worked, don't run stuff after OR
	else if ((ctype == TOKEN_OR) && (acc != 0))
		return 0;
	// if prev process didn't work, run stuff after OR
	else if ((ctype == TOKEN_OR) && (acc == 0))
		return 1;
	else
		return -1;
}


// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;
    // Your code here!

	// build the command
	// initialize "head"
	head = command_alloc();
	// cursor used for traversing & building
	command* curr = head;
	command* bottom = head;
    while ((s = parse_shell_token(s, &type, &token)) != NULL) {

		// sequence / background: expand horizontally
		if (type == TOKEN_BACKGROUND || type == TOKEN_SEQUENCE) {
			bottom->next = command_alloc();
			if (type == TOKEN_BACKGROUND) {			
				bottom->type = type;
			}
			curr = bottom->next;
			bottom = curr;
		}

		// conditional: expand vertically
		else if (type == TOKEN_AND || type == TOKEN_OR) {
			curr->up = command_alloc();
			curr->ctype = type;
			curr = curr->up;
		}
		
		// pipe: expand vertically
		else if (type == TOKEN_PIPE) {
			curr->up = command_alloc();
			curr->ptype = type;
			curr = curr->up;
		}
		

		// normal: add arguments
		else {
			command_append_arg(curr, token);
		}
	}
    // execute it
	if (head->argc)
		run_list(head);
    command_free(head);
}


int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    set_foreground(0);
    handle_signal(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            printf("sh61[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file))
                    perror("sh61");
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
