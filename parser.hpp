#pragma once

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <ios>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>
#include <type_traits>


#define MAX_HELPER_FMT_LIMIT 60

template <typename key1, typename key2, typename value>
struct DoubleKeyMap {
    std::unordered_map<key1, std::shared_ptr<value>> mkey1;
    std::unordered_map<key2, std::shared_ptr<value>> mkey2;

    void insert(const key1& k1, const value& val, const std::optional<key2> k2 = std::nullopt) {
        auto val_ptr = val.clone();
        
        this->mkey1[k1] = val_ptr;   
        if (k2.has_value()) {
            this->mkey2[*k2] = val_ptr;
        }
    };

    std::shared_ptr<value> search(const std::optional<key1> k1 = std::nullopt, const std::optional<key2> k2 = std::nullopt) {
        if (!k1.has_value() && !k2.has_value()) {
            // ensure atleast one key is set
            std::fprintf(
                stderr,
                "invalid search operation on double_key_map: atleast key value needed\n"
            );
            exit(1);
        }
        if (k1.has_value() && !k2.has_value()) {
            // Only Key1 is set and Key2 is missing
            if (this->mkey1.find(*k1) != this->mkey1.end()) {
                return this->mkey1[*k1];
            }
            return nullptr;
        } else if(!k1.has_value() && k2.has_value()) {
            if (this->mkey2.find(*k2) != this->mkey2.end()) {
                return this->mkey2[*k2];
            }

            return nullptr;
        } else {
            // Both keys are set so search for Both
            if (this->mkey2.find(*k2) != this->mkey2.end()) {
                return this->mkey2[*k2];
            } else if (this->mkey1.find(*k1) != this->mkey1.end()) {
                return this->mkey1[*k1];
            }
            return nullptr;
        }
    };

    auto begin() {
        return this->mkey1.begin();
    };
    auto end() {
        return this->mkey1.end();
    };
    auto begin() const {
        return this->mkey1.begin();
    };
    auto end() const {
        return this->mkey1.end();
    };

};


struct Args {
    std::unordered_map<std::string, std::string> flags;
    std::vector<std::string> pos_args;
    
    Args() {
        // By default the help flag is always initialised to "false"
        this->flags["help"] = "false";
    };

    bool getBool(const std::string& flag_name) const {
        auto f = flags.find(flag_name);
        if (f == flags.end()) {
            std::fprintf(
                stderr,
                "invalid flag name provided. please recheck\n"
            );
            exit(1);
        }

        return f->second == "true";
    };

    int getInt(const std::string& flag_name) const {
        auto f = flags.find(flag_name);
        if (f == flags.end()) {
            std::fprintf(
                stderr,
                "invalid flag name provided. please recheck\n"
            );
            exit(1);
        }
        int x;
        size_t inval_ix;
        try {
            x = std::stoi(f->second, &inval_ix);
            if (inval_ix != f->second.size()) {
                // there is trailing garbage in this expected int value
                std::fprintf(stderr, "invalid int argument, garbage value found: %s\n", f->second.c_str());
                exit(1);
            }
        } catch (std::invalid_argument& e) {
            std::fprintf(stderr, "error occured while getting flag value: %s", e.what());
            exit(1);
        } catch (std::out_of_range& e) {
            std::fprintf(stderr, "value too large to fit for int: %s\n", e.what());
            exit(1);
        }

        return x;
    };

    float getFloat(const std::string& flag_name) const {
        auto f = flags.find(flag_name);
        if (f == flags.end()) {
            std::fprintf(
                stderr,
                "invalid flag name provided. please recheck\n"
            );
            exit(1);
        }
        float x;
        size_t inval_ix;
        try {
           x = std::stof(f->second, &inval_ix);
           if (inval_ix != f->second.size()) {
                // there is trailing garbage
               std::fprintf(stderr, "invalid floating point value provided. gabage value found: %s\n", f->second.c_str());
               exit(1);
           }
        } catch (std::invalid_argument& e) {
            std::fprintf(stderr, "error occured while getting flag value: %s", e.what());
            exit(1);
        } catch (std::out_of_range& e) {
            std::fprintf(stderr, "value to large to fit into a float: %s\n", e.what());
        }
        return x;
    };
    
