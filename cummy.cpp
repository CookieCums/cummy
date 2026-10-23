#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#include "project_scanner.hpp"

namespace cummy {
namespace {

namespace fs = std::filesystem;

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    std::string result;
    if (first != std::string::npos) {
        const auto last = value.find_last_not_of(" \t\r\n");
        result = value.substr(first, last - first + 1);
    }
    return result;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string normalizeName(const std::string& name) {
    std::string result;
    for (const unsigned char ch : name) {
        if (ch != '-' && ch != '_' && ch != '.') {
            result.push_back(static_cast<char>(std::tolower(ch)));
        }
    }
    return result;
}

const std::unordered_set<std::string>& standardModules() {
    static const std::unordered_set<std::string> modules = {
        "abc", "aifc", "argparse", "array", "ast", "asynchat", "asyncio", "asyncore",
        "atexit", "base64", "bdb", "binascii", "bisect", "builtins", "bz2", "calendar",
        "cgi", "cgitb", "chunk", "cmath", "cmd", "code", "codecs", "codeop", "collections",
        "colorsys", "compileall", "concurrent", "configparser", "contextlib", "contextvars",
        "copy", "copyreg", "cProfile", "crypt", "csv", "ctypes", "curses", "dataclasses",
        "datetime", "dbm", "decimal", "difflib", "dis", "doctest", "email", "encodings",
        "enum", "errno", "faulthandler", "fcntl", "filecmp", "fileinput", "fnmatch", "fractions",
        "ftplib", "functools", "gc", "getopt", "getpass", "gettext", "glob", "graphlib",
        "grp", "gzip", "hashlib", "heapq", "hmac", "html", "http", "imaplib", "importlib",
        "inspect", "io", "ipaddress", "itertools", "json", "keyword", "linecache", "locale",
        "logging", "lzma", "mailbox", "mailcap", "marshal", "math", "mimetypes", "mmap",
        "modulefinder", "multiprocessing", "netrc", "numbers", "operator", "optparse", "os",
        "pathlib", "pdb", "pickle", "pickletools", "pipes", "pkgutil", "platform", "plistlib",
        "poplib", "posix", "pprint", "profile", "pstats", "pty", "pwd", "py_compile", "pyclbr",
        "pydoc", "queue", "quopri", "random", "re", "readline", "reprlib", "resource", "rlcompleter",
        "sched", "secrets", "select", "selectors", "shelve", "shlex", "shutil", "signal", "site",
        "smtplib", "sndhdr", "socket", "socketserver", "sqlite3", "ssl", "stat", "statistics",
        "string", "stringprep", "struct", "subprocess", "sunau", "symtable", "sys", "sysconfig",
        "syslog", "tabnanny", "tarfile", "telnetlib", "tempfile", "termios", "textwrap", "threading",
        "time", "timeit", "tkinter", "token", "tokenize", "tomllib", "trace", "traceback", "tracemalloc",
        "tty", "turtle", "types", "typing", "unicodedata", "unittest", "urllib", "uuid", "venv",
        "warnings", "wave", "weakref", "webbrowser", "winreg", "winsound", "wsgiref", "xdrlib",
        "xml", "xmlrpc", "zipapp", "zipfile", "zoneinfo", "_thread"
    };
    return modules;
}

const std::unordered_map<std::string, std::string>& distributionNames() {
    static const std::unordered_map<std::string, std::string> names = {
        {"PIL", "Pillow"}, {"cv2", "opencv-python"}, {"yaml", "PyYAML"},
        {"sklearn", "scikit-learn"}, {"bs4", "beautifulsoup4"}, {"dotenv", "python-dotenv"},
        {"dateutil", "python-dateutil"}, {"Crypto", "pycryptodome"}, {"jwt", "PyJWT"},
        {"serial", "pyserial"}, {"win32com", "pywin32"},
        {"OpenSSL", "pyOpenSSL"}, {"magic", "python-magic"}, {"attr", "attrs"},
        {"gi", "PyGObject"}, {"lxml", "lxml"}, {"requests", "requests"}, {"numpy", "numpy"},
        {"pandas", "pandas"}, {"scipy", "scipy"}, {"matplotlib", "matplotlib"},
        {"flask", "Flask"}, {"django", "Django"}, {"fastapi", "fastapi"},
        {"sqlalchemy", "SQLAlchemy"}, {"pytest", "pytest"}, {"httpx", "httpx"}
    };
    return names;
}

std::string distributionForModule(const std::string& module) {
    const auto& names = distributionNames();
    const auto found = names.find(module);
    return found == names.end() ? module : found->second;
}

// ---------------------------------------------------------------------------
// Dependency accumulation
// ---------------------------------------------------------------------------

void addDependency(std::unordered_map<std::string, Dependency>& dependencies,
                   const std::string& requirement, Origin origin) {
    const std::string clean = trim(requirement);
    static const std::regex requirementPattern(
        R"(^([A-Za-z0-9][A-Za-z0-9._-]*)(?:\[[^\]]+\])?\s*(.*)$)");
    std::smatch match;
    if (!std::regex_match(clean, match, requirementPattern)) return;

    const std::string key = normalizeName(match[1].str());
    const auto found = dependencies.find(key);

    // New key, or upgrade Imported -> Declared. Never downgrade.
    const bool isUpgrade =
        found != dependencies.end() &&
        origin == Origin::Declared &&
        found->second.origin == Origin::Imported;

    if (found == dependencies.end() || isUpgrade) {
        dependencies[key] = Dependency{clean, origin};
    }
}

void addRequirementLine(std::unordered_map<std::string, Dependency>& dependencies,
                        std::string line, Origin origin) {
    line = trim(line);
    if (line.empty() || line[0] == '#') return;

    const auto comment = line.find(" #");
    if (comment != std::string::npos) line = trim(line.substr(0, comment));

    if (line.size() >= 2 &&
        ((line.front() == '"' && line.back() == '"') ||
         (line.front() == '\'' && line.back() == '\''))) {
        line = line.substr(1, line.size() - 2);
    }

    if (!line.empty() && line[0] != '-') addDependency(dependencies, line, origin);
}

// ---------------------------------------------------------------------------
// Manifest readers
// ---------------------------------------------------------------------------

void readPyproject(const fs::path& file,
                   std::unordered_map<std::string, Dependency>& dependencies,
                   std::vector<ScanReport::Problem>& problems) {
    std::ifstream input(file);
    if (!input) {
        std::error_code error;
        if (fs::exists(file, error)) {
            problems.push_back({file, "unable to read pyproject.toml"});
        }
        return;
    }

    bool projectSection = false;
    bool optionalDependenciesSection = false;
    bool readingList = false;
    std::string line;

    while (std::getline(input, line)) {
        const std::string clean = trim(line);
        if (!clean.empty() && clean.front() == '[') {
            projectSection = clean == "[project]" ||
                             clean.rfind("[project.optional-dependencies", 0) == 0;
            optionalDependenciesSection =
                clean.rfind("[project.optional-dependencies", 0) == 0;
            readingList = false;
        } else if (projectSection) {
            const bool relevantLine = readingList || optionalDependenciesSection ||
                                      clean.rfind("dependencies", 0) == 0;
            const auto equals = clean.find('=');
            if (relevantLine && (readingList || equals != std::string::npos)) {
                if (!readingList) {
                    readingList = clean.find('[', equals) != std::string::npos &&
                                  clean.find(']', equals) == std::string::npos;
                    line = clean.substr(equals + 1);
                }

                static const std::regex quotedValue(R"(["']([^"']+)["'])");
                for (std::sregex_iterator it(line.begin(), line.end(), quotedValue), end;
                     it != end; ++it) {
                    addRequirementLine(dependencies, (*it)[1].str(), Origin::Declared);
                }
                if (readingList && line.find(']') != std::string::npos) readingList = false;
            }
        }
    }
}

void readDeclaredDependencies(const fs::path& root,
                              std::unordered_map<std::string, Dependency>& dependencies,
                              std::vector<ScanReport::Problem>& problems) {
    std::error_code error;
    fs::directory_iterator it(root, error);
    if (error) {
        problems.push_back({root, "cannot read project directory: " + error.message()});
    } else {
        for (const auto& entry : it) {
            std::error_code entryError;
            if (!entry.is_regular_file(entryError) || entryError) continue;

            const std::string filename = lower(entry.path().filename().string());
            if (filename.rfind("requirements", 0) != 0) continue;
            if (entry.path().extension() != ".txt") continue;

            std::ifstream input(entry.path());
            if (!input) {
                problems.push_back({entry.path(), "unable to read requirements file"});
                continue;
            }
            std::string line;
            while (std::getline(input, line)) {
                addRequirementLine(dependencies, line, Origin::Declared);
            }
        }
    }

    readPyproject(root / "pyproject.toml", dependencies, problems);
}

// ---------------------------------------------------------------------------
// Python import scanner
// ---------------------------------------------------------------------------

bool isIgnoredDirectory(const fs::path& path, const ScannerOptions& options) {
    const std::string name = lower(path.filename().string());
    for (const auto& ignored : options.ignoredDirectories) {
        if (lower(ignored) == name) return true;
    }
    return false;
}

void addImportedModule(std::unordered_map<std::string, Dependency>& dependencies,
                       const std::string& module) {
    const auto dot = module.find('.');
    const std::string root = module.substr(0, dot);
    if (root.empty()) return;
    if (standardModules().find(root) != standardModules().end()) return;
    addDependency(dependencies, distributionForModule(root), Origin::Imported);
}

bool scanPythonFile(const fs::path& file,
                    std::unordered_map<std::string, Dependency>& dependencies) {
    std::ifstream input(file);
    if (!input) return false;

    // `lazy` (PEP 810, Python 3.15) is allowed as an optional prefix on both
    // `import` and `from ... import` statements. It is a soft keyword.
    static const std::regex fromPattern(
        R"(^\s*(?:lazy\s+)?from\s+([A-Za-z_][A-Za-z0-9_.]*)\s+import\b)");
    static const std::regex importPattern(
        R"(^\s*(?:lazy\s+)?import\s+([^#]+))");

    std::string line;
    std::smatch match;
    while (std::getline(input, line)) {
        if (std::regex_search(line, match, fromPattern)) {
            addImportedModule(dependencies, match[1].str());
            continue;
        }
        if (!std::regex_search(line, match, importPattern)) continue;

        std::string imports = match[1].str();
        std::size_t start = 0;
        while (start < imports.size()) {
            const auto comma = imports.find(',', start);
            std::string item = trim(imports.substr(
                start, comma == std::string::npos ? std::string::npos : comma - start));

            const auto alias = item.find(" as ");
            if (alias != std::string::npos) item = item.substr(0, alias);

            const auto whitespace = item.find_first_of(" \t");
            if (whitespace != std::string::npos) item.resize(whitespace);

            if (!item.empty()) addImportedModule(dependencies, item);

            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }
    return true;
}

std::size_t scanProject(const fs::path& root,
                        const ScannerOptions& options,
                        ScanReport& report,
                        std::unordered_map<std::string, Dependency>& dependencies) {
    std::size_t filesScanned = 0;
    std::error_code error;

    fs::directory_options iteratorOptions = fs::directory_options::skip_permission_denied;
    if (options.followSymlinks) {
        iteratorOptions |= fs::directory_options::follow_directory_symlink;
    }

    fs::recursive_directory_iterator it(root, iteratorOptions, error), end;
    while (it != end) {
        if (error) {
            report.problems.push_back({root, "scan error: " + error.message()});
            error.clear();
            it.increment(error);
            continue;
        }

        const auto& entry = *it;
        std::error_code entryError;

        if (entry.is_directory(entryError) && !entryError) {
            const bool ignored = isIgnoredDirectory(entry.path(), options);
            const bool tooDeep =
                options.maxDepth >= 0 && it.depth() >= options.maxDepth;
            if (ignored || tooDeep) it.disable_recursion_pending();
        } else if (entry.is_regular_file(entryError) && !entryError &&
                   lower(entry.path().extension().string()) == ".py") {
            if (scanPythonFile(entry.path(), dependencies)) {
                ++filesScanned;
                if (options.verbose) report.scannedFiles.push_back(entry.path());
            } else {
                report.problems.push_back({entry.path(), "unable to read file"});
            }
        }

        it.increment(error);
    }

    return filesScanned;
}

std::vector<Dependency> sortedDependencies(
    const std::unordered_map<std::string, Dependency>& dependencies) {
    std::vector<Dependency> result;
    result.reserve(dependencies.size());
    for (const auto& item : dependencies) result.push_back(item.second);
    std::sort(result.begin(), result.end(), [](const Dependency& left, const Dependency& right) {
        return lower(left.requirement) < lower(right.requirement);
    });
    return result;
}

// ---------------------------------------------------------------------------
// Virtual environment + shell integration helpers
// ---------------------------------------------------------------------------

#ifdef _WIN32
constexpr char kPathSeparator = ';';
#else
constexpr char kPathSeparator = ':';
#endif

bool stdinIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdin)) != 0;
#else
    return isatty(STDIN_FILENO) != 0;
