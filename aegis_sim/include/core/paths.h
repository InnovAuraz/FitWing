#pragma once
#include <string>
#include <vector>

namespace aegis {

// Folder that contains the running executable (always with forward slashes).
std::string exeDir();

// Finds a runtime folder (shaders, resources).  Search order:
//   <exe>/<name>, <cwd>/<name>, <exe>/../<name>, <exe>/../../<name>, <source dir>/<name>
// Returns "" if nothing is found.
std::string findRuntimeDir(const std::string& name);

// Reads a whole text file.  Returns false when the file cannot be opened.
bool readTextFile(const std::string& path, std::string& out);
bool fileExists(const std::string& path);
bool writeTextFileAtomic(const std::string& path, const std::string& text);

}  // namespace aegis
