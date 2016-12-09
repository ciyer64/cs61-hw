#include "sh61.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>

// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command {
    int argc;      // number of arguments
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none

	// Part 2: Background
	int type;	   // command type (i.e. background or not)
	int tag;	   // keeping track of order for debugging

	// Part 3: Command Lists
	command* next; // next command
	command* prev; // prev command

	// Part 4: Conditionals, and Part 5: Pipes
	int sym;	   // vertical operator (&&, ||, or |)
	command* up;   // next command in a "column" (conditional/pipe)
	command* down; // previous command in a "column" (see above)

	// Part 5: Pipes
	int infd;	   // file descriptor for infile
	int outfd;	   // file descriptor for outfile
	int exstat;	   // exit status of command (for waitpid)

	// Part 7: Redirection
	// for each case (in, out, error), save descriptor and file
	int in_rd;
	char* inf;
	int out_rd;
	char* outf;
	int err_rd;
	char* errf;

};

void run_vert(command* c);
int accum_test(int acc, int ctype, int status, int cdr);
int should_run_proc(int acc, int ctype);
int term_cd(command* c, int* cdr);

command* head;
//command* tail;

//int currpgid = -1;

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
	c->inf = NULL;
	c->outf = NULL;
	c->errf = NULL;
    return c;
}


// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
	//command* tmp = c;
	//while (tmp){
		//c = tmp;
    for (int i = 0; i != c->argc; ++i)
		free(c->argv[i]);
    free(c->argv);
		//free(c->inf);
		//free(c->outf);
		//free(c->errf);
		//tmp = c->next;
    free(c);
	//}
}

// list_free(c)
//		Free list starting with c as its head.