#endif
}

bool stdoutIsTerminal() {
#ifdef _WIN32
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

std::string shellQuote(const std::string& value) {
    std::string result = "'";
    for (char ch : value) {
        if (ch == '\'') result += "'\\''";
        else result += ch;
    }
    result += "'";
    return result;
}

fs::path venvPython(const fs::path& venv) {
    std::error_code error;
    if (fs::is_regular_file(venv / "bin" / "python", error)) return venv / "bin" / "python";
    if (fs::is_regular_file(venv / "bin" / "python3", error)) return venv / "bin" / "python3";
    if (fs::is_regular_file(venv / "Scripts" / "python.exe", error)) return venv / "Scripts" / "python.exe";
    return {};
}

// Searches `start` and up to 3 ancestors for a venv-like directory.
fs::path findVenv(const fs::path& start) {
    std::error_code error;
    fs::path current = fs::absolute(start, error);
    if (error) return {};
    for (int depth = 0; depth < 4; ++depth) {
        for (const char* name : {".venv", "venv", "env"}) {
            fs::path candidate = current / name;
            if (fs::is_directory(candidate, error) && !venvPython(candidate).empty()) {
                return candidate;
            }
        }
        fs::path parent = current.parent_path();
        if (parent.empty() || parent == current) break;
        current = parent;
    }
    return {};
}

bool commandExists(const std::string& name) {
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv || !*pathEnv) return false;
    const std::string path(pathEnv);
    std::size_t start = 0;
    while (start <= path.size()) {
        const std::size_t end = path.find(kPathSeparator, start);
        std::string directory = path.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        if (directory.empty()) directory = ".";
        std::error_code error;
#ifdef _WIN32
        for (const char* extension : {".exe", ".bat", ".cmd"}) {
            if (fs::is_regular_file(fs::path(directory) / (name + extension), error)) return true;
        }
#else
        const fs::path base = fs::path(directory) / name;
        if (fs::is_regular_file(base, error)) {
            const auto permissions = fs::status(base, error).permissions();
            if ((permissions & (fs::perms::owner_exec | fs::perms::group_exec |
                                fs::perms::others_exec)) != fs::perms::none) {
                return true;
            }
        }
#endif
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

std::string findPythonCommand() {
    if (commandExists("python3")) return "python3";
    if (commandExists("python")) return "python";
    return {};
}

enum AutoMode { AutoAsk = 0, AutoAlways = 1, AutoNever = 2 };

fs::path settingsFile() {
    const char* home = std::getenv("HOME");
#ifdef _WIN32
    if (!home || !*home) home = std::getenv("USERPROFILE");
#endif
    if (!home || !*home) return {};
    return fs::path(home) / ".config" / "cummy" / "settings";
}

AutoMode readAutoMode() {
    const fs::path path = settingsFile();
    if (path.empty()) return AutoAsk;
    std::ifstream input(path);
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("auto_activate=", 0) == 0) {
            const std::string value = line.substr(14);
            if (value == "always") return AutoAlways;
            if (value == "never") return AutoNever;
        }
    }
    return AutoAsk;
}

