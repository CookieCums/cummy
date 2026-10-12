#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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

void addDependency(std::unordered_map<std::string, Dependency>& dependencies,
                   const std::string& requirement, bool declared) {
    const std::string clean = trim(requirement);
    static const std::regex requirementPattern(
        R"(^([A-Za-z0-9][A-Za-z0-9._-]*)(?:\[[^\]]+\])?\s*(.*)$)");
    std::smatch match;
    if (std::regex_match(clean, match, requirementPattern)) {
        const std::string key = normalizeName(match[1].str());
        const auto found = dependencies.find(key);
        if (found == dependencies.end() || (declared && !found->second.declared)) {
            dependencies[key] = {clean, declared};
        }
    }
}

void addRequirementLine(std::unordered_map<std::string, Dependency>& dependencies,
                        std::string line, bool declared) {
    line = trim(line);
    if (!line.empty() && line[0] != '#') {
        const auto comment = line.find(" #");
        if (comment != std::string::npos) line = trim(line.substr(0, comment));
        if (line.size() >= 2 && ((line.front() == '"' && line.back() == '"') ||
                                 (line.front() == '\'' && line.back() == '\''))) {
            line = line.substr(1, line.size() - 2);
        }
        if (!line.empty() && line[0] != '-') addDependency(dependencies, line, declared);
    }
}

void readPyproject(const fs::path& file,
                   std::unordered_map<std::string, Dependency>& dependencies) {
    std::ifstream input(file);
    bool projectSection = false;
    bool optionalDependenciesSection = false;
    bool readingList = false;
    std::string line;
    if (input) {
        while (std::getline(input, line)) {
            const std::string clean = trim(line);
            if (!clean.empty() && clean.front() == '[') {
                projectSection = clean == "[project]" || clean.rfind("[project.optional-dependencies", 0) == 0;
                optionalDependenciesSection = clean.rfind("[project.optional-dependencies", 0) == 0;
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
                    for (std::sregex_iterator it(line.begin(), line.end(), quotedValue), end; it != end; ++it) {
                        addRequirementLine(dependencies, (*it)[1].str(), true);
                    }
                    if (readingList && line.find(']') != std::string::npos) readingList = false;
                }
            }
        }
    }
}

void readDeclaredDependencies(const fs::path& root,
                              std::unordered_map<std::string, Dependency>& dependencies) {
    std::error_code error;
    for (const auto& entry : fs::directory_iterator(root, error)) {
        if (error || !entry.is_regular_file()) continue;
        const std::string filename = lower(entry.path().filename().string());
        if (filename.rfind("requirements", 0) == 0 && entry.path().extension() == ".txt") {
            std::ifstream input(entry.path());
            std::string line;
            while (std::getline(input, line)) addRequirementLine(dependencies, line, true);
        }
    }
    readPyproject(root / "pyproject.toml", dependencies);
}

bool isIgnoredDirectory(const fs::path& path) {
    static const std::unordered_set<std::string> ignored = {
        ".git", ".hg", ".svn", ".venv", "venv", "env", "__pycache__", "node_modules",
        "build", "dist", "site-packages", ".tox", ".mypy_cache", ".pytest_cache"
    };
    return ignored.find(lower(path.filename().string())) != ignored.end();
}

void addImportedModule(std::unordered_map<std::string, Dependency>& dependencies,
                       const std::string& module) {
    const auto dot = module.find('.');
    const std::string root = module.substr(0, dot);
    if (!root.empty() && standardModules().find(root) == standardModules().end()) {
        addDependency(dependencies, distributionForModule(root), false);
    }
}

void scanPythonFile(const fs::path& file,
                    std::unordered_map<std::string, Dependency>& dependencies) {
    std::ifstream input(file);
    static const std::regex fromPattern(R"(^\s*from\s+([A-Za-z_][A-Za-z0-9_.]*)\s+import\b)");
    static const std::regex importPattern(R"(^\s*import\s+([^#]+))");
    std::string line;
    std::smatch match;
    if (input) {
        while (std::getline(input, line)) {
            if (std::regex_search(line, match, fromPattern)) {
                addImportedModule(dependencies, match[1].str());
            } else if (std::regex_search(line, match, importPattern)) {
                std::string imports = match[1].str();
                std::size_t start = 0;
                while (start < imports.size()) {
                    const auto comma = imports.find(',', start);
                    std::string item = trim(imports.substr(start, comma == std::string::npos ? comma : comma - start));
                    const auto alias = item.find(" as ");
                    if (alias != std::string::npos) item = item.substr(0, alias);
                    const auto whitespace = item.find_first_of(" \t");
                    if (whitespace != std::string::npos) item.resize(whitespace);
                    if (!item.empty()) addImportedModule(dependencies, item);
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
        }
    }
}

std::size_t scanProject(const fs::path& root,
                        std::unordered_map<std::string, Dependency>& dependencies) {
    std::size_t filesScanned = 0;
    std::error_code error;
    fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, error), end;
    while (it != end) {
        if (error) {
            error.clear();
            it.increment(error);
            continue;
        }
        const auto& entry = *it;
        if (entry.is_directory(error) && isIgnoredDirectory(entry.path())) {
            it.disable_recursion_pending();
        } else if (entry.is_regular_file(error) && lower(entry.path().extension().string()) == ".py") {
            scanPythonFile(entry.path(), dependencies);
            ++filesScanned;
        }
        it.increment(error);
    }
    return filesScanned;
}

std::vector<Dependency> sortedDependencies(const std::unordered_map<std::string, Dependency>& dependencies) {
    std::vector<Dependency> result;
    for (const auto& item : dependencies) result.push_back(item.second);
    std::sort(result.begin(), result.end(), [](const Dependency& left, const Dependency& right) {
        return lower(left.requirement) < lower(right.requirement);
    });
    return result;
}

} // namespace