void list_free(command* c) {
	command* nbtm;
	command* rcm;
	while(c){
		if(c->up){
			nbtm = nbtm->up;
			while(nbtm){
				rcm = nbtm;
				nbtm = nbtm->up;
				command_free(rcm);
			}
		}
		rcm = c;
		c = c->next;
		command_free(rcm);
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
    //(void) pgid;
    // Your code here!
	int pipefd[2];
	int shouldrun=1;


	while(shouldrun==1){

		if (c->infd != 0) {
			close(pipefd[1]);
		}

		if (c->sym == TOKEN_PIPE){
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
				if (pgid == 0)
					setpgid(0, 0);
				else
					setpgid(0, pgid);

				// handle redirects
				if (c->in_rd == 1){
					c->in_rd = open(c->inf, O_RDONLY);
					if (c->in_rd == -1){
						fprintf(stderr, "No such file or directory\n");
						_exit(1);
					}
					dup2(c->in_rd, STDIN_FILENO);
					close(c->in_rd);
				}
				if (c->out_rd == 1){
					c->out_rd = creat(c->outf, S_IRWXU);
					if (c->out_rd == -1){
						fprintf(stderr, "No such file or directory\n");
						_exit(1);
					}
					dup2(c->out_rd, STDOUT_FILENO);
					close(c->out_rd);				
				}
				if (c->err_rd == 1){
					c->err_rd = creat(c->errf, S_IRWXU);
					if (c->err_rd == -1){
						fprintf(stderr, "No such file or directory\n");
						_exit(1);
					}
					dup2(c->err_rd, STDERR_FILENO);
					close(c->err_rd);				
				}
				// for writing into pipe:
				if (c->outfd != 1){
					close(pipefd[0]);
					dup2(c->outfd, STDOUT_FILENO);
					close(c->outfd);
				}
				// for reading into pipe:
				if (c->infd != 0){
					if (c->inf == NULL){
						dup2(c->infd, STDIN_FILENO);
						close(c->infd);
					}
				}

				execvp(c->argv[0],c->argv);
				_exit(1);
				break;

			// error: exit
			case -1:
				_exit(1);
				break;

			// parent process: do nothing, save child pid
			default:
				setpgid(c->pid,pgid);
				break;
		}
		// break if the next command isn't piped 
		// or if reached end of column
		if(c->up == NULL || c->sym != TOKEN_PIPE) {
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

// FUNCTION SUMMARY:
// if the command is not backgrounded, we run the column then proceed
// if it is backgrounded, we fork:
// 		-> if it is the child, we run the column then exit
//		-> if it is the parent, we finished the column, so we proceed horizontally
//		-> if it is error, just exit

void run_list(command* c) {
	while (c) {
		// handle CD in the parent process
		if (c->argv && strcmp("cd", c->argv[0])==0){
			chdir(c->argv[1]);
		}
		
		if (c->type == TOKEN_BACKGROUND) {
			pid_t f = fork();

			switch(f) {

				case 0:
					run_vert(c);
					_exit(1);
					break;

				case -1:
					_exit(1);
					break;
				
				default:
					c = c->next;
					break;
			}
		}

		else {
			run_vert(c);
			c = c->next;
		}
	}
    //fprintf(stderr, "run_command not done yet\n");
}

// run_vert(c)
// Run a vertical column of commands (i.e. pipes and/or conditionals)
void run_vert(command* c) {
	int shouldrun = 1;
	int accum = 1;
	int prev_sym = -2;
	int cdr = -2;

	while (c) {
		// handle CD in the child process
		if (c->argv && strcmp("cd", c->argv[0])==0){
			chdir(c->argv[1]);
		}

		if (shouldrun) {
			pid_t cpr = start_command(c, 0);
			while(c->sym == TOKEN_PIPE && c->up){
				c = c->up;
			}
			set_foreground(c->pid);
			waitpid(cpr, &c->exstat, 0);
			set_foreground(0);
			accum = accum_test(accum, prev_sym, c->exstat, cdr);
		}
		prev_sym = c->sym;
		shouldrun = should_run_proc(accum, prev_sym);
		c = c->up;		
	}
}

int accum_test(int acc, int ctype, int status, int cdr) {
	// redirect or CD failed
	if (errno != 0 || cdr == -1)
		return 0;
	// first command
	else if (ctype == -2)
		return (WEXITSTATUS(status) == 0);
	// command joined by an AND
	else if (ctype == TOKEN_AND)
		return (acc && (WEXITSTATUS(status) == 0));
	// command joined by an OR
	else if (ctype == TOKEN_OR)
		return (acc || (WEXITSTATUS(status) == 0));
	// error case
	return -1;
}

// should_run_proc(acc, ctype)
// determines whether to run a process based on conditionals:
// Run (return 1) if:
//		-> token is AND and prev process worked, or
//		-> token is OR and prev process didn't work
// Don't run (return 0) if:
//		-> token is OR and prev process didn't work, or
//		-> token is OR and prev process worked

int should_run_proc(int acc, int ctype) {

	if (((ctype == TOKEN_AND) && (acc != 0)) || 
		((ctype == TOKEN_OR) && (acc == 0)))
		return 1;

	else if (((ctype == TOKEN_AND) && (acc == 0)) ||
		((ctype == TOKEN_OR) && (acc != 0)))
		return 0;

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
	// cursor for bottom (impt for backgrounding)
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
			curr->sym = type;
			curr = curr->up;
		}
		
		// pipe: expand vertically
		else if (type == TOKEN_PIPE) {
			curr->up = command_alloc();
			curr->sym = type;
			curr = curr->up;
		}

		else if (type == TOKEN_REDIRECTION) {
			int type_rd = 0;
			// check which redirect
			if (strcmp("<", token) == 0){
				curr->in_rd = 1;
				type_rd = 1;
			}
			else if (strcmp(">", token) == 0){
				curr->out_rd = 1;
				type_rd = 2;
			}
			else if (strcmp("2>", token) == 0){
				curr->err_rd = 1;
				type_rd = 3;
			}
			// parse the next token (file name)
			// and save it

			s = parse_shell_token(s, &type, &token);

			switch(type_rd){
				case 1:
					curr->inf = token;
					break;
				case 2:
					curr->outf = token;
					break;
				case 3:
					curr->errf = token;
					break;
			}
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
		int zombstat;
		while(waitpid(-1, &zombstat, WNOHANG) > 0);

    }

    return 0;
}