void writeAutoMode(AutoMode mode) {
    const fs::path path = settingsFile();
    if (path.empty()) return;
    std::error_code error;
    fs::create_directories(path.parent_path(), error);
    std::ofstream output(path);
    if (!output) return;
    const char* value = mode == AutoAlways ? "always" : mode == AutoNever ? "never" : "ask";
    output << "auto_activate=" << value << "\n";
}

int promptActivate(const fs::path& venv) {
    std::cerr << "Found virtual environment: " << venv.string() << "\n"
              << "Activate it? [y]es / [n]o / [a]lways / ne[v]er: ";
    std::cerr.flush();
    std::string line;
    if (!std::getline(std::cin, line) || line.empty()) return 0;
    switch (std::tolower(static_cast<unsigned char>(line.front()))) {
        case 'y': return 1;
        case 'a': return 2;
        case 'v': return 3;
        default: return 0;
    }
}

void printActivation(const fs::path& venv, std::ostream& out) {
    const std::string path = venv.string();
    out << "if [ -z \"$VIRTUAL_ENV\" ] && [ -z \"${_CUMMY_SAVED_PATH+x}\" ]; then\n"
        << "    export _CUMMY_SAVED_PATH=\"$PATH\"\n"
        << "fi\n"
        << "export VIRTUAL_ENV=" << shellQuote(path) << "\n"
        << "export PATH=" << shellQuote(path + "/bin") << ":\"$PATH\"\n"
        << "unset PYTHONHOME\n";
}

