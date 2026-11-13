#pragma once

#ifndef PARSER_HPP
#define PARSER_HPP

#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
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
#define SP_C_POS "--"   // special character for positional arguments

struct ParserError : public std::exception {
private:
    std::string message;

public:
    // Constructor to set a custom message
    explicit ParserError(const std::string& msg) : message(std::move(msg)) {}

    // 2. Override the what() method to return the error message
    const char* what() const noexcept override {
        return message.c_str();
    }
};


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
            throw ParserError("invalid search ket: atleat one key needed");
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

    bool empty() const {
        return this->mkey1.empty() && this->mkey2.empty();
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
            throw ParserError("invalid flag name provided. does not exist");
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
            throw ParserError("invalid flag name provided. does not exist");
        }
        int x;
        size_t inval_ix;
        try {
            x = std::stoi(f->second, &inval_ix);
            if (inval_ix != f->second.size()) {
                // there is trailing garbage in this expected int value
                std::fprintf(stderr, "invalid int argument, garbage value found: %s\n", f->second.c_str());
                throw ParserError("invalid integer argument: " + f->second);
            }
        } catch (std::invalid_argument& e) {
            std::fprintf(stderr, "error occured while getting flag value: %s", e.what());
            throw ParserError("error occured while getting flag value");
        } catch (std::out_of_range& e) {
            std::fprintf(stderr, "value too large to fit for int: %s\n", e.what());
            throw ParserError("value too large for integer. not supported");
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
            throw ParserError("invalid flag name provided");
        }
        float x;
        size_t inval_ix;
        try {
           x = std::stof(f->second, &inval_ix);
           if (inval_ix != f->second.size()) {
                // there is trailing garbage
               std::fprintf(stderr, "invalid floating point value provided. gabage value found: %s\n", f->second.c_str());
               throw ParserError("invalid float value: " + f->second);
           }
        } catch (std::invalid_argument& e) {
            std::fprintf(stderr, "error occured while getting flag value: %s", e.what());
            throw ParserError("error occured while getting flag value");
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
            throw ParserError("invalid flag name provided");
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
        std::stringstream err_ss; 
        err_ss << "type mismatch for flag argument of: " << this->fname;
        T temp;
        if constexpr (std::is_same_v<T, bool>) {
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
          return;
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
            throw ParserError("default flag not allowed for required flag: " + name);
        }
        if (!is_bool && optional_ && !default_value.has_value()) {
            std::fprintf(
                stderr,
                "optional flags must have a default value set.\n" 
            );
            throw ParserError("default value expected: received none for: " + name);
        }
        if (is_bool && !optional_) {
            std::fprintf(
                stderr,
                "boolean flag cannot be a required flag.\n" 
            );
            throw ParserError("boolean flag: " + name + " cannot be a required flag");
        }

        char name_start = name[0];
        if (!isValid((name_start))) {
            std::fprintf(
                stderr,
                "invalid flag name. name cannot start with digits, special characters and '-' and '--': got(%c)\n",
                name_start
            );
            throw ParserError("invalid flag name initialisation");
        }
        this->fname = name;
        this->required = !optional_;
        this->isboolean = is_bool;
        this->persistent = persistent_;
        
        if (is_bool && !default_value.has_value()) {
            this->default_val = "false";
        } else if (!required) {      
            parseVal(default_value.value());
            this->default_val = default_value.value();
        } 
        if (!short_name.empty()) assert (short_name.size() == 1); 

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
    
    std::string description;

    // An exact assertion on the number of expected positional argument is expected. So set this exactly to the number of
    // expected positional arguments only if the command expects any number of positional arguments this is relaxed
    uint8_t expected_pos_args = 0;
    bool ellipsis_args = false;
    
    CmdFunc handler = nullptr;
    
    ~Command() = default;

    Command(std::string_view name, std::string desc = "", uint8_t expected_pargs = 0) : 
        name_(std::string(name)) , description(std::string(desc)), expected_pos_args(expected_pargs) {};
    Command(std::string_view name, std::string desc, CmdFunc handler_func, uint8_t expected_pargs = 0) {
        this->name_ = std::string(name);
        this->description = desc;
        this->handler = handler_func;
        this->expected_pos_args = expected_pargs;
    };

    void setEllipsis(bool elp_arg) {
        this->ellipsis_args = elp_arg;
    };

    bool expectEllipsis() {
        return this->ellipsis_args;
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
            throw ParserError("sub command already exists: " + cmd->name_);
        }

        assert (cmd->registered_flags.empty());
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
                throw ParserError("conflicting flag name for sub command: " + this->name_);
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
        
        std::stringstream ss {this->description};

        while (ss >> word) {
            if (new_line) {
                new_line = false;
                std::printf("%s", indent);
                std::printf("%s ", word.c_str());
            }
            
            if (line_len + word.size() + 1 <= MAX_HELPER_FMT_LIMIT) {
                std::printf("%s ", word.c_str());
                line_len += word.size();
            } else {
                std::printf("%s", word.c_str());
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
            std::printf("%s--%s / (-%s) [%c]: %s\n", 
                    indent, flg->fname.c_str(), flg->short_hand.c_str(), flg->required ? 'R' : 'O', 
                    flg->description.c_str()); 
        }

        // Check for any persistent flags too
        Command* temp = this->parent;
        while (temp) {
            for (const auto& [fname, flg] : temp->registered_flags) {
                if (flg->persistent) std::printf("%s--%s / (-%s) [%c]: %s\n", 
                        indent, flg->fname.c_str(), flg->short_hand.c_str(), flg->required ? 'R' : 'O',
                        flg->description.c_str());
            }
            temp = temp->parent;
        }
    };

};


