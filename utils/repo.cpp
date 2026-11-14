#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <linux/limits.h>
#include <stdexcept>
#include <string>
#include <vector>
#include "../include/repo.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>

#define ENV_EXT ".env"

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

// Check for push; use this before the first push to just check that you have done the 
// absolute basics of precautions before publishing the commits
void check_env(const Args& args) {
    
    std::string env_fname = args.getStr("env_file");
    assert ((env_fname.size() >= 4 && env_fname.substr(env_fname.size()-4, 4) == ENV_EXT) || "environment file is expected to have .env extension\n");

    std::string cwd;
    char buffer[PATH_MAX];
    if (getcwd(buffer, sizeof(buffer)) != nullptr) {
        cwd.assign(buffer);
    } else {
        fprintf(stderr, "error getting the cwd\n");
        throw std::runtime_error("error getting cwd\n");
    }

    std::fstream f_ignore(cwd + "/.gitignore", std::ios::in | std::ios::app);
    assert (f_ignore.is_open() || "file could not be opened\n");

    std::string line;

    bool has_ignored_env = false;
    while (std::getline(f_ignore, line)) {
        size_t L = line.size();
        if (L >= 4 && line.substr(L - 4, 4) == ENV_EXT) {
            has_ignored_env = true;
        }
    };

    // assert the reading of the entire file for moving forward
    assert (f_ignore.eof() || "error reading the .gitignore file. did not reach EOF");

    if (!has_ignored_env) {
        // append the .env ignoring tag in the .gitignore file
        f_ignore << "*.env";
    }

    // create a precaution file *.env.example
    std::string env_path(cwd + env_fname);
    std::fstream envf(env_path, std::ios::out);
    std::fstream eg_envf(env_path+".example", std::ios::out);
    while (getline(envf, line)) {
        size_t i = line.find_first_of('=');
        // By default i will just assume that all the env variables are strings and thus replace them with just quotes
        // Prompt the user to check the env example
        eg_envf << line.substr(0, i+1) << "=\"\"";
        std::printf("created: %s.example . Check after operations\n", env_path.c_str());
    }

    assert (envf.eof() || "error reading the env file. did not reach EOF\n");

    // defer the close of the file input stream
    f_ignore.close();
    envf.close();
    eg_envf.close();
};