    std::string getStr(const std::string flag_name) const {
        auto f = flags.find(flag_name);
        if (f == flags.end()) {
            std::fprintf(
                stderr,
                "invalid flag name provided. please recheck\n"
            );
            exit(1);
        }

        return f->second;
    };

    const std::vector<std::string> getPosArgs() const {
        return this->pos_args;
    };

};



class FlagBase {
public:

    bool persistent = false;
    bool required = false;
    bool isboolean = false;
    std::string short_hand;
    std::string fname;
    std::string description;
    std::string default_val;

    virtual ~FlagBase() = default;

    virtual std::string getName() const = 0;
    
    virtual void parseVal(const std::string& str_val) = 0;
    virtual std::shared_ptr<FlagBase> clone() const = 0;

};


template <typename T>
class Flag : public FlagBase {
private:
    bool isValid(char c) {
        unsigned char uc = static_cast<unsigned char>(c);
        return std::isalpha(uc);
    };

public:

    ~Flag() = default;

    std::string getName() const override {
        return this->fname;
    };

    void parseVal(const std::string& str_val) override {
        std::istringstream ss(str_val);
        std::stringstream err_ss("type mismatch for flag argument of: "); err_ss << this->fname;
        T temp;
        if (this->isboolean) {
            if (!(ss >> std::boolalpha >> temp)) {
                throw std::invalid_argument(err_ss.str());
            } else {
                std::string remaining{};
                ss >> remaining;
                if (!remaining.empty()) {
                    throw std::invalid_argument(err_ss.str());
                }
            } 
        } else if constexpr (std::is_same_v<T, std::string>) {
          // do nothing  
        } else {

            if (!(ss >> temp)) {
                throw std::invalid_argument(err_ss.str());
            } else {
                std::string remaining{};
                ss >> remaining;
                if (!remaining.empty()) {
                    throw std::invalid_argument(err_ss.str());
                }
            }
        }
    };

    std::shared_ptr<FlagBase> clone() const override {
        return std::make_shared<Flag<T>>(*this);
    };

    
    // Boolean flag has to be optional | An optional argument has to have a default value | A required flag cannot have a default value
    // Cause ? You guessed it ! IT IS REQUIRED
    Flag (const std::string& name, const std::optional<std::string> default_value = std::nullopt ,bool persistent_ = false, bool optional_ = true,
            const std::string& short_name = "", const std::string& desc = ""
        ) {

        assert(!name.empty());
        bool is_bool = std::is_same_v<T, bool>;
        if (!optional_ && default_value.has_value()) {
            std::fprintf(
                stderr,
                "required flags should not have a default value set.\n" 
            );
            exit(1);
        }
        if (!is_bool && optional_ && !default_value.has_value()) {
            std::fprintf(
                stderr,
                "optional flags must have a default value set.\n" 
            );
            exit(1);
        }
        if (is_bool && !optional_) {
            std::fprintf(
                stderr,
                "boolean flag cannot be a required flag.\n" 
            );
            exit(1);
        }

        char name_start = name[0];
        if (!isValid((name_start))) {
            std::fprintf(
                stderr,
                "invalid flag name. name cannot start with digits, special characters and '-' and '--': got(%c)\n",
                name_start
            );
            exit(1);
        }
        this->fname = name;
        this->required = !optional_;
        this->isboolean = is_bool;
        this->persistent = persistent_;
        
        if (is_bool && !default_value.has_value()) {
            this->default_val = "false";
        } else if (!required) {      
            parseVal(this->default_val);
            this->default_val = default_value.value();
        } 
        this->short_hand = short_name;
        this->description = desc;
    };

    
};


using CmdFunc = std::function<void(const Args&)>;


/*
Command class acts as the Node to a Command tree structure with hierarchy from the root Command
The Flags identified are matched from the current node up till the root command: cause the root / global flags are for every child
For the Final leaf node of command it must have a handler to handle the exection that it aims to do -> if not raise error
*/
class Command {
public:
    Command* parent = nullptr;
    std::unordered_map<std::string, Command*> children;
    std::string name_;

    // std::unordered_map<std::string, Flag> registered_flags;
    