void printDeactivation(std::ostream& out) {
    out << "if [ -n \"$VIRTUAL_ENV\" ]; then\n"
        << "    if [ -n \"${_CUMMY_SAVED_PATH+x}\" ]; then\n"
        << "        export PATH=\"$_CUMMY_SAVED_PATH\"\n"
        << "        unset _CUMMY_SAVED_PATH\n"
        << "    else\n"
        << "        __cummy_new=\"\"\n"
        << "        __cummy_ifs=\"$IFS\"\n"
        << "        IFS=\":\"\n"
        << "        for __cummy_p in $PATH; do\n"
        << "            case \"$__cummy_p\" in\n"
        << "                \"$VIRTUAL_ENV/bin\"|\"$VIRTUAL_ENV/Scripts\") ;;\n"
        << "                *) if [ -z \"$__cummy_new\" ]; then __cummy_new=\"$__cummy_p\"; "
        << "else __cummy_new=\"$__cummy_new:$__cummy_p\"; fi ;;\n"
        << "            esac\n"
        << "        done\n"
        << "        IFS=\"$__cummy_ifs\"\n"
        << "        export PATH=\"$__cummy_new\"\n"
        << "        unset __cummy_new __cummy_ifs __cummy_p\n"
        << "    fi\n"
        << "    unset VIRTUAL_ENV\n"
        << "    unset PYTHONHOME\n"
        << "fi\n";
}

bool samePath(const fs::path& left, const fs::path& right) {
    std::error_code errorLeft;
    std::error_code errorRight;
    const fs::path canonicalLeft = fs::weakly_canonical(left, errorLeft);
    const fs::path canonicalRight = fs::weakly_canonical(right, errorRight);
    return !errorLeft && !errorRight && canonicalLeft == canonicalRight;
}

} // namespace

// ---------------------------------------------------------------------------
// ProjectScanner::scan implementation
// ---------------------------------------------------------------------------

ScanReport ProjectScanner::scan(const fs::path& root,
                                const ScannerOptions& options) const {
    std::unordered_map<std::string, Dependency> dependencies;
    ScanReport report;

    readDeclaredDependencies(root, dependencies, report.problems);
    report.pythonFilesScanned = scanProject(root, options, report, dependencies);
    report.dependencies = sortedDependencies(dependencies);
    return report;
}

