#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char* argv[]){
	
	FILE *batch_file;
	char error_message[30] = "An error has occurred\n";
	char *path = malloc(5 * sizeof(char)); //"/bin" + '\0'
	strcpy(path, "/bin"); //bin is the default path directory
	char *buffer = NULL; 
	char *temp1, *temp2, *temp3;
	char *saveptr1, *saveptr2;
	char *token, *path_token, *raw_token;
	char *command_test;
	char *redirection_file;
	int shell_mode;
	int redirection_mode = 0;
	int stream;
	int command_success;
	int rc, process_counter, i;
	size_t size = 0;
	
	if (argc == 1)
		shell_mode = 0; //interactive mode
	else if (argc == 2){
		shell_mode = 1; //batch mode
		batch_file = fopen(argv[1], "r");
		if (!batch_file){
			write(STDERR_FILENO, error_message, strlen(error_message));
			exit(1);
		}
	}else{
		write(STDERR_FILENO, error_message, strlen(error_message)); //shell is opened with wrong numbers of parameters (>1)
		exit(1);
	}
	while (1){
		if (!shell_mode){ //interactive mode
			printf("\x1B[33mwish> \x1B[0m");
			getline(&buffer, &size, stdin);
		}else{ //batch mode
			if (getline(&buffer, &size, batch_file) == -1){
				fclose(batch_file);
				break;
			}
		}
		buffer[strlen(buffer)-1] = '\0'; //newline-character is replaced by null
		
		if (!strcmp(buffer, "exit")){
			free(buffer);
			exit(0);
		}
		temp1 = strndup(buffer, 2); //take first two characters from buffer (command)
		temp2 = strndup(buffer, 4); //take first four characters from buffer (command)
		
		if (!strcmp(temp1, "cd") && (buffer[2] == ' ' || strlen(buffer) == 2)){ // check if command´s first two charcters are "cd"
			strtok(buffer, " ");
			if (!(token = strtok(NULL, " ")))
				write(STDERR_FILENO, error_message, strlen(error_message)); //no arguments
			else{
				if (strtok(NULL, " "))
					write(STDERR_FILENO, error_message, strlen(error_message)); //too many arguments
				else{
					if (chdir(token)) //try to access the wanted directory
						write(STDERR_FILENO, error_message, strlen(error_message));
				}
			}
		}else if (!strcmp(temp2, "path") && (buffer[4] == ' ' || strlen(buffer) == 4)){ // check is command´s first four characters are "path"
			strtok(buffer, " ");
			free(path); //old path is freed
			path = NULL; 
			if ((token = strtok(NULL, " "))){
				path = malloc(strlen(token) * sizeof(char));
				strcpy(path, token); //first argument is added to path
				while ((token = strtok(NULL, " "))){ //next arguments is added to path
					path = realloc(path, (strlen(path) + 1 + strlen(token)) * sizeof(char));
					strcat(path, " ");
					strcat(path, token); 
				}
			}
		}else{
			if (!path)
				write(STDERR_FILENO, error_message, strlen(error_message)); //path is empty
			else{
				process_counter = 0;
				raw_token = strtok_r(buffer, "&", &saveptr1); //takes the first process with all it´s arguments.
				while (raw_token){ //while different processes exist
					token = strtok_r(raw_token, " ", &saveptr2); //takes the first argument, which is the command itself
					if (!token){
						raw_token = strtok_r(NULL, "&", &saveptr1);
						continue;
					}
					temp3 = strdup(path); //copy of the path
					path_token = strtok(temp3, " ");
					while (path_token){ //testing command with all path directories
						command_test = malloc((strlen(path_token) + 1 + strlen(token)) * sizeof(char));
						strcpy(command_test, path_token);
						strcat(command_test, "/");
						strcat(command_test, token);
						command_success = access(command_test, X_OK); //test if the command can be executed
						if (!command_success)
							break;
						free(command_test);
						path_token = strtok(NULL, " ");
					}
					free(temp3);
					if (command_success) 
						write(STDERR_FILENO, error_message, strlen(error_message)); //unknown operation
					else{ //if executable is found, create a child process
						rc = fork(); 	
						if (rc < 0){ //if fork failed	
							write(STDERR_FILENO, error_message, strlen(error_message));
							exit(1);
						}else if (rc == 0){ //child process
							char **args = NULL;  
							int counter = 1; 
							while (1){ //allocate needed amount of memory for all arguments inside args and put those arguments inside args
								args = realloc(args, counter * sizeof(char*));
								if (!token)
									break;
								else if (!strcmp(">", token)){ //if redirection is wanted
									redirection_file = strtok_r(NULL, " ", &saveptr2); //take the file name
									if (strtok_r(NULL, " ", &saveptr2)){ //if multiple arguments are found, exit the child process
										write(STDERR_FILENO, error_message, strlen(error_message)); //too many redirection arguments
										exit(1);
									}
									redirection_mode++;
									break;
								}
								args[counter-1] = token; 
								token = strtok_r(NULL, " ", &saveptr2);
								counter++;
							}
							args[counter-1] = NULL; //the last argument needs to be NULL
							if (redirection_mode){
								close(STDOUT_FILENO); //close the stdout-stream
								stream = open(redirection_file, O_CREAT|O_WRONLY|O_TRUNC, S_IRWXU); //open a file
								dup2(stream, fileno(stderr)); /* fileno(stderr) == STDERR_FILENO */ //create a copy of file stream and replace it with stderr
							}
							execv(command_test, args); //execute the command in child process
							write(STDERR_FILENO, error_message, strlen(error_message)); //oh no, execv returned
							exit(1);
						}else
							process_counter++;
					}
					raw_token = strtok_r(NULL, "&", &saveptr1); //move to the next command with it´s arguments
				}
				for (i = 0; i < process_counter; i++){
					//printf("%d waiting\n", i);
					waitpid(-1, NULL, 0); //wait for all the child processes to finish
				}
			}
		}
		free(temp1);
		free(temp2);
	}
	return 0;
}
