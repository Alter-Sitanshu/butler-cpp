#include "parser.hpp"
#include <cstdio>
#include <optional>
#include <string_view>


#define SP_C_POS "--"   // special character for positional arguments


// Function to make a root command
Command RootCmd(std::string_view name, const std::string& desc) {
    return Command(name, desc);
};

/*
This function parses the command line arguments and flags based on a command tree structure
Returns the final command ptr / ctx which the user can use to call the executable handler function
*/
Command* Parser(const std::vector<std::string_view>& args, Command* const root, Args& my_args) {  
    
    Command* curr = root;
    int N = args.size();
    if (N == 0) {
        // No arguments were passed. Trigger the default help command (print the description of root)
        my_args.flags["help"] = "true";
        return curr;
    }

    
    for (int i = 0; i < N; i++) {
        std::string_view s = args[i];
        // std::printf("%.*s\n", static_cast<int>(s.length()), s.data());
        if (s.size() == 0) {
            // Empty tokens will be skipped
            continue;
        }

        if (s.size() >= 2 && s[0] == '-') {
            // This is a flag token
            unsigned int l = 1;
            bool short_hand = true;
            if (s.size() > 2 && s[1] == '-') {
                // This is a long flag token
                l = 2;
                short_hand = false;
                while (l < s.size() && s[l] != '=') {
                    l++;
                }
                if (l == 2 || (s[s.size() - 1] == '=')) {
                    // Empty flag name Not allowed and last character assignment operator not allowed
                     std::fprintf(
                        stderr,
                        "invlaid flag type no name specified. '--' / '-=' / '--=' is an invalid way of flag initialisation\n"
                    );
                    exit(1);
                }
            } else if (s[1] != '-') {
                //  This is a short hand flag token
                if (s[1] == '=' || (s[s.size() - 1] == '=')) { 
                     std::fprintf(
                        stderr,
                        "invlaid flag type no name specified. '--' / '--=' / '-='  is an invalid way of flag initialisation\n"
                    );
                    exit(1);
                }
                while (l < s.size() && s[l] != '=') {
                    l++;
                }
            } else {
                // It has only 2 characters and both are '-' chars this is not allowed
                i++;
                // go to the next token, onwards everything is positional arguments
                while (i < N) {
                    std::string s {args[i]};
                    my_args.pos_args.push_back(s);
                    i++;
                }

                // after this the parsing loop ends
                return curr;
            }
            

            std::string flag_name { s.substr(short_hand ? 1 : 2, short_hand ? l-1 : l-2) };
            bool bool_flag = false;
            bool valid_flag = false;
            std::shared_ptr<FlagBase> flg;
            
            if (flag_name == "help" || (short_hand && flag_name == "h")) {
                // If the user enters the --help flag, no need to parse the arguments further
                // set the variables appropriately and return the current context command
                // the root program will execute the helperHandler of this current context command
                valid_flag = true;                
                my_args.flags["help"] = "true";
                bool_flag = true;
                return curr;
            } else if ((short_hand && curr->registered_flags.search(std::nullopt, flag_name)) || \
                    (!short_hand && curr->registered_flags.search(flag_name, std::nullopt))
                ) {
                // This flag belongs to the current context of command
                // No need to search up the parent tree
                valid_flag = true;
                flg = short_hand ? curr->registered_flags.search(std::nullopt, flag_name) : \
                                    curr->registered_flags.search(flag_name, std::nullopt);
                if (flg->isboolean) {
                    bool_flag = true;
                    my_args.flags[flg->fname] = "true";
                } else if (!flg->required) {
                    my_args.flags[flg->fname] = flg->default_val;
                }
            }

            // Go up the tree searching if the flag given is a valid registered flag name
            if (!valid_flag) {

                Command* temp = curr->parent;

                while (temp != nullptr) {
                    auto f = short_hand ? temp->registered_flags.search(std::nullopt, flag_name) : \
                                            temp->registered_flags.search(flag_name, std::nullopt);
                    if (!f || !f->persistent) {
                        temp = temp -> parent;
                    } else {
                        flg = f;
                        if (flg->isboolean) {
                            // no need to search for the value in the next token
                            // we can ignore the next token from considering it to be the flag value
                            bool_flag = true;
                            my_args.flags[flg->fname] = "true";
                        } else if (!flg->required) {
                            // we can look for potential next token to be the flag value 
                            my_args.flags[flg->fname] = flg->default_val;
                        }
                        break;
                    }
                }

                valid_flag = temp != nullptr;
            }

            if (!valid_flag) {
                // This is an invalid flag argument : guess what ? FLAG IT !!!
                std::fprintf(
                    stderr,
                    "invlaid flag name mentioned: %s\n", flag_name.c_str()
                );
                exit(1);
            }

            std::string flag_value = flg->default_val;
            
            // If I get to the end of the string, the value of the flag may be set in the next token
            // Or this is a boolen flag which i have to set it to true
            // For now lets assume the next token is the flags value
            if (l == s.size()) {
                if (i < N-1 && !flg->isboolean) {
                    std::string nxt_tok(args[++i]);
                    // by default it will consume the next token but if it a flag that is an error to FLAG !!
                    if (nxt_tok[0] == '-' || curr->children.find(nxt_tok) != curr->children.end()) {
                        // either this next token is flag in itself or a known sub command
                        std::fprintf(
                            stderr,
                            "flag %s expected a value. Error no value provided\n",
                            flag_name.c_str()
                        );
                        exit(1);
                    }

                    flag_value.assign(nxt_tok);
                } else if (flg->required) {
                    // Either this is a boolean flag or this is the last token
                    // But a boolean flag cannot be required. so this must be last and a required flag
                    std::fprintf(
                        stderr,
                        "error: required non-boolean flag (%s) is unset\n",
                        flag_name.c_str()
                    );
                    exit(1);
                }
            } else {
                // the flag's value is set after the = char in this token only
                if (bool_flag) {
                    // It is not allowed to assign values to a boolean flag
                    std::fprintf(
                        stderr,
                        "invlaid flag argument: boolean flags donot accept any values\n"
                    );
                    exit(1);
                }
                flag_value.assign(s.substr(l+1));
            }

            flg->parseVal(flag_value);
            // Only when the flag mentioned is not a boolean flag , it will have a potential flag_value set
            // No need to assign flag_value if it is boolean then. else i have checked for the flag's requirement
            // The next token is consumed as a value in all other else cases
            // At last if the required flag is at last, no value is there so a error is raised
            if (!bool_flag) my_args.flags[flg->fname] = flag_value; 
        } else if (!s.empty()) {
            auto sc = curr->children.find(std::string (s));
            if (sc != curr->children.end()) {
                // This is a sub command registered inside the current command
                // change / update the current command ctx
                curr = sc->second;
            } else {
                // It does not belong the registrered subcommands and thus should be treated as a positional argument
                my_args.pos_args.push_back(std::string(s));
            }
        }
    }
    // Final check for all the set flags that all the set flags are set
    for (const auto& [fname, f] : curr->registered_flags) {
        auto flg = my_args.flags.find(fname);

        if (f->required && flg == my_args.flags.end()) {
            std::fprintf(
                stderr,
                "required flag (%s) unset. expected a value\n",
                fname.c_str()
            );
            exit(1);
        } else if (!f->required && flg == my_args.flags.end()) {
            my_args.flags[fname] = f->default_val;
        }
    }
    Command* temp = curr->parent;

    while (temp) {
        for (const auto& [fname, f] : temp->registered_flags) {
            if (f->persistent){
                auto flg = my_args.flags.find(fname);
                if(f->required &&  flg == my_args.flags.end()) {
                    std::fprintf(
                        stderr,
                        "required flag (%s) unset. expected a value\n",
                        fname.c_str()
                    );
                    exit(1);
                } else if (!f->required && flg == my_args.flags.end()) {
                    my_args.flags[fname] = f->default_val;
                }
            }
        }

        temp = temp->parent;
    }


    return curr;
};
