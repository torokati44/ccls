#include "project.h"

#include "libclangmm/Utility.h"
#include "platform.h"
#include "utils.h"

#include <clang-c/CXCompilationDatabase.h>

#include <iostream>

namespace {
std::vector<CompilationEntry> LoadFromDirectoryListing(const std::string& project_directory) {
  std::vector<CompilationEntry> result;

  std::vector<std::string> args;
  for (const std::string& line : ReadLines(project_directory + "/clang_args")) {
    if (line.empty() || StartsWith(line, "#"))
      continue;
    std::cerr << "Adding argument " << line << std::endl;
    args.push_back(line);
  }


  std::vector<std::string> files = GetFilesInFolder(project_directory, true /*recursive*/, true /*add_folder_to_path*/);
  for (const std::string& file : files) {
    if (EndsWith(file, ".cc") || EndsWith(file, ".cpp") || EndsWith(file, ".c") || EndsWith(file, ".h")) {
      CompilationEntry entry;
      entry.filename = NormalizePath(file);
      entry.args = args;
      result.push_back(entry);
    }
  }

  return result;
}


// https://github.com/Andersbakken/rtags/blob/6b16b81ea93aeff4a58930b44b2a0a207b456192/src/Source.cpp
static const char *kValueArgs[] = {
  "--param",
  "-G",
  "-MF",
  "-MQ",
  "-MT",
  "-T",
  "-V",
  "-Xanalyzer",
  "-Xassembler",
  "-Xclang",
  "-Xlinker",
  "-Xpreprocessor",
  "-arch",
  "-b",
  "-gcc-toolchain",
  "-imacros",
  "-imultilib",
  "-include",
  "-iprefix",
  "-isysroot",
  "-ivfsoverlay",
  "-iwithprefix",
  "-iwithprefixbefore",
  "-o",
  "-target",
  "-x"
};
static const char *kBlacklist[] = {
  "--param",
  "-M",
  "-MD",
  "-MF",
  "-MG",
  "-MM",
  "-MMD",
  "-MP",
  "-MQ",
  "-MT",
  "-Og",
  "-Wa,--32",
  "-Wa,--64",
  "-Wl,--incremental-full",
  "-Wl,--incremental-patch,1",
  "-Wl,--no-incremental",
  "-fbuild-session-file=",
  "-fbuild-session-timestamp=",
  "-fembed-bitcode",
  "-fembed-bitcode-marker",
  "-fmodules-validate-once-per-build-session",
  "-fno-delete-null-pointer-checks",
  "-fno-use-linker-plugin"
  "-fno-var-tracking",
  "-fno-var-tracking-assignments",
  "-fno-enforce-eh-specs",
  "-fvar-tracking",
  "-fvar-tracking-assignments",
  "-fvar-tracking-assignments-toggle",
  "-gcc-toolchain",
  "-march=",
  "-masm=",
  "-mcpu=",
  "-mfpmath=",
  "-mtune=",
  "-s",

  //"-B",
  //"-f",
  //"-pipe",
  //"-W",
  "/",
  "..",
};

std::vector<CompilationEntry> LoadCompilationEntriesFromDirectory(const std::string& project_directory) {
  CXCompilationDatabase_Error cx_db_load_error;
  CXCompilationDatabase cx_db = clang_CompilationDatabase_fromDirectory(project_directory.c_str(), &cx_db_load_error);
  if (cx_db_load_error == CXCompilationDatabase_CanNotLoadDatabase) {
    std::cerr << "Unable to load compile_commands.json located at \"" << project_directory << "\"; using directory listing instead." << std::endl;
    return LoadFromDirectoryListing(project_directory);
  }

  CXCompileCommands cx_commands = clang_CompilationDatabase_getAllCompileCommands(cx_db);

  unsigned int num_commands = clang_CompileCommands_getSize(cx_commands);
  std::vector<CompilationEntry> result;
  for (unsigned int i = 0; i < num_commands; i++) {
    CXCompileCommand cx_command = clang_CompileCommands_getCommand(cx_commands, i);
    CompilationEntry entry;

    std::string directory = clang::ToString(clang_CompileCommand_getDirectory(cx_command));
    std::string relative_filename = clang::ToString(clang_CompileCommand_getFilename(cx_command));
    std::string absolute_filename = directory + "/" + relative_filename;
    entry.filename = NormalizePath(absolute_filename);

    unsigned int num_args = clang_CompileCommand_getNumArgs(cx_command);
    entry.args.reserve(num_args);
    for (unsigned int j = 0; j < num_args; ++j) {
      std::string arg = clang::ToString(clang_CompileCommand_getArg(cx_command, j));


      bool bad = false;
      for (auto& entry : kValueArgs) {
        if (StartsWith(arg, entry)) {
          bad = true;
          continue;
        }
      }
      if (bad) {
        ++j;
        continue;
      }


      for (auto& entry : kBlacklist) {
        if (StartsWith(arg, entry)) {
          bad = true;
          continue;
        }
      }
      if (bad) {
        continue;
      }


      if (StartsWith(arg, "-I")) {
        std::string path =  directory + "/" + arg.substr(2);
        path = NormalizePath(path);
        arg = "-I" + path;
      }

      entry.args.push_back(arg);

      //if (StartsWith(arg, "-I") || StartsWith(arg, "-D") || StartsWith(arg, "-std"))
    }


    result.push_back(entry);
  }

  clang_CompileCommands_dispose(cx_commands);
  clang_CompilationDatabase_dispose(cx_db);

  return result;
}
}  // namespace

void Project::Load(const std::string& directory) {
  entries = LoadCompilationEntriesFromDirectory(directory);
}

optional<CompilationEntry> Project::FindCompilationEntryForFile(const std::string& filename) {
  for (auto& entry : entries) {
    if (filename == entry.filename)
      return entry;
  }

  return nullopt;
}
