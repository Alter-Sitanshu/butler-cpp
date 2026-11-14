#pragma once

#ifndef REPO_HPP
#define REPO_HPP

#include "parser.hpp"

void prune(const Args& args);           // Butler prune cache folders or build folders
void check_env(const Args& args);       // Butler check for push(cfp)
void summarise(const Args& args);       // Summarises the entire repository
void check_secrets(const Args& args);   // Finds for potential secrets exposed in the current working tree














#endif

