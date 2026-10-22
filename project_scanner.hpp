#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace cummy {

enum class Origin {
    Declared,   // came from requirements*.txt / pyproject.toml
    Imported,   // inferred from `import x` / `from x import ...`
};

struct Dependency {
    std::string requirement;
    Origin origin = Origin::Imported;
};

struct ScannerOptions {
    std::vector<std::string> ignoredDirectories = {
        ".git", ".hg", ".svn", ".venv", "venv", "env", "__pycache__",
        "node_modules", "build", "dist", "site-packages",
        ".tox", ".mypy_cache", ".pytest_cache",
    };

    // -1 = unlimited. 0 = root's files only. 1 = root + direct subdirs.
    int maxDepth = -1;

    // When true, directory symlinks are traversed. Note: circular symlinks
    // are not detected and will loop.
    bool followSymlinks = false;

    // When true, ScanReport::scannedFiles is populated. Leave false for
    // large trees when you only need the dependency list.
    bool verbose = false;
};

struct ScanReport {
    std::size_t pythonFilesScanned = 0;
    std::vector<std::filesystem::path> scannedFiles;   // empty unless options.verbose
    std::vector<Dependency> dependencies;

    struct Problem {
        std::filesystem::path path;
        std::string message;
    };
    std::vector<Problem> problems;
};

class ProjectScanner {
public:
    /**
     * Read root dependency manifests and statically scan Python imports below root.
     * Returns discovered dependencies in deterministic (alphabetical) order.
     *
     * Unreadable files and malformed manifests are reported in `ScanReport::problems`
     * rather than silently skipped.
     */
    [[nodiscard]] ScanReport scan(
        const std::filesystem::path& root,
        const ScannerOptions& options = {}) const;
};

} // namespace cummy