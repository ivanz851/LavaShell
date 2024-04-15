#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>


#define MAX_ARGS_COUNT 20
#define MAX_ARG_LEN 100


struct Command {
    char** args; 
    char* fileIn;
    char* fileOut;

    Command() {
        args = (char**)malloc(MAX_ARGS_COUNT * sizeof(char*));
        for (int i = 0; i < MAX_ARGS_COUNT; ++i) {
            args[i] = (char*)malloc(MAX_ARG_LEN * sizeof(char));
        }

        fileIn = (char*)malloc(MAX_ARG_LEN * sizeof(char));
        fileIn[0] = '\0';

        fileOut = (char*)malloc(MAX_ARG_LEN * sizeof(char));
        fileOut[0] = '\0';
    }
};


bool filenameNotEmpty(char* filename) {
    return filename != NULL && filename[0] != '\0';
}


char* parseFileName(char*& ptr) {
    ++ptr;
    while (*ptr == ' ') {
        ++ptr;
    }

    char* fileName = (char*)malloc(MAX_ARG_LEN * sizeof(char));
    fileName[0] = '\0';
    size_t fileNameSymbInd = 0;

    bool escapeFlag = false;
    bool quoteOpenedFlag = false;

    while (*ptr != '\0' && *ptr != ' ') {
        char c = *ptr;
        
        if (escapeFlag) {
            fileName[fileNameSymbInd++] = c;
            escapeFlag = false;

            if (c == '"') {
                quoteOpenedFlag = !quoteOpenedFlag;
            }

            ++ptr;
            continue;
        }

        if (c == '\\') {
            escapeFlag = true;
            ++ptr;
            continue;
        }

        if (c == '"') {
            quoteOpenedFlag = !quoteOpenedFlag;
        } else {
            fileName[fileNameSymbInd++] = c;
        }

        ++ptr;
    }

    fileName[fileNameSymbInd++] = '\0';    
    return fileName;
}


void printParsedCmd(const Command& command) {
    int argsCnt = 0;
    for (; command.args[argsCnt] != NULL; ++argsCnt) {
    
    }

    std::cout << "argsCnt = " << argsCnt << std::endl;
    for (int i = 0; i < argsCnt; ++i) {
        std::cout << command.args[i] << std::endl;
    }
    if (filenameNotEmpty(command.fileOut)) {
        std::cout << "file Out = " << command.fileOut << std::endl;
    }

    if (filenameNotEmpty(command.fileIn)) {
        std::cout << "file In = " << command.fileIn << std::endl;
    }
}


int executeCmdInPipe(const Command& command, int in_fd, int out_fd) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        if (in_fd != STDIN_FILENO) {
            dup2(in_fd, STDIN_FILENO);
            close(in_fd);
        }
        if (out_fd != STDOUT_FILENO) {
            dup2(out_fd, STDOUT_FILENO);
            close(out_fd);
        }
        if (filenameNotEmpty(command.fileIn)) {
            int fd = open(command.fileIn, O_RDONLY);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        if (filenameNotEmpty(command.fileOut)) {
            int fd = open(command.fileOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(1);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        execvp(command.args[0], command.args);
        fprintf(stderr, "./lavash: line 1: %s: command not found\n", command.args[0]);
        exit(127);
    } else {
        int status;
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid");
            return -1;
        }
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return -1; 
        }
    }
}

bool check1984(const Command& command) {
    return command.args[0] != NULL && strcmp(command.args[0], "1984") == 0;
}


int executeSingleCmd(const Command& command) {
    if (check1984(command)) {
        return 0;
    }

    pid_t pid = fork();
    if (pid == 0) {
        if (filenameNotEmpty(command.fileIn)) {
            int fd = open(command.fileIn, O_RDONLY);
            if (fd == -1) {
                if (errno == ENOENT) {
                    fprintf(stderr, "./lavash: line 1: %s: No such file or directory\n", command.fileIn);
                } else {
                    perror("open");
                }
                exit(1);
            }

            if (dup2(fd, STDIN_FILENO) == -1) {
                perror("dup2");
                exit(1);
            }

            close(fd);
        }
        
        if (filenameNotEmpty(command.fileOut)) {
            int fd = open(command.fileOut, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open");
                exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                exit(1); 
            }

            close(fd);
        }
        
        execvp(command.args[0], command.args);

        fprintf(stderr, "./lavash: line 1: %s: command not found\n", command.args[0]);
        exit(127);
    }

    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        return exit_status;
    }

    return 1;
}


