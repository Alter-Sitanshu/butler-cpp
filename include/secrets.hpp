#include <regex>
#include <cstddef>
#include <vector>
#include <filesystem>

#include "parser.hpp"

#define BUF_SIZE 4096
#define MAX_BLOB_SIZE 5<<20     // 5MB max limit for the file

struct SecretRule {
    std::string name;
    std::regex pattern;
};

struct SecretFinding {
    std::string filepath;
    std::string rule;
    size_t line;
    std::string commit;
};

std::vector<SecretFinding> scanFile(const std::filesystem::path& filepath);
void scanSecrets(const Args& args);