namespace {

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------

void printTerminalLogo() {
    std::cout << R"( /\_/\
( o.o )  Cummy
 > ^ <   Python dependency scanner

)";
}

void printHelp() {
    printTerminalLogo();
    std::cout << "Usage:\n"
              << "  cummy help\n"
              << "  cummy scan [PROJECT_DIR] [--path PROJECT_DIR] [--verbose]\n"
              << "  cummy gen dep [OUTPUT_FILE] [--path PROJECT_DIR]\n"
              << "  cummy gen install [OUTPUT_FILE] [--path PROJECT_DIR]\n"
              << "  cummy cum [--path PROJECT_DIR] [--shell] [--yes|--no]\n"
              << "  cummy uncum [--shell]\n"
              << "  cummy setup\n\n"
              << "Examples:\n"
              << "  cummy scan\n"
              << "  cummy scan --verbose\n"
              << "  cummy gen dep\n"
              << "  cummy gen install\n"
              << "  cummy cum --yes\n"
              << "  cummy uncum\n"
              << "  cummy setup\n\n"
              << "Shell integration:\n"
              << "  On Windows, `cummy cum` opens a shell with the nearest virtual\n"
              << "  environment activated; type `exit` to return.\n"
              << "  On Bash, Zsh, or Fish, `cummy setup` prints shell integration\n"
              << "  so `cummy cum` and `cummy uncum` work in the current shell.\n\n"
              << "The default output for `gen` is requirements.txt in the project directory.\n";
}

enum class CommandKind {
    Help, Scan, GenDep, GenInstall, Activate, Deactivate, Setup, Invalid
};

struct CommandLineOptions {
    CommandKind command = CommandKind::Help;
    bool shellMode = false;
    bool verbose = false;
    bool forceYes = false;
    bool forceNo = false;
    bool outputProvided = false;
    fs::path projectPath = fs::current_path();
    fs::path outputName = "requirements.txt";
    int errorCode = 0;
    std::string errorMessage;
};

void setCommandError(CommandLineOptions& options, int code, const std::string& message) {
    options.errorCode = code;
    options.errorMessage = message;
}

void parseProjectOptions(CommandLineOptions& options, int argc, char* argv[],
                         int start, bool allowOutput) {
    for (int index = start; index < argc && options.errorCode == 0; ++index) {
        const std::string argument = argv[index];
        if (argument == "--path" || argument == "-p") {
            if (index + 1 < argc) options.projectPath = argv[++index];
            else setCommandError(options, 2, "Missing directory after " + argument + ".");
        } else if (allowOutput && (argument == "--output" || argument == "-o")) {
            if (index + 1 < argc) {
                options.outputName = argv[++index];
                options.outputProvided = true;
            } else {
                setCommandError(options, 2, "Missing filename after " + argument + ".");
            }
        } else if (argument == "--shell") {
            options.shellMode = true;
        } else if (argument == "--verbose" || argument == "-v") {
            options.verbose = true;
        } else if (argument == "--yes" || argument == "-y") {
            options.forceYes = true;
        } else if (argument == "--no") {
            options.forceNo = true;
        } else if (!argument.empty() && argument[0] == '-') {
            setCommandError(options, 2, "Unknown option: " + argument);
        } else if (allowOutput && !options.outputProvided) {
            options.outputName = argument;
            options.outputProvided = true;
        } else if (!allowOutput) {
            options.projectPath = argument;
        } else {
            setCommandError(options, 2, "Unexpected argument: " + argument);
        }
    }
}

CommandLineOptions parseArguments(int argc, char* argv[]) {
    CommandLineOptions options;
    if (argc < 2) {
        options.command = CommandKind::Help;
        return options;
    }
    const std::string first = argv[1];
    if (first == "help" || first == "--help" || first == "-h") {
        options.command = CommandKind::Help;
    } else if (first == "scan") {
        options.command = CommandKind::Scan;
        parseProjectOptions(options, argc, argv, 2, false);
    } else if (first == "cum") {
        options.command = CommandKind::Activate;
        parseProjectOptions(options, argc, argv, 2, false);
    } else if (first == "uncum") {
        options.command = CommandKind::Deactivate;
        parseProjectOptions(options, argc, argv, 2, false);
    } else if (first == "setup") {
        options.command = CommandKind::Setup;
    } else if (first == "gen" && argc >= 3) {
        const std::string subcommand = argv[2];
        if (subcommand == "dep") {
            options.command = CommandKind::GenDep;
            parseProjectOptions(options, argc, argv, 3, true);
        } else if (subcommand == "install") {
            options.command = CommandKind::GenInstall;
            parseProjectOptions(options, argc, argv, 3, true);
        } else {
            setCommandError(options, 2, "Unknown gen subcommand: " + subcommand);
            options.command = CommandKind::Invalid;
        }
    } else {
        setCommandError(options, 2, "Unknown command. Run 'cummy help' for usage.");
        options.command = CommandKind::Invalid;
    }
    return options;
}

