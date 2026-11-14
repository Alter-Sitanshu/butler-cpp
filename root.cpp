#include "./include/parser.hpp"
#include "./include/repo.hpp"
#include "./include/pdf.hpp"
#include "./include/secrets.hpp"
#include <string>
#include <string_view>

using namespace std;

int main(int argc, char* argv[]) {
    int N = argc - 1;
    Command root = MakeRootCmd("butler", "Butler is a dynamic CLI assistant that can be tweaked to cater various needs and requirements to automate and help in day to day redundant tasks. Just Plug and Build your custom Butler commands and functions to make it cater to your needs. Do not trust blindly !");
    
    Parser p = Parser(&root);
    
    root.addFlag(Flag<bool>(
        "verbose", "false", true, true, "v",
        "prints the steps decided by the tool while executing, inshort it just verbally declares what it is going to do"
    ));

    Command repoCmd = Command(
            "repo", 
            "This is a repository helper command family to clean/maintain/navigate the specified folder/repository", 
            nullptr,
            0
    ); 
    root.addSubCmd(&repoCmd);

    repoCmd.addFlag(Flag<bool>(
        "confirm", "false", true, true, "c",
        "This flag controls whether the tool confirms before deleting or pruning a directory/file."
    ));

    Command pruneRepoCmd = Command(
        "prune",
        "unneccesary build folders for languages"\
        "like NodeJS(node_modules), Python(__pycache__, env, venv), ./build/,\n"\
        "Use this function before pushing to github/gitlab to prevent any unwanted files," \
        "secrets, dirs leaking on the web.\n\n"\
        "Usage: butler prune_cache [path]\n",
        prune,
        0
    );

    repoCmd.addSubCmd(&pruneRepoCmd);
    
    Command pdfCmd = Command("pdf", "Offers help regarding pdf files and the related operations of merge, split, password protect and more");
    root.addSubCmd(&pdfCmd);
    Command mergeCmd = Command(
        "merge",
        "merges 2 or more pdf files and outputs the merged file\n\n Usage: butler pdf merge [...args] -o [output.pdf]",
        mergePDF 
    );

    mergeCmd.setEllipsis(true);
    pdfCmd.addSubCmd(&mergeCmd);

    mergeCmd.addFlag(Flag<std::string>(
        "output", "output_merge.pdf", false, true, "o",
        "This flag is used as the output file's name while saving"
    ));

    
    Command protectCmd = Command("protect", "Adds encryption to the specified pdf files. You can choose to accept single pair of passwords"\
            "for all the files, then you need to set the flag --all (-a), to indicate that intent. If you wish to choose permissions you can choose"\
            "from the available perms (default, copy, edit, print, access) [case sensitive]"\
            "example usage:\n"\
            "butler pdf protect --all file1.pdf file2.pdf file3.pdf\n"\
            "butler pdf protect --perm=copy file1.pdf",
            protectPDF
    );
    protectCmd.setEllipsis(true);
    pdfCmd.addSubCmd(&protectCmd);
    protectCmd.addFlag(Flag<bool>(
        "all", "false", false, true, "a", "If set accepts only one pair of passwords and encrypts all files with them"
    ));
    protectCmd.addFlag(Flag<std::string>(
        "perm", "default", false, true, "p", "Specifies the permissions allowed for the protected file"
    ));
    


    Command secretCmd = Command("secrets", "Scans the repository for potentially exposed SECRETS and hard coded API KEYS."\
            " By default it scans the staged"\
            "files. But this can be changed by toggling the --history boolean flag which tells the tool to search for exposed secrets also in the git commit"\
            "history of the repository",
            scanSecrets,
            0
    );
    repoCmd.addSubCmd(&secretCmd);
    secretCmd.addFlag(Flag<bool>(
        "history", "false", false, true, "", "Scan the git commit history"
    ));



    std::vector<std::string_view> args(N);
    for (int i = 1; i <= N; i++) {
        args[i-1] = std::string_view(argv[i]);
    }

    p.parse(args);

    return 0;
}