ScanReport ProjectScanner::scan(const fs::path& root) const {
    std::unordered_map<std::string, Dependency> dependencies;
    readDeclaredDependencies(root, dependencies);

    ScanReport report;
    report.pythonFilesScanned = scanProject(root, dependencies);
    report.dependencies = sortedDependencies(dependencies);
    return report;
}

namespace {

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
              << "  cummy scan [--path PROJECT_DIR]\n"
              << "  cummy gen dep [OUTPUT_FILE] [--path PROJECT_DIR]\n\n"
              << "Examples:\n"
              << "  cummy scan\n"
              << "  cummy scan --path ..\\my-project\n"
              << "  cummy gen dep\n"
              << "  cummy gen dep customfile.txt --path ..\\my-project\n\n"
              << "The default output is requirements.txt in the project directory.\n";
}

struct CommandLineOptions {
    bool showHelp = false;
    bool scan = false;
    bool generate = false;
    bool outputProvided = false;
    fs::path projectPath = fs::current_path();
    fs::path outputName = "requirements.txt";
    int errorCode = 0;
    std::string errorMessage;
};

void setCommandError(CommandLineOptions& options, int errorCode, const std::string& message) {
    options.errorCode = errorCode;
    options.errorMessage = message;
}

CommandLineOptions parseArguments(int argc, char* argv[]) {
    CommandLineOptions options;
    if (argc < 2 || std::string(argv[1]) == "help" || std::string(argv[1]) == "--help" ||
        std::string(argv[1]) == "-h") {
        options.showHelp = true;
    } else {
        const std::string command = argv[1];
        options.generate = command == "gen" && argc >= 3 && std::string(argv[2]) == "dep";
        options.scan = command == "scan";
        if (!options.generate && !options.scan) {
            setCommandError(options, 2, "Unknown command. Run 'cummy help' for usage.");
        } else {
            const int firstOption = options.generate ? 3 : 2;
            for (int index = firstOption; index < argc && options.errorCode == 0; ++index) {
                const std::string argument = argv[index];
                if (argument == "--path" || argument == "-p") {
                    if (index + 1 < argc) {
                        options.projectPath = argv[++index];
                    } else {
                        setCommandError(options, 2, "Missing directory after " + argument + ".");
                    }
                } else if (argument == "--output" || argument == "-o") {
                    if (index + 1 < argc) {
                        options.outputName = argv[++index];
                        options.outputProvided = true;
                    } else {
                        setCommandError(options, 2, "Missing filename after " + argument + ".");
                    }
                } else if (!argument.empty() && argument[0] == '-') {
                    setCommandError(options, 2, "Unknown option: " + argument);
                } else if (options.generate && !options.outputProvided) {
                    options.outputName = argument;
                    options.outputProvided = true;
                } else if (options.scan) {
                    options.projectPath = argument;
                } else {
                    setCommandError(options, 2, "Unexpected argument: " + argument);
                }
            }
        }
    }
    return options;
}

int run(int argc, char* argv[]) {
    const CommandLineOptions options = parseArguments(argc, argv);
    int exitCode = 0;

    if (options.showHelp) {
        printHelp();
    } else if (options.errorCode != 0) {
        std::cerr << options.errorMessage << "\n";
        exitCode = options.errorCode;
    } else {
        std::error_code error;
        const fs::path projectPath = fs::absolute(options.projectPath, error);
        const bool projectExists = !error && fs::is_directory(projectPath, error);
        if (!projectExists) {
            std::cerr << "Project directory does not exist: " << projectPath.string() << "\n";
            exitCode = 1;
        } else {
            const ScanReport report = ProjectScanner{}.scan(projectPath);
            if (options.scan) {
                printTerminalLogo();
                std::cout << "Project: " << projectPath.string() << "\n"
                          << "Python files scanned: " << report.pythonFilesScanned << "\n"
                          << "Detected dependencies: " << report.dependencies.size() << "\n";
                for (const auto& dependency : report.dependencies) {
                    std::cout << "  " << dependency.requirement << "\n";
                }
            } else {
                const fs::path outputPath = options.outputName.is_absolute()
                    ? options.outputName
                    : projectPath / options.outputName;
                if (outputPath.has_parent_path()) fs::create_directories(outputPath.parent_path(), error);

                if (error) {
                    std::cerr << "Could not create output directory: " << error.message() << "\n";
                    exitCode = 1;
                } else {
                    std::ofstream output(outputPath);
                    if (!output) {
                        std::cerr << "Could not write dependency file: " << outputPath.string() << "\n";
                        exitCode = 1;
                    } else {
                        for (const auto& dependency : report.dependencies) {
                            output << dependency.requirement << '\n';
                        }
                        output.close();
                        if (!output) {
                            std::cerr << "Failed while writing dependency file.\n";
                            exitCode = 1;
                        } else {
                            printTerminalLogo();
                            std::cout << "Scanned " << report.pythonFilesScanned
                                      << " Python file(s); found " << report.dependencies.size()
                                      << " dependency/dependencies.\n"
                                      << "Wrote " << outputPath.string() << "\n";
                        }
                    }
                }
            }
        }
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