void handleSetup() {
    std::cout << "# Add this to ~/.bashrc or ~/.zshrc, then reload your shell:\n"
              << "cummy() {\n"
              << "    if [ \"$#\" -gt 0 ] && { [ \"$1\" = \"cum\" ] || [ \"$1\" = \"uncum\" ]; }; then\n"
              << "        local subcommand=\"$1\"\n"
              << "        shift\n"
              << "        eval \"$(command cummy \"$subcommand\" \"$@\")\"\n"
              << "    else\n"
              << "        command cummy \"$@\"\n"
              << "    fi\n"
              << "}\n"
              << "\n"
              << "# fish shell equivalent:\n"
              << "# function cummy\n"
              << "#     if test \"$argv[1]\" = cum; or test \"$argv[1]\" = uncum\n"
              << "#         set -l subcommand $argv[1]\n"
              << "#         set -e argv[1]\n"
              << "#         eval (command cummy $subcommand $argv)\n"
              << "#     else\n"
              << "#         command cummy $argv\n"
              << "#     end\n"
              << "# end\n";
}

#ifdef _WIN32
std::wstring quoteWindowsArgument(const std::wstring& argument) {
    std::wstring quoted = L"\"";
    size_t backslashes = 0;
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
        } else if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted += character;
            backslashes = 0;
        } else {
            quoted.append(backslashes, L'\\');
            quoted += character;
            backslashes = 0;
        }
    }
    quoted.append(backslashes * 2, L'\\');
    quoted += L'"';
    return quoted;
}

int runActivatedWindowsShell(const fs::path& venv) {
    std::wstring commandLine;
    const char* powershell = std::getenv("PSModulePath");
    if (powershell && *powershell) {
        const fs::path activationScript = venv / "Scripts" / "Activate.ps1";
        if (!fs::is_regular_file(activationScript)) {
            throw std::runtime_error("PowerShell activation script not found: " +
                                     activationScript.string());
        }
        std::wstring scriptPath = activationScript.wstring();
        std::wstring escapedPath;
        for (const wchar_t character : scriptPath) {
            escapedPath += character;
            if (character == L'\'') escapedPath += character;
        }
        const std::wstring activationCommand = L". '" + escapedPath + L"'";
        commandLine = quoteWindowsArgument(L"powershell.exe") +
                      L" -NoLogo -NoExit -ExecutionPolicy Bypass -Command " +
                      quoteWindowsArgument(activationCommand);
    } else {
        const fs::path activationScript = venv / "Scripts" / "activate.bat";
        if (!fs::is_regular_file(activationScript)) {
            throw std::runtime_error("Command Prompt activation script not found: " +
                                     activationScript.string());
        }
        const char* commandProcessor = std::getenv("COMSPEC");
        const std::wstring executable = commandProcessor && *commandProcessor
            ? fs::path(commandProcessor).wstring()
            : L"cmd.exe";
        commandLine = quoteWindowsArgument(executable) + L" /d /k \"\"" +
                      activationScript.wstring() + L"\"\"";
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0,
                        nullptr, nullptr, &startup, &process)) {
        throw std::runtime_error("Could not start an activated shell (Windows error " +
                                 std::to_string(GetLastError()) + ").");
    }

    CloseHandle(process.hThread);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, INFINITE);
    if (waitResult != WAIT_OBJECT_0) {
        const DWORD error = GetLastError();
        CloseHandle(process.hProcess);
        throw std::runtime_error("Could not wait for the activated shell (Windows error " +
                                 std::to_string(error) + ").");
    }

    DWORD shellExitCode = 0;
    if (!GetExitCodeProcess(process.hProcess, &shellExitCode)) {
        const DWORD error = GetLastError();
        CloseHandle(process.hProcess);
        throw std::runtime_error("Could not read the activated shell's exit code (Windows error " +
                                 std::to_string(error) + ").");
    }
    CloseHandle(process.hProcess);
    return static_cast<int>(shellExitCode);
}
#endif

