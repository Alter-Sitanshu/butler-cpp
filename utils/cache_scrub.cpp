#include <cstddef>
#include <cstdio>
#include <cstring>
#include <linux/limits.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "../include/cache_scrub.hpp"
#include "../include/parser.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <limits.h>


bool acceptConfirmation() {
    std::printf(" Confirm [y/Y/n/N]: ");
    char resp;
    int _ = std::scanf(" %c", &resp);

    char temp;
    while((temp = getchar()) != '\n' && temp != EOF);

    return resp == 'y' || resp == 'Y';
};


void prune(const Args& args) {
    
    std::vector<std::string> paths = {
        "/build", "/*/__pycache__", "/__pycache__",  "/node_modules", "/venv", "/env",
        ".cache"
    };

    // atleast one path must be provided
    assert(paths.size() > 0);


    bool toConfirm = args.getBool("confirm");
    
    pid_t child_proc = fork();

    if (child_proc < 0) {
        // fork failed for this process
        throw std::runtime_error("cannot execute task, fork failed\n");
    
    } else if (child_proc == 0) {
        // Inside the child process context environment
        std::printf("Executing sub command for task: prune\n");
        size_t N = paths.size();

        char** args = new char*[N + 3];   // the extra 3 char* for rm --recursive *paths NULL
        
        char buffer[PATH_MAX];
        if (getcwd(buffer, sizeof(buffer)) != nullptr) {
            std::printf("CWD: %s\n", buffer);
        } else {
            std::fprintf(stderr, "getcwd() error\n");
            throw std::runtime_error("unable to get current working directory");
        }

        std::string cwd(buffer); 

        args[0] = const_cast<char*>("rm"); 
        args[1] = const_cast<char*>("-r");

        bool earlyTerm = true;

        for (size_t i = 0; i < N; ++i) {
            std::string sv(cwd + paths[i]);

            if (toConfirm) {
                std::printf("Delete %s ?", sv.c_str());
                bool resp = acceptConfirmation();
                // ignore the folder if the user decides not to delete it !! RESPECT
                if (!resp) continue;
            }

            earlyTerm = false;
            args[i+2] = strdup(sv.c_str());
        }

        args[N+2] = NULL;   // terminating null character
        
        if (earlyTerm) {
            // no directory was confirmed by the user so just ignore this
            for (size_t i = 0; i < N; ++i) {
                free(args[i + 2]);
            }
            delete[] args;
            return;
        }

        execvp(args[0], args);
        
        // If execvp fails, print it and terminate the child immediately
        std::perror("execvp failed");
        
        // Clean up duplicated string memory if exec failed
        for (size_t i = 0; i < N; ++i) {
            free(args[i + 2]);
        }
        delete[] args;
        
        std::_Exit(EXIT_FAILURE); 

    } else {
        // the parent sub comamnd process
        int status;
        waitpid(child_proc, &status, 0);
        std::printf("removed redundant folders, status: %d\n", status); 
    }

};



