#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h> 

#define buffer_length 256

int wrap(unsigned, int, int);

int main(int argc, char* argv[argc+1]){

	//convert string width to integer
	int w = atoi(argv[1]);

	//check that width is a valid number
	assert(w > 0);

	unsigned int width = (unsigned int)w; //convert width to unsigned int
	int file; //input
	struct stat sbuf;
	int dircheck;
	int check;
	int closed;

	//check if we need to read from standard input
	if(argc < 3){
		//read from standard input
		check = wrap(width, 0, 1);
	}
	else{
		//check if file or directory
		dircheck = stat(argv[2], &sbuf);
		if(dircheck != 0){
			perror("FILE NOT IN DIRECTORY");
		}

		//check if it is directory
		if(S_ISDIR(sbuf.st_mode)){
			DIR* dir = opendir(argv[2]);
			struct dirent* rdir;
			do{
				rdir = readdir(dir);
				if(rdir != NULL){

					//only wrap if it is a regular file
					if(rdir->d_type == 8){
						unsigned long namelen = strlen(rdir->d_name);
						char* filename = malloc(sizeof(char) * (namelen+5));

						//fill array with filename
						for(int i = 0; i < 5; i++){
							filename[i] = rdir->d_name[i];
						}
						int valid = strcmp(filename, "wrap.");

						if((rdir->d_name[0] != '.') && (valid != 0)){
							
							//file is valid to wrap, switch directories and open it
							int dirchange = chdir(argv[2]);
							if(dirchange != 0){
								perror("CANNOT CHANGE DIRECTORY");
							}

							file = open(rdir->d_name, O_RDONLY);
							if(file == -1){
								perror("CANNOT OPEN FILE");
							}

							//make destination file
							for(int i = 0; i < namelen+5; i++){
								if(i == 0){
									filename[i] = 'w';
								}
								else if(i == 1){
									filename[i] = 'r';
								}
								else if(i == 2){
									filename[i] = 'a';
								}
								else if(i == 3){
									filename[i] = 'p';
								}
								else if(i == 4){
									filename[i] = '.';
								}
								else{
									filename[i] = rdir->d_name[i-5];
								}
							}
							
							int filenew = open(filename, O_RDWR | O_TRUNC | O_CREAT, 0666);
							if(filenew == -1){
								perror("CANNOT CREATE DESTINATION FILE");
							}

							check = check + wrap(width, file, filenew);

							closed = close(filenew);
							if(closed != 0){
								perror("FILE NOT CLOSED\n");
							}
							closed = close(file);
							if(closed != 0){
								perror("FILE NOT CLOSED\n");
							}

							dirchange = chdir(".."); //go back to working directory

						}

						free(filename);
					}
				}

			}while(rdir != NULL);


			closed = closedir(dir);
			if(closed != 0){
				perror("DIRECTORY NOT CLOSED\n");
			}

   			
		}
		else{
			if(S_ISREG(sbuf.st_mode)){
				file = open(argv[2], O_RDONLY);
				if(file == -1){
					perror("CANNOT OPEN FILE");
				}
				check = wrap(width, file, 1);
				closed = close(file);
				if(closed != 0){
					perror("FILE NOT CLOSED\n");
				}
			}
		}	
	}
	
	if(check != 0){
		perror("ERROR: WORD EXCEEDS WIDTH\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
	
}

int wrap(unsigned width, int input_df, int output_fd){

	char buffer[buffer_length]; //buffer array
	char* word = malloc(sizeof(char) * 2); //word array
	char space[1]; //space array
	space[0] = ' ';
	char newln[1]; //new line array
	newln[0] = '\n';
	int len = 2; //variable length of word array
	ssize_t r; //buffer
	int new_line = 0; //true-false variable to track if first word on the line
	int word_length = 0; //variable length of current word
	int index = 0; //current index of word
	int newline_count = 0; //check to see if paragraph is needed
	int line = 0; //keeps track of how much of the width we have used
	int ret = 0; //return value


	//loop to move through file
	do{
		
		//read into buffer
		r = read(input_df, buffer, buffer_length);

		//write(1, buffer, r);

		//for loop to read each individual character into a char array
		for(int i=0; i<r; i++){
			
			//if not space
			if(isspace(buffer[i]) == 0){

				//check if we need to make a paragraph
				if(newline_count >= 2){
					write(output_fd, newln, 1);
					write(output_fd, newln, 1);
					new_line = 0;
					line = 0;
				}
				newline_count = 0;

				//add to new word array
				if(index < len){
					word[index] = buffer[i];
					index++;
					word_length++;
				}
				else{
					len = len*2;
					char *p = realloc(word, sizeof(char) * (len));
					if (!p) return 1;
					word = p;
					word[index] = buffer[i];
					index++;
					word_length++;
				}

			}
			else{  //if it is a space check different conditions

				//if there is word we need to print out
				if(word_length != 0){

					line = line + word_length;

					//if its not a new line
					if(new_line != 0){
						line++; //add one for space
						if(line > width){
							if(word_length > width){
								write(output_fd, newln, 1);
								write(output_fd, word, word_length);
								write(output_fd, newln, 1);
								new_line = 0;
								ret = -1;
								line = 0;
							}
							else{
								write(output_fd, newln, 1);
								write(output_fd, word, word_length);
								line = word_length;
							}
							word_length = 0;
							index = 0;
						}
						else{
							write(output_fd, space, 1);
							write(output_fd, word, word_length);
							word_length = 0;
							index = 0;
						}
					}
					else{
						if(line > width){
							write(output_fd, word, word_length);
							write(output_fd, newln, 1);
							word_length = 0;
							index = 0;
							new_line = 0;
							ret = -1;
							line = 0;
						}
						else{
							write(output_fd, word, word_length);
							word_length = 0;
							index = 0;
							new_line = 1;
						}
					}
				}
				
				//keep track of how many new line characters are outputted
				if(buffer[i] == '\n'){
					newline_count++;
				}
			}

		}

	}while(r != 0);

	//repeat of previous code block for the last line
	if(word_length != 0){
		line = line + word_length;

		//if its not a new line
		if(new_line != 0){
			line++; //add one for space
			if(line > width){
				if(word_length > width){
					write(output_fd, newln, 1);
					write(output_fd, word, word_length);
					write(output_fd, newln, 1);
					new_line = 0;
					ret = -1;
					line = 0;
				}
				else{
					write(output_fd, newln, 1);
					write(output_fd, word, word_length);
					write(output_fd, newln, 1);
					line = word_length;
				}
				word_length = 0;
				index = 0;
			}
			else{
				write(output_fd, space, 1);
				write(output_fd, word, word_length);
				write(output_fd, newln, 1);
				word_length = 0;
				index = 0;
			}
		}
		else{
			if(line > width){
				write(output_fd, word, word_length);
				write(output_fd, newln, 1);
				word_length = 0;
				index = 0;
				new_line = 0;
				ret = -1;
				line = 0;
			}
			else{
				write(output_fd, word, word_length);
				write(output_fd, newln, 1);
				word_length = 0;
				index = 0;
				new_line = 1;
			}
		}
	}

	free(word);

	//return condition
	return ret;

}