class Parser {
private:
    Command* root;
    Args args;

    std::shared_ptr<FlagBase> resolveFlag(Command* curr, const std::string& name, bool is_short) {
        auto flg = is_short ? curr->registered_flags.search(std::nullopt, name)
                            : curr->registered_flags.search(name, std::nullopt);
        if (flg) return flg;

        for (Command* temp = curr->parent; temp != nullptr; temp = temp->parent) {
            auto f = is_short ? temp->registered_flags.search(std::nullopt, name)
                            : temp->registered_flags.search(name, std::nullopt);
            if (f && f->persistent) {
                return f;
            }
        }
        return nullptr;
    }
    
    // read the CLI strings from argv and convert it to a vector of string_view arguments
    // This function parses the command line arguments and flags based on a command tree structure
    // Returns the final command ptr / ctx which the user can use to call the executable handler function
    Command* parseCLI(const std::vector<std::string_view>& str_args) {
        Command* curr = this->root;
        int N = str_args.size();
        if (N == 0) {
            // No arguments were passed. Trigger the default help command (print the description of root)
            this->args.flags["help"] = "true";
            return curr;
        }
    
        
        for (int i = 0; i < N; i++) {
            std::string_view s = str_args[i];
            // std::printf("%.*s\n", static_cast<int>(s.length()), s.data());
            if (s.size() == 0) {
                // Empty tokens will be skipped
                continue;
            }
    
            if (s.size() >= 2 && s[0] == '-') {
                // This is a flag token
                bool bool_flag = false;
                bool valid_flag = false;
                std::shared_ptr<FlagBase> flg;
                
                unsigned int l = 1;
                unsigned int x = 1;
                bool short_hand = true;
                if (s.size() > 2 && s[1] == '-') {
                    // This is a long flag token
                    // start scanning from the third char
                    l = 2; x = 2;
                    short_hand = false;
                    while (l < s.size() && s[l] != '=') {
                        l++;
                    }
                    if (l == 2 || (s[s.size() - 1] == '=')) {
                        // If the third char is '=' or the last char is '='
                        // Empty flag name Not allowed and last character assignment operator not allowed
                        std::fprintf(
                            stderr,
                            "invlaid flag type no name specified. '--' / '-=' / '--=' is an invalid way of flag initialisation\n"
                        );
                        throw ParserError("invalid name for flag specified");
                    }
                } else if (s[1] != '-') {
                    //  This is a short hand flag token
                    if (s[1] == '=' || (s[s.size() - 1] == '=')) { 
                        std::fprintf(
                            stderr,
                            "invlaid flag type no name specified. '--' / '--=' / '-='  is an invalid way of flag initialisation\n"
                        );
                        throw ParserError("invalid name for flag specified");
                    }
                    while (l < s.size() && s[l] != '=') {
                        l++;
                    }
    
                    // According to the rule, the short hand notation is limited to only size 1 string
                    // if l > 2, for example, -rf=foo, expands to -r (boolean flag) and -f=foo, at start l = 1(r), moves to l = 2(f)
                    // so at the end l = 3. hence there is concatted short flags
                    for (unsigned int m = 1; m < l-1; m++) {
                        // leave the last flag to be handled by the rest of the parser.
                        valid_flag = false;
                        std::string sflg_name{s[m]};
                        if (sflg_name == "h") {
                            valid_flag = true;
                            this->args.flags["help"] = "true";
                            bool_flag = true;
                            return curr;
                        } else {
                            flg = resolveFlag(curr, sflg_name, true);
                            if (flg){
                                valid_flag = true;
                                assert(flg->isboolean);
                                this->args.flags[flg->fname] = "true";
                            }
                        }
    
                        if (!valid_flag) {
                            std::fprintf(
                                stderr, 
                                "invalid flag name specified: %s", sflg_name.c_str()    
                            );
                            throw std::invalid_argument("invalid flag name specified");
                        }
    
                        x++; // increment to indicate that we have parsed this short flag
                    }
    
                    // at last the last short flag name is substr(x, l-x);
    
                } else {
                    // It has only 2 characters and both are '-' chars this is not allowed
                    i++;
                    // go to the next token, onwards everything is positional arguments
                    while (i < N) {
                        std::string s {str_args[i]};
                        this->args.pos_args.push_back(s);
                        i++;
                    }
    
                    // after this the parsing loop ends
                    return curr;
                }
                
    
                std::string flag_name { s.substr(x, l-x) };
                
                
                if (flag_name == "help" || (short_hand && flag_name == "h")) {
                    // If the user enters the --help flag, no need to parse the arguments further
                    // set the variables appropriately and return the current context command
                    // the root program will execute the helperHandler of this current context command
                    valid_flag = true;                
                    this->args.flags["help"] = "true";
                    bool_flag = true;
                    return curr;
                } else { 
                    // Go up the tree searching if the flag given is a valid registered flag
                    flg = resolveFlag(curr, flag_name, short_hand);
                    if (flg) {
                        valid_flag = true;
                        if (flg->isboolean) {
                            bool_flag = true;
                        } else if (!flg->required) {
                            this->args.flags[flg->fname] = flg->default_val;
                        }
                    }
                
                }
    
                if (!valid_flag) {
                    // This is an invalid flag argument : guess what ? FLAG IT !!!
                    std::fprintf(
                        stderr,
                        "invlaid flag name mentioned: %s\n", flag_name.c_str()
                    );
                    throw ParserError("invalid flag name mentioned: " + flag_name);
                }
    
                std::string flag_value = flg->isboolean ? "true" : flg->default_val;
                
                // If I get to the end of the string, the value of the flag may be set in the next token
                // Or this is a boolen flag which i have to set it to true
                // For now lets assume the next token is the flags value
                if (l == s.size()) {
                    if (i < N-1 && !flg->isboolean) {
                        std::string nxt_tok(str_args[i+1]);
                        // by default it will consume the next token but if it a flag that is an error to FLAG !!
                        if (nxt_tok[0] == '-' || curr->children.find(nxt_tok) != curr->children.end()) {
                            // either this next token is flag in itself or a known sub command
                            if (flg->required) {
                                // this is a required flag, do not consume this conflicting token
                                // throw error
                                std::fprintf(
                                    stderr,
                                    "flag %s expected a value. Error no value provided\n",
                                    flag_name.c_str()
                                );
                                throw std::invalid_argument("invalid flag value provided for required flag");
                            }
                            // the next token is conflicting, avoid consuming it as current flag's value
                        } else {
                            i++; // consume the next token
                            flag_value.assign(nxt_tok);
                        }
    
                    } else if (flg->required) {
                        // Either this is a boolean flag or this is the last token
                        // But a boolean flag cannot be required. so this must be last and a required flag
                        std::fprintf(
                            stderr,
                            "error: required non-boolean flag (%s) is unset\n",
                            flag_name.c_str()
                        );
                        throw ParserError("required non-boolean flag expects value: " + flag_name);
                    }
                } else {
                    // the flag's value is set after the = char in this token only
                    if (bool_flag) {
                        // It is not allowed to assign values to a boolean flag
                        std::fprintf(
                            stderr,
                            "invlaid flag argument: boolean flags donot accept any values\n"
                        );
                        throw ParserError("boolean flags donot accept values");
                    }
                    flag_value.assign(s.substr(l+1));
                }
    
                flg->parseVal(flag_value);
                this->args.flags[flg->fname] = flag_value; 
                // this is a valid flag value parsed. if the next token is consumed
                // only when this is a required, non boolean flag
                
            } else if (!s.empty()) {
                auto sc = curr->children.find(std::string (s));
                if (sc != curr->children.end()) {
                    // This is a sub command registered inside the current command
                    // change / update the current command ctx
                    curr = sc->second;
                } else {
                    // It does not belong the registrered subcommands and thus should be treated as a positional argument
                    if (!curr->children.empty()) {
                        throw std::runtime_error("unknown sub command name for: " + curr->name_ ); 
                    }
                    this->args.pos_args.push_back(std::string(s));
                }
            }
        }


        // Final check for all the set flags that all the set flags are set
        for (const auto& [fname, f] : curr->registered_flags) {
            auto flg = this->args.flags.find(fname);
    
            if (f->required && flg == this->args.flags.end()) {
                std::fprintf(
                    stderr,
                    "required flag (%s) unset. expected a value\n",
                    fname.c_str()
                );
                throw ParserError("expected a value for " + fname);
            } else if (!f->required && flg == this->args.flags.end()) {
                this->args.flags[fname] = f->default_val;
            }
        }
        Command* temp = curr->parent;
    
        while (temp) {
            for (const auto& [fname, f] : temp->registered_flags) {
                if (f->persistent){
                    auto flg = this->args.flags.find(fname);
                    if(f->required &&  flg == this->args.flags.end()) {
                        std::fprintf(
                            stderr,
                            "required flag (%s) unset. expected a value\n",
                            fname.c_str()
                        );
                        throw ParserError("expected a value for " + fname);
                    } else if (!f->required && flg == this->args.flags.end()) {
                        this->args.flags[fname] = f->default_val;
                    }
                }
            }
    
            temp = temp->parent;
        }

        // Final check for the required positional argument validation
        if (!curr->expectEllipsis()) assert( curr->expected_pos_args == this->args.pos_args.size());
        // we will assert exact matching of requirements for the positional arguments of the final command
    
        return curr;
    };

public:
    // Makes a root command
    
    // The Parsing and command tree constructor
    Parser(Command* const root) {
        this->root = root;
    };



    void parse(const std::vector<std::string_view>& str_args) {
        Command* exe_cmd = parseCLI(str_args);
        if (this->args.flags["help"] == "true") {
            exe_cmd->helpHandler();
        } else {
            assert (exe_cmd->handler != nullptr);
            exe_cmd->handler(this->args);
        }
    };

};


inline Command MakeRootCmd(std::string_view name, const std::string& desc) {
    return Command(name, desc);
};



#endif
