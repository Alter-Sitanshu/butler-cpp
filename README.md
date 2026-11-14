# Butler C++

Butler is a lightweight command-line utility written in C++ to help with common repository and document maintenance tasks. The project is built as a personal CLI assistant to automate repetitive operations while learning systems programming and low-level C++ development.

## Current features

### 1. Repository maintenance
- Remove common junk directories such as:
  - `node_modules`
  - `__pycache__`
  - `.venv` / `venv`
  - `build/`
- Intended for cleanup before committing or publishing code.

### 2. PDF utilities
- Merge multiple PDF files into one output file.
- Add encryption/protection to PDF files.
- Supports permission configuration for protected PDFs.
- Allows encrypting all selected files with a single password pair when needed.

### 3. Secret scanning
- Scan tracked files in the current repository for likely exposed secrets.
- Detect common credential patterns such as:
  - AWS access keys
  - GitHub tokens
  - Google API keys
  - Slack tokens
  - private keys
  - API key / secret assignment patterns
- Optional git history scan to look for secrets that may still exist in older commits.

### 4. CLI framework
- Nested command structure with flags and subcommands.
- Generic parser for building command-driven tools.
- Extensible design for adding new helper commands in the future.

## Command overview

The tool is exposed through a root command and subcommands such as:

- `butler repo prune`
- `butler pdf merge`
- `butler pdf protect`
- `butler repo secrets`

## Example usage

### Prune repository junk
```bash
./build/butler repo prune .
```

### Merge PDFs
```bash
./build/butler pdf merge file1.pdf file2.pdf -o merged.pdf
```

### Protect PDFs
```bash
./build/butler pdf protect --all file1.pdf file2.pdf
```

### Scan for secrets in current repo
```bash
./build/butler repo secrets
```

### Scan for secrets in git history
```bash
./build/butler repo secrets --history
```

## Build

```bash
make
```

This produces the executable at:

```bash
./build/butler
```

## Notes

This project is still evolving and is mainly being used as a practical learning project for C++ tooling, git-aware scanning, PDF processing, and CLI command design.
