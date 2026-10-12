#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace cummy {

struct Dependency {
    std::string requirement;
    bool declared = false;
};

struct ScanReport {
    std::size_t pythonFilesScanned = 0;
    std::vector<Dependency> dependencies;
};

class ProjectScanner {
public:
    /**
     * Read root dependency manifests and statically scan Python imports below root.
     * Returns discovered dependencies in deterministic order.
     */
    [[nodiscard]] ScanReport scan(const std::filesystem::path& root) const;
};

}