void handleActivate(const CommandLineOptions& options, int& exitCode) {
    std::error_code error;
    const fs::path root = fs::absolute(options.projectPath, error);
    if (error || !fs::is_directory(root, error)) {
        std::cerr << "Project directory does not exist: " << root.string() << "\n";
        exitCode = 1;
        return;
    }

    const bool piped = !stdoutIsTerminal() || options.shellMode;

    const fs::path venv = findVenv(root);
    if (venv.empty()) {
        std::cerr << "No virtual environment found near " << root.string() << "\n";
        if (piped) std::cout << "# no virtual environment found\n";
        exitCode = 1;
        return;
    }

    const char* activeEnv = std::getenv("VIRTUAL_ENV");
    if (activeEnv && *activeEnv && samePath(fs::path(activeEnv), venv)) {
        std::cerr << "Virtual environment already active: " << venv.string() << "\n";
        if (piped) printActivation(venv, std::cout);
        return;
    }

    bool proceed = false;
    if (options.forceYes) {
        proceed = true;
    } else if (options.forceNo) {
        proceed = false;
    } else {
        const AutoMode mode = readAutoMode();
        if (mode == AutoAlways) {
            proceed = true;
        } else if (mode == AutoNever) {
            proceed = false;
#ifdef _WIN32
        } else if (!piped) {
            proceed = true;
#endif
        } else if (stdinIsTerminal()) {
            const int choice = promptActivate(venv);
            if (choice == 1) {
                proceed = true;
            } else if (choice == 2) {
                writeAutoMode(AutoAlways);
                proceed = true;
            } else if (choice == 3) {
                writeAutoMode(AutoNever);
                proceed = false;
            }
        }
    }

    if (!proceed) {
        if (piped) std::cout << "# activation skipped\n";
        return;
    }

    if (piped) {
        printActivation(venv, std::cout);
    } else {
#ifdef _WIN32
        std::cout << "Opening a shell with this virtual environment activated:\n"
                  << "  " << venv.string() << "\n"
                  << "Type exit to return to your previous shell.\n";
        exitCode = runActivatedWindowsShell(venv);
#else
        std::cout << "Virtual environment ready: " << venv.string() << "\n\n"
                  << "To activate it in your shell, run:\n"
                  << "  eval \"$(command cummy cum --shell --yes)\"\n\n"
                  << "For persistent shell integration, run:\n"
                  << "  cummy setup\n";
#endif
    }
}

void handleDeactivate(const CommandLineOptions& options, int& exitCode) {
    (void)exitCode;
    const char* activeEnv = std::getenv("VIRTUAL_ENV");
    const bool active = activeEnv && *activeEnv;

    if (!active) {
        std::cerr << "No virtual environment is currently active.\n";
    }

    const bool piped = !stdoutIsTerminal() || options.shellMode;
    if (piped) {
        printDeactivation(std::cout);
    } else if (active) {
#ifdef _WIN32
        std::cout << "Type exit to leave the shell with the virtual environment activated.\n";
#else
        std::cout << "Active virtual environment: " << activeEnv << "\n\n"
                  << "To deactivate in your shell, run:\n"
                  << "  eval \"$(command cummy uncum --shell)\"\n\n"
                  << "For persistent shell integration (recommended), run:\n"
                  << "  cummy setup\n";
#endif
    }
}

void handleGenInstall(const CommandLineOptions& options, int& exitCode) {
    std::error_code error;
    const fs::path root = fs::absolute(options.projectPath, error);
    if (error || !fs::is_directory(root, error)) {
        std::cerr << "Project directory does not exist: " << root.string() << "\n";
        exitCode = 1;
        return;
    }

    // 1. Scan + write requirements.
    ScannerOptions scanOptions;
    scanOptions.verbose = options.verbose;
    const ScanReport report = ProjectScanner{}.scan(root, scanOptions);

    const fs::path outputPath = options.outputName.is_absolute()
        ? options.outputName
        : root / options.outputName;

    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path(), error);
        if (error) {
            std::cerr << "Could not create output directory: " << error.message() << "\n";
            exitCode = 1;
            return;
        }
    }

    {
        std::ofstream output(outputPath);
        if (!output) {
            std::cerr << "Could not write dependency file: " << outputPath.string() << "\n";
            exitCode = 1;
            return;
        }
        for (const auto& dependency : report.dependencies) {
            output << dependency.requirement << '\n';
        }
    }
    std::cout << "Wrote " << outputPath.string() << " ("
              << report.dependencies.size() << " dependencies)\n";

    if (!report.problems.empty()) {
        std::cerr << "Encountered " << report.problems.size() << " scan problem(s):\n";
        for (const auto& problem : report.problems) {
            std::cerr << "  " << problem.path.string() << ": " << problem.message << "\n";
        }
    }

    if (report.dependencies.empty()) {
        std::cout << "No dependencies detected; nothing to install.\n";
        return;
    }

    // 2. Determine which venv to install into.
    fs::path venv;
    const char* activeEnv = std::getenv("VIRTUAL_ENV");
    if (activeEnv && *activeEnv && fs::is_directory(fs::path(activeEnv), error)) {
        venv = fs::path(activeEnv);
        std::cout << "Using active virtual environment: " << venv.string() << "\n";
    } else {
        venv = findVenv(root);
        if (!venv.empty()) {
            std::cout << "Using virtual environment: " << venv.string() << "\n";
        } else {
            bool create = false;
            if (stdinIsTerminal()) {
                std::cerr << "No virtual environment found. Create one at .venv? [Y/n]: ";
                std::cerr.flush();
                std::string line;
                create = true;
                if (std::getline(std::cin, line) && !line.empty()) {
                    create = std::tolower(static_cast<unsigned char>(line.front())) != 'n';
                }
            } else {
                create = (readAutoMode() == AutoAlways);
            }

            if (create) {
                const std::string python = findPythonCommand();
                if (python.empty()) {
                    std::cerr << "No python found in PATH; cannot create a venv.\n";
                } else {
                    const fs::path newVenv = root / ".venv";
                    const std::string command =
                        python + " -m venv " + shellQuote(newVenv.string());
                    std::cout << "Creating virtual environment: " << command << "\n";
                    if (std::system(command.c_str()) != 0) {
                        std::cerr << "Failed to create virtual environment.\n";
                    } else {
                        venv = newVenv;
                    }
                }
            }
        }
    }

    const fs::path venvPythonPath = venv.empty() ? fs::path() : venvPython(venv);

    // 3. Prefer uv when available.
    if (commandExists("uv")) {
        std::string command = "uv pip install -r " + shellQuote(outputPath.string());
        if (!venvPythonPath.empty()) {
            command += " --python " + shellQuote(venvPythonPath.string());
        }
        std::cout << "Running: " << command << "\n";
        exitCode = std::system(command.c_str()) == 0 ? 0 : 1;
        return;
    }

    // 4. Fall back to pip.
    std::string python = venvPythonPath.empty() ? findPythonCommand() : venvPythonPath.string();
    if (python.empty()) {
        std::cerr << "No python or uv found; cannot install dependencies.\n";
        exitCode = 1;
        return;
    }
    const std::string command =
        shellQuote(python) + " -m pip install -r " + shellQuote(outputPath.string());
    std::cout << "Running: " << command << "\n";
    exitCode = std::system(command.c_str()) == 0 ? 0 : 1;
}

