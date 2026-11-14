#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <sstream>

#include "../include/secrets.hpp"
                        

const std::vector<SecretRule>& getSecretRules() {
    static const std::vector<SecretRule> rules = {
        {
            "AWS Access Key",
            std::regex(R"(AKIA[0-9A-Z]{16})")
        },
        {
            "GitHub Token",
            std::regex(R"((ghp_|gho_|ghu_|ghs_|github_pat_)[A-Za-z0-9_]{20,})")
        },
        {
            "Google API Key",
            std::regex(R"(AIza[0-9A-Za-z_-]{35})")
        },
        {
            "Slack Token",
            std::regex(R"(xox[baprs]-[0-9A-Za-z-]{10,})")
        },
        {
            "Private Key",
            std::regex(R"(-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----)")
        },
        {
            "Generic Credential Assignment",
            std::regex(
                R"((api[_-]?key|secret[_-]?key|password|passwd|access[_-]?token)(?:\s*[:=]\s*|\s+)["']?[A-Za-z0-9_./+=-]{8,})",
                std::regex::icase
            )
        }
    };

    return rules;
}


std::string executeGit(const char* command);

namespace {

std::string shellQuote(const std::string& value) {
    if (value.empty()) {
        return "''";
    }

    static const std::string unsafe = " \t\n\r\'\"\\$&;|<>*?[](){}!#~`";
    if (value.find_first_of(unsafe) == std::string::npos) {
        return value;
    }

    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('\'');
    for (char ch : value) {
        if (ch == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted.push_back(ch);
        }
    }
    quoted.push_back('\'');
    return quoted;
}

std::vector<SecretFinding> scanContent(const std::filesystem::path& source, std::istream& input) {
    std::vector<SecretFinding> findings;
    std::string line;
    size_t lineNumber = 0;

    const std::vector<SecretRule>& rules = getSecretRules();

    while (std::getline(input, line)) {
        ++lineNumber;

        if (line.find('\0') != std::string::npos)
            break;

        for (const auto& rule : rules) {
            if (std::regex_search(line, rule.pattern)) {
                findings.push_back({
                    source.string(),
                    rule.name,
                    lineNumber
                });
            }
        }
    }

    return findings;
}

}

std::vector<SecretFinding> scanFile_H(const std::string& hash, const std::filesystem::path& filepath) {
    std::string gitRef = hash + ":" + filepath.generic_string();
    std::string cmd = "git show " + shellQuote(gitRef);

    try {
        std::string file_content = executeGit(cmd.c_str());
        std::istringstream contentStream(file_content);
        return scanContent(filepath, contentStream);
    } catch (const std::exception&) {
        return {};
    }
}


std::vector<SecretFinding> scanFile(const std::filesystem::path& filepath) {
    std::vector<SecretFinding> findings;

    std::ifstream file(filepath, std::ios::binary);

    if (!file.is_open()) {
        std::printf("Early exit for: %s\n", filepath.c_str());
        return findings;
    }

    return scanContent(filepath, file);
}

std::string executeGit(const char* command) {
    char buffer[BUF_SIZE];
    std::string output;

    FILE* pipe = popen(command, "r");

    if (!pipe)
        throw std::runtime_error("Failed to execute Git");

    size_t bytesRead;
    while ((bytesRead = fread(
        buffer, 1, BUF_SIZE, pipe
    )) > 0) {
        output.append(buffer, bytesRead);
    }

    int status = pclose(pipe);

    if (status != 0)
        throw std::runtime_error("Git command failed");

    return output;
}

std::vector<std::string> getTrackedFiles() {
    std::string output = executeGit(
        "git ls-files -z --full-name"
    );

    std::vector<std::string> files;

    size_t start = 0;

    while (start < output.size()) {
        // find the next occurance of \0 null char starting from start index
        size_t end = output.find('\0', start);

        // we reached the end the buffer string then break
        if (end == std::string::npos)
            break;
        // just take the substring from the current start to the found null char index
        files.push_back(output.substr(start, end - start));
        start = end + 1;    // move the start index forward
    }

    return files;
}


void scanHistory() {
    std::string hashes = executeGit("git rev-list --all");
    std::vector<std::string> hashv;
    std::istringstream hashStream(hashes);
    std::string hash;

    while (std::getline(hashStream, hash)) {
        if (!hash.empty()) {
            hashv.push_back(hash);
        }
    }

    std::vector<SecretFinding> findings;

    for (const auto& commit : hashv) {
        std::string treeOutput = executeGit(("git ls-tree -r --name-only " + commit).c_str());
        std::istringstream treeStream(treeOutput);
        std::string filePath;

        while (std::getline(treeStream, filePath)) {
            if (filePath.empty()) {
                continue;
            }

            auto fileFindings = scanFile_H(commit, filePath);
            for (auto& finding : fileFindings) {
                findings.push_back({
                    commit + ":" + finding.filepath,
                    finding.rule,
                    finding.line
                });
            }
        }
    }

    if (findings.empty()) {
        std::cout << "No potential secrets found in git history.\n";
        return;
    }

    for (const auto& finding : findings) {
        std::cout
            << "[!] " << finding.rule
            << " found in " << finding.filepath
            << ":" << finding.line << '\n';
    }

    std::cout << "\n"
              << findings.size()
              << " potential secrets found in git history.\n";
}


void scanSecrets(const Args& args) {
    // git rev-parse --show-toplevel : resolve the top level of the repository first
    std::string base_dir = executeGit("git rev-parse --show-toplevel");
    // trim the \n at last of the base_dir output from the command execution 
    base_dir = base_dir.substr(0, base_dir.size() - 1);
    

    bool checkHist = args.getBool("history");

    if (checkHist) return scanHistory();
    
    std::vector<std::string> files = getTrackedFiles();
    
    std::vector<SecretFinding> findings;

    for (const auto& fpath : files) {
        auto fileFindings = scanFile(
            base_dir + '/' + fpath
        );

        findings.insert(
            findings.end(),
            fileFindings.begin(),
            fileFindings.end()
        );
    }

    if (findings.empty()) {
        std::cout << "No potential secrets found.\n";
        return;
    }

    for (const auto& finding : findings) {
        std::cout
            << "[!] " << finding.rule
            << " found in " << finding.filepath
            << ":" << finding.line << '\n';
    }

    std::cout << "\n"
              << findings.size()
              << " potential secrets found.\n";  

}