int executeCmdsUsingPipe(const std::vector<Command>& commands) {
    if (commands.empty()) {
        return 0;
    } else if (commands.size() == 1) {
        return executeSingleCmd(commands.front());
    } else  {
        std::vector<int[2]> pipes(commands.size() - 1);
        for (size_t i = 0; i < pipes.size(); ++i) {
            if (pipe(pipes[i]) == -1) {
                perror("pipe");
                return -1;
            }
        }

        int prevReadFd = STDIN_FILENO;
        int status = 0;
        for (size_t i = 0; i < commands.size(); ++i) {
            int nextWriteFd = (i + 1 == commands.size()) ? STDOUT_FILENO : pipes[i][1];
            status = 0;
            if (!check1984(commands[i])) {
                status = executeCmdInPipe(commands[i], prevReadFd, nextWriteFd);
            }

            close(pipes[i][1]);
            prevReadFd = pipes[i][0];
        }

        return status;
    }
}


Command parseCmd(char*& ptr) {
    Command command;

    size_t argInd = 0;
    size_t argSymbInd = 0;
    
    char* fileIn = NULL;
    char* fileOut = NULL;


    bool escapeFlag = false;
    bool quoteOpenedFlag = false;

    while (*ptr != '\0') {
        if (*ptr == '|' || *ptr == '&') {
            break;
        } else {
            char c = *ptr;

            if (escapeFlag) {
                command.args[argInd][argSymbInd++] = c;
                escapeFlag = false;

                if (c == '"') {
                    quoteOpenedFlag = !quoteOpenedFlag;
                }

                ++ptr;
                continue;
            }

            if (c == '\\') {
                escapeFlag = true;
                ++ptr;
                continue;
            }

            if (c == '<' && !quoteOpenedFlag) {
                fileIn = parseFileName(ptr);
            } else if (c == '>' && !quoteOpenedFlag) {
                fileOut = parseFileName(ptr);
            } else if (c == ' ' && !quoteOpenedFlag) {
                if (argSymbInd > 0) {
                    command.args[argInd][argSymbInd++] = '\0';

                    ++argInd;
                    argSymbInd = 0;
                }
            } else if (c == '"') {
                quoteOpenedFlag = !quoteOpenedFlag;
            } else {
                command.args[argInd][argSymbInd++] = c;
            }

            if (*ptr == '\0') {
                break;
            }
        }

        ++ptr;
    }
    if (argSymbInd > 0) {
        command.args[argInd][argSymbInd++] = '\0';

        ++argInd;
        argSymbInd = 0;
    }

    delete[] command.args[argInd];
    command.args[argInd] = NULL;
    ++argInd;

    delete[] command.fileOut;
    command.fileOut = fileOut;
    fileOut = NULL;

    delete[] command.fileIn;
    command.fileIn = fileIn;
    fileIn = NULL;

    return command;
}


int processInput(int argc, char* argv[]) {
    std::vector<Command> commands;
    char *ptr = argv[2];

    bool spoiledСonjunction = false;
    bool disjunctionFlag = false;

    while (*ptr != '\0') {
        Command command = parseCmd(ptr);
        commands.push_back(command);

        if (*ptr == '&') {
            if (!spoiledСonjunction) {
                int status = 0;
                status = executeCmdsUsingPipe(commands);
                if (status != 0) {
                    spoiledСonjunction = true;
                }
            }
            commands.clear();

            ++ptr;
            ++ptr;
            continue;
        } else if (*ptr == '|') {
            ++ptr;
            if (*ptr == '|') {
                disjunctionFlag = true;

                if (!spoiledСonjunction) {
                    int status = executeCmdsUsingPipe(commands);

                    if (status == 0) {
                        commands.clear();
                        return 0;
                    }
                }   
                commands.clear();
                spoiledСonjunction = false;

                    ++ptr;
                continue;
            } else {
                continue;
            }
        }
    }

    if (disjunctionFlag) {    
        if (spoiledСonjunction) {
            return 1;
        } else {
            int status = executeCmdsUsingPipe(commands);
            return status;
        } 
    } else {
        if (!spoiledСonjunction) {
            int status = executeCmdsUsingPipe(commands);
            if (status == 0) {
                return 0;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
    }
}


int main(int argc, char* argv[]) {
    if (argc > 1) {
        int status = processInput(argc, argv);
        return status;
    }

    return 0;
}