void printProblemSummary(const std::vector<ScanReport::Problem>& problems) {
    if (problems.empty()) return;
    std::cerr << "Warning: " << problems.size() << " problem(s) during scan:\n";
    for (const auto& problem : problems) {
        std::cerr << "  " << problem.path.string() << ": " << problem.message << "\n";
    }
}

int runScanOrDep(const CommandLineOptions& options) {
    std::error_code error;
    const fs::path projectPath = fs::absolute(options.projectPath, error);
    const bool projectExists = !error && fs::is_directory(projectPath, error);
    if (!projectExists) {
        std::cerr << "Project directory does not exist: " << projectPath.string() << "\n";
        return 1;
    }

    ScannerOptions scanOptions;
    scanOptions.verbose = options.verbose;
    const ScanReport report = ProjectScanner{}.scan(projectPath, scanOptions);
    printProblemSummary(report.problems);

    if (options.command == CommandKind::Scan) {
        printTerminalLogo();
        std::cout << "Project: " << projectPath.string() << "\n"
                  << "Python files scanned: " << report.pythonFilesScanned << "\n"
                  << "Detected dependencies: " << report.dependencies.size() << "\n";
        for (const auto& dependency : report.dependencies) {
            std::cout << "  " << dependency.requirement << "\n";
        }
        if (options.verbose) {
            std::cout << "\nFiles scanned:\n";
            for (const auto& path : report.scannedFiles) {
                std::cout << "  " << path.string() << "\n";
            }
        }
        return 0;
    }

    const fs::path outputPath = options.outputName.is_absolute()
        ? options.outputName
        : projectPath / options.outputName;
    if (outputPath.has_parent_path()) {
        fs::create_directories(outputPath.parent_path(), error);
    }
    if (error) {
        std::cerr << "Could not create output directory: " << error.message() << "\n";
        return 1;
    }

    std::ofstream output(outputPath);
    if (!output) {
        std::cerr << "Could not write dependency file: " << outputPath.string() << "\n";
        return 1;
    }
    for (const auto& dependency : report.dependencies) {
        output << dependency.requirement << '\n';
    }
    output.close();
    if (!output) {
        std::cerr << "Failed while writing dependency file.\n";
        return 1;
    }

    printTerminalLogo();
    std::cout << "Scanned " << report.pythonFilesScanned
              << " Python file(s); found " << report.dependencies.size()
              << " dependency/dependencies.\n"
              << "Wrote " << outputPath.string() << "\n";
    return 0;
}

int run(int argc, char* argv[]) {
    const CommandLineOptions options = parseArguments(argc, argv);

    if (options.command == CommandKind::Help) {
        printHelp();
        return 0;
    }
    if (options.command == CommandKind::Invalid || options.errorCode != 0) {
        std::cerr << options.errorMessage << "\n";
        return options.errorCode != 0 ? options.errorCode : 2;
    }
    if (options.command == CommandKind::Setup) {
        handleSetup();
        return 0;
    }

    int exitCode = 0;
    switch (options.command) {
        case CommandKind::Activate:   handleActivate(options, exitCode);   break;
        case CommandKind::Deactivate: handleDeactivate(options, exitCode); break;
        case CommandKind::GenInstall: handleGenInstall(options, exitCode); break;
        default:                      exitCode = runScanOrDep(options);    break;
    }
    return exitCode;
}

} // namespace

} // namespace cummy

int main(int argc, char* argv[]) {
    int exitCode = 0;
    try {
        exitCode = cummy::run(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "Error: " << exception.what() << "\n";
        exitCode = 1;
    }
    return exitCode;
}