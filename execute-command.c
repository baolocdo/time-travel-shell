/*-----------------------------------------------------------------------------
	UCLA CS 111 Lab 1. "Time Travel Shell"
	Professor Peter Reiher
	Winter 2014

	FILE:   execute-command.c
	DESCR:  Executes commands from shell script.

	AUTHOR(s):
	Alan Kha        904030522	akhahaha@gmail.com
	Braden Anderson 203744563	bradencanderson@gmail.com

-----------------------------------------------------------------------------*/

#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// Returns the status of command c.
int command_status (command_t c)
{
	return c->status;
}

// Executes command c. No idea what the time_travel arg is for.
// main() calls this with root. successive recursive calls traverse the tree.
void execute_command (command_t c, int time_travel)
{
	pid_t child;
	int fd[2];
	int r = 0;
	int w = 1;

	switch (c->type)
	{
		case SIMPLE_COMMAND:
			// create child to execute command
			child = fork();

			if (child == 0) // child process
			{
				// handle input < redirect
				if (c->input)
				{
					// open input file
					int in;
					if ((in = open(c->input, O_RDONLY, 0666)) == -1)
						error(3, 0, "Cannot open input file.");

					// replace standard input with input file
					if (dup2(in, 0) == -1)
						error(3, 0, "Cannot perform input redirect.");

					close(in);
				}

				// handle output > redirect
				if (c->output)
				{
					// open output file
					int out;
					if ((out = open(c->output, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == -1)
						error(3, 0, "Cannot open output file.");

					// replace standard output with output file
					if (dup2(out, 1) == -1)
						error(3, 0, "Cannot perform output redirect.");

					close(out);
				}

				// execute command
				execvp(c->u.word[0], c->u.word);
				error(3, 0, "Cannot execute command.");
			}
			else if (child > 0) // parent process
			{
				// wait for the child process
				int status;
				waitpid(child, &status, 0);
				c->status = status;
			}
			else
				error(3, 0, "Cannot create child process.");
			break;

		case AND_COMMAND:
			// execute first command
			execute_command(c->u.command[0], time_travel);
			c->status = c->u.command[0]->status;

			// execute second if first succeeds
			if (!c->status)
			{
				execute_command(c->u.command[1], time_travel);
				c->status = c->u.command[1]->status;
			}
			break;

		case OR_COMMAND:
			// execute first command
			execute_command(c->u.command[0], time_travel);
			c->status = c->u.command[0]->status;

			// execute second if first failed
			if (c->status)
			{
				execute_command(c->u.command[1], time_travel);
				c->status = c->u.command[1]->status;
			}
			break;

		case SUBSHELL_COMMAND:
			execute_command(c->u.subshell_command, time_travel);
			c->status = c->u.subshell_command->status;
			break;

		case SEQUENCE_COMMAND:
			execute_command(c->u.command[0], time_travel);
			execute_command(c->u.command[1], time_travel);
			c->status = c->u.command[1]->status;
			break;

		case PIPE_COMMAND:
			// initialize pipe
			if (pipe(fd) == -1)
				error(3, 0, "Cannot create pipe.");

			// create child to execute commands
			child = fork();
			if (child == 0) // child
			{
				close(fd[r]); // close read end
				
				// redirect standard output to read end of pipe
				if (dup2(fd[w], w) == -1)
					error(3, 0, "Cannot write to pipe.");

				// execute first command
				execute_command(c->u.command[0], time_travel);
				c->status = c->u.command[0]->status;
				
				close(fd[w]); // close write end
				
				exit(0);
			}
			else if (child > 0) // parent
			{
				// wait for the child process
				int status;
				waitpid(child, &status, 0);

				close(fd[w]); // close write end
				
				// redirect standard input to write end of pipe
				if (dup2(fd[r], r) == -1)
					error(3, 0, "Cannot read from pipe.");

				// execute second command
				execute_command(c->u.command[1], time_travel);
				c->status = c->u.command[1]->status;
				
				close(fd[r]); // close read end
			}
			else
				error(3, 0, "Cannot create child process.");

			break;

		default:
				error(3, 0, "Command type not recognized.");
			break;
	}

	return ;
}

// Executes command_stream with time travel parallelism.
int execute_time_travel (command_stream_t command_stream)
{	
	while (command_stream != NULL)
	{
		command_stream_t list = NULL;
		command_stream_t list_curr = NULL;
		command_stream_t prev = NULL;
		command_stream_t curr = command_stream;
		
		while (curr != NULL)
		{
			// add to list if empty, should be first command in stream
			if (list == NULL)
			{
				list = curr;
				list_curr = curr;
				command_stream = curr->next;
				curr = curr->next;
			}
			// add to list if no dependencies with list
			else if (is_dependent(curr->depends, list) == 0)
			{
				// if start of command stream
				if (prev == NULL)
				{
					list_curr->next = curr;
					list_curr = curr;
					command_stream = curr->next;
					curr = curr->next;
				}
				else
				{
					list_curr->next = curr; 
					list_curr = curr;
					prev->next = curr->next;
					curr = curr->next;
				}
			}
			else
			{
				prev = curr;
				curr = curr->next;
			}
			
			list_curr->next = NULL;
		}
	
		// TODO: execute ready list, currently not concurrent
		command_t command;
		while ((command = read_command_stream(list)))
		{
			execute_command(command, 1);
		}
	}
	
	return 0;
}