    // The first key is the full flag name and the second key is the shorthand notation of the flag
    // while inserting it is completely optional to provide the second key. but the first key is mandatory hence the decision
    DoubleKeyMap<std::string, std::string, FlagBase> registered_flags; 


    std::stringstream description;

    CmdFunc handler = nullptr;
    
    ~Command() = default;

    Command(std::string_view name, std::string desc = "") : name_(std::string(name)) , description(std::string(desc)) {};
    Command(Command* const parnt,  std::string_view name, std::string desc = "") {
        this->parent = parnt;
        this->name_ = std::string(name);
        this->description = std::stringstream(desc);
    };
    Command(std::string_view name, std::string desc, CmdFunc handler_func) {
        this->name_ = std::string(name);
        this->description = std::stringstream(desc);
        this->handler = handler_func;
    };


    // CAUTION: Must register your function before adding flags to it
    void addSubCmd(Command* const cmd) {
        cmd->parent = this;
        if (this->children.find(cmd->name_) != this->children.end()) {
            std::fprintf(
                stderr,
                "conflicting sub command name: %s under %s\n",
                cmd->name_.c_str(), this->name_.c_str()
            );
            exit(1);
        }
        this->children[cmd->name_] = cmd;
    };

    // CAUTION: Must register the command before adding flags to it. EXCEPT ROOT COMMAND
    void addFlag(const FlagBase& flag) {
        // 'flag' here will be deduces as Flag<bool>, Flag<int>, or Flag<float>
        Command* temp = this; 
        bool has_sfname = !flag.short_hand.empty();
        std::shared_ptr<FlagBase> fl (nullptr);
        while (temp) {
            if (!has_sfname) fl = temp->registered_flags.search(flag.fname); 
            else fl = temp->registered_flags.search(flag.fname, flag.short_hand);
            
            if (fl && (temp == this || fl->persistent)) {
                // If i found a matching flag, and if it is this command or a persisitent flag of a parent
                // It should not be allowed
                std::fprintf(
                    stderr,
                    "conflicting flag name %s in command %s\n", 
                    flag.fname.c_str(), this->name_.c_str()
                );
                exit(1);
            }
                
            temp = temp->parent;
        }
        if (!has_sfname) this->registered_flags.insert(flag.fname, flag);
        else this->registered_flags.insert(flag.fname,  flag, flag.short_hand);
       
    };

    // provided the command context it just prints the available flags, description by user
    void helpHandler() {
        char indent[5] = "    "; // indent of the output helper is formatted with an initial tab (4) character
        std::string word;

        std::printf("Description (%s)\n", this->name_.c_str());
        bool new_line = true;
        int line_len = 0;
        while (this->description >> word) {
            if (new_line) {
                new_line = false;
                std::printf("%s", indent);
            }
            
            if (line_len + word.size() + 1 <= MAX_HELPER_FMT_LIMIT) {
                std::printf("%s ", word.c_str());
                line_len += word.size();
            } else {
                std::printf("\n");
                line_len = 0;
                new_line = true;
            }
        }
        std::printf("\n");

        // List the first degree children of the command
        if (!this->children.empty()) std::printf("Available sub sommands :-\n");
        for (const auto& [scname, _] : this->children) {
            std::printf("%s- %s\n", indent, scname.c_str());
        }

        // List the registered flags under this command
        std::printf("Registered Flags :\n");
        for (const auto& [fname, flg] : this->registered_flags) {
            std::printf("%s--%s / (-%s) : %s\n", indent, flg->fname.c_str(), flg->short_hand.c_str(), flg->description.c_str());
            
        }

        // Check for any persistent flags too
        Command* temp = this->parent;
        while (temp) {
            for (const auto& [fname, flg] : temp->registered_flags) {
                if (flg->persistent) std::printf("%s--%s / (-%s) : %s\n", indent, flg->fname.c_str(), flg->short_hand.c_str(), flg->description.c_str());
            }
            temp = temp->parent;
        }
    };

};


// Makes a root command
Command RootCmd(std::string_view name, const std::string& desc);

// The Parsing and command tree constructor
Command* Parser(const std::vector<std::string_view>& args, Command* const root, Args& my_args);
