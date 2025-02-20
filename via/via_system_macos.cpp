/*
 * Copyright (c) 2016-2021 Valve Corporation
 * Copyright (c) 2016-2021 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Mark Young <marky@lunarg.com>
 * Author: Jeremy Kniager <jeremyk@lunarg.com>
 */

#ifdef VIA_MACOS_TARGET

#include <cstring>
#include <sstream>
#include <algorithm>

#include <stdlib.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

#include "via_system_macos.hpp"

ViaSystemMacOS::ViaSystemMacOS() : ViaSystem() {
    uint32_t bufSize = 0;
    _NSGetExecutablePath(NULL, &bufSize);
    char temp_exe_path_c_str[bufSize];
    _NSGetExecutablePath(temp_exe_path_c_str, &bufSize);
    if (bufSize > 0) {
        std::string exe_location = temp_exe_path_c_str;
        _exe_path = exe_location.substr(0, exe_location.rfind("/"));
    } else {
        _exe_path = "";
    }
    char temp_c_string[1024];
    if (getcwd(temp_c_string, 1023) != NULL) {
        _cur_path = temp_c_string;
    } else {
        _cur_path = "";
    }
}

// Run the test in the specified directory with the corresponding
// command-line arguments.
// Returns 0 on no error, 1 if test file wasn't found, and -1
// on any other errors.
int ViaSystemMacOS::RunTestInDirectory(std::string path, std::string test, std::string cmd_line) {
    int err_code = -1;
    char orig_dir[1024];
    orig_dir[0] = '\0';

    LogInfo("       Command-line: " + cmd_line);

    if (NULL != getcwd(orig_dir, 1023)) {
        if (path.empty()) {
            // If the path is empty, check system paths.
            auto const which_query = "which " + test;
            err_code = system(which_query.c_str());
            if (err_code != 0) {
                LogWarning(test + " not found.  Skipping.");
            } else {
                err_code = system(cmd_line.c_str());
            }
        } else {
            int err = chdir(path.c_str());
            if (-1 != err) {
                if (-1 != access(test.c_str(), X_OK)) {
                    err_code = system(cmd_line.c_str());
                } else {
                    // Can't run because it's either not there or an actual
                    // exe.  So, just return a separate error code.
                    err_code = 1;
                    LogWarning(test + " not found.  Skipping.");
                }
            } else {
                // Path doesn't exist at all
                err_code = 1;
                LogWarning(test + " not found.  Skipping.");
            }
            err = chdir(orig_dir);
        }
    }
    return err_code;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemEnvironmentInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    char path[1035];
    char *env_value;
    utsname uts_buffer;

    PrintBeginTable("Environment", 3);

    FILE *fp = popen("sw_vers", "r");
    if (fp == NULL) {
        PrintBeginTableRow();
        PrintTableElement("ERROR");
        PrintTableElement("Failed to run sw_vers");
        PrintTableElement("");
        PrintEndTableRow();
        result = VIA_SYSTEM_CALL_FAILURE;
    } else {
        // Read the output a line at a time - output it.
        while (fgets(path, sizeof(path) - 1, fp) != NULL) {
            uint32_t index;
            index = strlen(path) - 1;
            while (path[index] == ' ' || path[index] == '\t' || path[index] == '\r' || path[index] == '\n' || path[index] == '\"') {
                path[index] = '\0';
                index = strlen(path) - 1;
            }
            if (NULL != strstr(path, "ProductName")) {
                index = strlen("ProductName:");
                while (path[index] == ' ' || path[index] == '\t' || path[index] == '\"') {
                    index++;
                }
                _os_name = &path[index];
                PrintBeginTableRow();
                PrintTableElement("MacOS");
                PrintTableElement("");
                PrintTableElement("");
                PrintEndTableRow();
                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement("Product Name");
                PrintTableElement(_os_name);
                PrintEndTableRow();
            } else if (NULL != strstr(path, "ProductVersion")) {
                index = strlen("ProductVersion:");
                while (path[index] == ' ' || path[index] == '\t' || path[index] == '\"') {
                    index++;
                }
                std::string _os_version = &path[index];
                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement("Product Version");
                PrintTableElement(_os_version);
                PrintEndTableRow();
            } else if (NULL != strstr(path, "BuildVersion")) {
                index = strlen("BuildVersion:");
                while (path[index] == ' ' || path[index] == '\t' || path[index] == '\"') {
                    index++;
                }
                std::string _build_version = &path[index];
                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement("Build Version");
                PrintTableElement(_build_version);
                PrintEndTableRow();
            }
        }
        pclose(fp);
    }

    errno = 0;
    if (uname(&uts_buffer) != 0) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("ERROR");
        PrintTableElement("Failed to query uname");
        PrintEndTableRow();
        result = VIA_SYSTEM_CALL_FAILURE;
    } else {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Kernel Build");
        PrintTableElement(uts_buffer.release);
        PrintEndTableRow();
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Machine Target");
        PrintTableElement(uts_buffer.machine);
        PrintEndTableRow();
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Version");
        PrintTableElement(uts_buffer.version);
        PrintEndTableRow();
    }

    env_value = getenv("DYLD_LIBRARY_PATH");
    if (env_value != NULL) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("DYLD_LIBRARY_PATH");
        PrintTableElement(env_value);
        PrintEndTableRow();
    }

    PrintEndTable();
    return result;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemHardwareInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    char generic_string[1024];
    uint64_t memory;
    struct statvfs fs_stats;

    PrintBeginTable("Hardware", 3);

    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    snprintf(generic_string, 1023, "%d", num_cpus);

    PrintBeginTableRow();
    PrintTableElement("CPUs");
    PrintTableElement(generic_string);
    PrintTableElement("");
    PrintEndTableRow();

    memory = (sysconf(_SC_PHYS_PAGES) * sysconf(_SC_PAGE_SIZE)) >> 10;
    if ((memory >> 10) > 0) {
        memory >>= 10;
        if ((memory >> 20) > 0) {
            snprintf(generic_string, 1023, "%u TB", static_cast<uint32_t>(memory >> 20));
        } else if ((memory >> 10) > 0) {
            snprintf(generic_string, 1023, "%u GB", static_cast<uint32_t>(memory >> 10));
        } else {
            snprintf(generic_string, 1023, "%u MB", static_cast<uint32_t>(memory));
        }
    } else {
        snprintf(generic_string, 1023, "%u KB", static_cast<uint32_t>(memory));
    }

    PrintBeginTableRow();
    PrintTableElement("Memory");
    PrintTableElement("Physical Available");
    PrintTableElement(generic_string);
    PrintEndTableRow();

    // Print system disk space usage
    if (0 == statvfs("/System/Library/CoreServices/SystemVersion.plist", &fs_stats)) {
        uint64_t bytes_total = (uint64_t)fs_stats.f_frsize * (uint64_t)fs_stats.f_bavail;
        std::string suffixes[5] = {"bytes", "Ki", "Mi", "Gi", "Ti"};
        for (size_t i = 0; i < 5; i++) {
            if (bytes_total < 1000 || i == 4) {
                snprintf(generic_string, 1023, "%u %s", static_cast<uint32_t>(bytes_total), suffixes[i].c_str());
                PrintBeginTableRow();
                PrintTableElement("System Disk Space");
                PrintTableElement("Free");
                PrintTableElement(generic_string);
                PrintEndTableRow();
                break;
            }
            bytes_total /= 1000;
        }
    }

    // Print current directory disk space info
    sprintf(generic_string, "df -h \'%s\' | awk \'{ print $4 } \' | tail -n 1", _cur_path.c_str());
    FILE *fp = popen(generic_string, "r");
    if (fp == NULL) {
        PrintBeginTableRow();
        PrintTableElement("Current Dir Disk Space");
        PrintTableElement("WARNING");
        PrintTableElement("Failed to determine current directory disk space");
        PrintEndTableRow();
    } else {
        PrintBeginTableRow();
        PrintTableElement("Current Dir Disk Space");
        PrintTableElement("Free");
        if (fgets(generic_string, 1023, fp) != NULL) {
            uint32_t i = 0;
            while (generic_string[i] != '\0') {
                if (generic_string[i] == '\n') {
                    generic_string[i] = '\0';
                    break;
                }
                i++;
            }
            PrintTableElement(generic_string);
        } else {
            PrintTableElement("Failed to determine current directory disk space");
        }
        PrintEndTableRow();
        pclose(fp);
    }

    PrintEndTable();
    return result;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemExecutableInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    PrintBeginTable("Executable Info", 2);

    PrintBeginTableRow();
    PrintTableElement("Exe Directory");
    PrintTableElement(_exe_path);
    PrintEndTableRow();

    PrintBeginTableRow();
    PrintTableElement("Current Directory");
    PrintTableElement(_cur_path);
    PrintEndTableRow();

    PrintBeginTableRow();
    PrintTableElement("App Version");
    PrintTableElement(_app_version);
    PrintEndTableRow();

    PrintBeginTableRow();
    PrintTableElement("Vulkan API Version");
    PrintTableElement(std::to_string(VK_API_VERSION_MAJOR(VK_HEADER_VERSION_COMPLETE)) + "." +
        std::to_string(VK_API_VERSION_MINOR(VK_HEADER_VERSION_COMPLETE)) + "." +
        std::to_string(VK_API_VERSION_PATCH(VK_HEADER_VERSION_COMPLETE)));
    PrintEndTableRow();

    PrintBeginTableRow();
    PrintTableElement("Byte Format");
#if __x86_64__ || __ppc64__
    PrintTableElement("64-bit");
#else
    PrintTableElement("32-bit");
#endif
    PrintEndTableRow();

    PrintEndTable();
    return result;
}

// Utility function to determine if a driver may exist in the folder.
static bool CheckDriver(ViaSystemMacOS *via_sys_macos, std::string &folder_loc, std::string &object_name) {
    bool success = false;
    (void)via_sys_macos;
    std::string full_name = folder_loc;
    if (folder_loc.c_str()[folder_loc.size() - 1] != '/') {
        full_name += "/";
    }
    full_name += object_name;
    if (access(full_name.c_str(), R_OK) != -1) {
        success = true;
    }
    return success;
}

// Pointer to a function sed to validate if the system object is found
typedef bool (*PFN_CheckIfValid)(ViaSystemMacOS *via_sys_macos, std::string &folder_loc, std::string &object_name);

static bool FindMacOSSystemObject(ViaSystemMacOS *via_sys_macos, std::string object_name, std::string &location,
                                  PFN_CheckIfValid func, bool break_on_first) {
    bool found_one = false;
    std::string path_to_check;
    char *env_value = getenv("DYLD_LIBRARY_PATH");

    for (uint32_t iii = 0; iii < 2; iii++) {
        switch (iii) {
            case 0:
                path_to_check = "/usr/lib";
                break;
            case 1:
                path_to_check = "/usr/local/lib";
                break;
            default:
                continue;
        }

        if (func(via_sys_macos, path_to_check, object_name)) {
            location = path_to_check + "/" + object_name;

            // We found one runtime, clear any failures
            found_one = true;
            if (break_on_first) {
                goto out;
            }
        }
    }

    // DYLD_LIBRARY_PATH may have multiple folders listed in it (colon
    // ':' delimited)
    if (env_value != NULL) {
        char *tok = strtok(env_value, ":");
        while (tok != NULL) {
            if (strlen(tok) > 0) {
                path_to_check = tok;
                if (func(via_sys_macos, path_to_check, object_name)) {
                    location = path_to_check + "/" + object_name;

                    // We found one runtime, clear any failures
                    found_one = true;
                }
            }
            tok = strtok(NULL, ":");
        }
    }

out:
    return found_one;
}

static bool VerifyOpen(std::string library_file, std::string &error) {
    bool success = false;
    void *handle = dlopen(library_file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (NULL == handle) {
        error = dlerror();
    } else {
        dlclose(handle);
        success = true;
    }
    return success;
}

bool ViaSystemMacOS::ReadDriverJson(std::string cur_driver_json, bool &found_lib) {
    bool found_json = false;
    std::ifstream *stream = NULL;
    Json::Value root = Json::nullValue;
    Json::Value inst_exts = Json::nullValue;
    Json::Value dev_exts = Json::nullValue;
    Json::Reader reader;
    std::string full_driver_path;
    char generic_string[2048];
    uint32_t j = 0;

    stream = new std::ifstream(cur_driver_json.c_str(), std::ifstream::in);
    if (nullptr == stream || stream->fail()) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Error reading JSON file");
        PrintTableElement(cur_driver_json);
        PrintEndTableRow();
        goto out;
    }

    if (!reader.parse(*stream, root, false) || root.isNull()) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Error reading JSON file");
        PrintTableElement(reader.getFormattedErrorMessages());
        PrintEndTableRow();
        goto out;
    }

    PrintBeginTableRow();
    PrintTableElement("");
    PrintTableElement("JSON File Version");
    if (!root["file_format_version"].isNull()) {
        PrintTableElement(root["file_format_version"].asString());
    } else {
        PrintTableElement("MISSING!");
    }
    PrintEndTableRow();

    if (root["ICD"].isNull()) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("ICD Section");
        PrintTableElement("MISSING!");
        PrintEndTableRow();
        goto out;
    }

    found_json = true;

    PrintBeginTableRow();
    PrintTableElement("");
    PrintTableElement("API Version");
    if (!root["ICD"]["api_version"].isNull()) {
        PrintTableElement(root["ICD"]["api_version"].asString());
    } else {
        PrintTableElement("MISSING!");
    }
    PrintEndTableRow();

    PrintBeginTableRow();
    PrintTableElement("");
    PrintTableElement("Library Path");
    if (!root["ICD"]["library_path"].isNull()) {
        std::string driver_name = root["ICD"]["library_path"].asString();
        std::string location;
        bool could_load = true;
        std::string load_error;
        PrintTableElement(driver_name);
        PrintEndTableRow();

        if (DetermineJsonLibraryPath(cur_driver_json.c_str(), driver_name.c_str(), full_driver_path)) {
            // First try the generated path.
            if (access(full_driver_path.c_str(), R_OK) != -1) {
                found_lib = true;
                could_load = VerifyOpen(full_driver_path, load_error);
            } else if (driver_name.find("/") == std::string::npos) {
                if (FindMacOSSystemObject(this, driver_name, location, CheckDriver, true)) {
                    found_lib = true;
                    could_load = VerifyOpen(location, load_error);
                }
            }
        }
        if (!found_lib) {
            bool found = false;
            uint32_t image_count = _dyld_image_count();
            for (uint32_t i = 0; i < image_count; i++) {
                std::string path = _dyld_get_image_name(i);
                if (NULL != strstr(path.c_str(), driver_name.c_str())) {
                    found = true;

                    snprintf(generic_string, 2047, "Found at %s", path.c_str());
                    PrintBeginTableRow();
                    PrintTableElement("");
                    PrintTableElement("");
                    PrintTableElement(generic_string);
                    PrintEndTableRow();
                    found_lib = true;
                    could_load = VerifyOpen(path.c_str(), load_error);
                    break;
                }
            }
            if (!found) {
                snprintf(generic_string, 1023,
                         "Failed to find driver %s "
                         "referenced by JSON %s",
                         driver_name.c_str(), cur_driver_json.c_str());
                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement("");
                PrintTableElement(generic_string);
                PrintEndTableRow();
            }
        } else if (!could_load) {
            PrintBeginTableRow();
            PrintTableElement("");
            PrintTableElement("FAILED TO LOAD!");
            PrintTableElement(load_error);
            PrintEndTableRow();
        }
    } else {
        PrintTableElement("MISSING!");
        PrintEndTableRow();
    }

    char count_str[1024];
    j = 0;
    dev_exts = root["ICD"]["device_extensions"];
    if (!dev_exts.isNull() && dev_exts.isArray()) {
        snprintf(count_str, 1023, "%d", dev_exts.size());
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Device Extensions");
        PrintTableElement(count_str);
        PrintEndTableRow();

        for (Json::ValueIterator dev_ext_it = dev_exts.begin(); dev_ext_it != dev_exts.end(); dev_ext_it++) {
            Json::Value dev_ext = (*dev_ext_it);
            Json::Value dev_ext_name = dev_ext["name"];
            if (!dev_ext_name.isNull()) {
                snprintf(generic_string, 1023, "[%d]", j);

                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                PrintTableElement(dev_ext_name.asString());
                PrintEndTableRow();
            }
        }
    }
    inst_exts = root["ICD"]["instance_extensions"];
    j = 0;
    if (!inst_exts.isNull() && inst_exts.isArray()) {
        snprintf(count_str, 1023, "%d", inst_exts.size());
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("Instance Extensions");
        PrintTableElement(count_str);
        PrintEndTableRow();

        for (Json::ValueIterator inst_ext_it =

                 inst_exts.begin();
             inst_ext_it != inst_exts.end(); inst_ext_it++) {
            Json::Value inst_ext = (*inst_ext_it);
            Json::Value inst_ext_name = inst_ext["name"];
            if (!inst_ext_name.isNull()) {
                snprintf(generic_string, 1023, "[%d]", j);

                PrintBeginTableRow();
                PrintTableElement("");
                PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                PrintTableElement(inst_ext_name.asString());
                PrintEndTableRow();
            }
        }
    }

out:

    if (nullptr != stream) {
        stream->close();
        delete stream;
        stream = NULL;
    }

    return found_json;
}

void ViaSystemMacOS::PrintDriverEnvVarInfo(const char* var, bool& found_json, bool& found_lib) {
    bool found_this_lib = false;
    char *env_var_value = getenv(var);
    if (NULL != env_var_value) {
        PrintBeginTableRow();
        PrintTableElement(var);
        PrintTableElement(env_var_value);
        PrintTableElement("");
        PrintEndTableRow();

        // These variables may have multiple folders listed in it (colon
        // ':' delimited)
        char *tok = strtok(env_var_value, ":");
        if (tok != NULL) {
            while (tok != NULL) {
                if (access(tok, R_OK) != -1) {
                    PrintBeginTableRow();
                    PrintTableElement(tok, VIA_ALIGN_RIGHT);
                    PrintTableElement("");
                    PrintTableElement("");
                    PrintEndTableRow();
                    if (ReadDriverJson(tok, found_this_lib)) {
                        found_json = true;
                        found_lib |= found_this_lib;
                    }
                } else {
                    PrintBeginTableRow();
                    PrintTableElement(tok, VIA_ALIGN_RIGHT);
                    PrintTableElement("No such file");
                    PrintTableElement("");
                    PrintEndTableRow();
                }
                tok = strtok(NULL, ":");
            }
        } else {
            if (access(env_var_value, R_OK) != -1) {
                PrintBeginTableRow();
                PrintTableElement(env_var_value, VIA_ALIGN_RIGHT);
                PrintTableElement("");
                PrintTableElement("");
                PrintEndTableRow();
                if (ReadDriverJson(env_var_value, found_this_lib)) {
                    found_json = true;
                    found_lib |= found_this_lib;
                }
            } else {
                PrintBeginTableRow();
                PrintTableElement(env_var_value, VIA_ALIGN_RIGHT);
                PrintTableElement("No such file");
                PrintTableElement("");
                PrintEndTableRow();
            }
        }
    }
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemDriverInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    bool found_json = false;
    bool found_lib = false;
    bool found_this_lib = false;
    uint32_t i = 0;
    char generic_string[1024];
    char cur_vulkan_driver_json[1024];
    char *home_env_value = NULL;
    std::vector<std::string> driver_paths;

    PrintBeginTable("Vulkan Driver Info", 3);

    // There are several folders ICD JSONs could be in.  So,
    // try all of them.
    driver_paths.push_back("/etc/vulkan/icd.d");
    driver_paths.push_back("/usr/share/vulkan/icd.d");
    driver_paths.push_back("/usr/local/etc/vulkan/icd.d");
    driver_paths.push_back("/usr/local/share/vulkan/icd.d");

    home_env_value = getenv("HOME");
    if (NULL == home_env_value) {
        driver_paths.push_back("~/.local/share/vulkan/icd.d");
    } else {
        std::string home_icd_dir = home_env_value;
        home_icd_dir += "/.local/share/vulkan/icd.d";
        driver_paths.push_back(home_icd_dir);
    }

    // Loop through all folders discovered above.
    for (size_t dir = 0; dir < driver_paths.size(); dir++) {
        // Just to make things clear, make sure to add a
        // identifier before the drivers path results.
        if (dir == 0) {
            PrintBeginTableRow();
            PrintTableElement("Standard Paths");
            PrintTableElement("");
            PrintTableElement("");
            PrintEndTableRow();
        }

        // Make sure the directory exists.
        DIR *driver_dir = opendir(driver_paths[dir].c_str());
        if (NULL == driver_dir) {
            PrintBeginTableRow();
            PrintTableElement(driver_paths[dir], VIA_ALIGN_RIGHT);
            PrintTableElement("No such folder");
            PrintTableElement("");
            PrintEndTableRow();

            continue;
        }

        PrintBeginTableRow();
        PrintTableElement(driver_paths[dir], VIA_ALIGN_RIGHT);
        PrintTableElement("");
        PrintTableElement("");
        PrintEndTableRow();

        dirent *cur_ent;
        i = 0;
        while ((cur_ent = readdir(driver_dir)) != NULL) {
            if (NULL != strstr(cur_ent->d_name, ".json")) {
                snprintf(generic_string, 1023, "[%d]", i++);
                snprintf(cur_vulkan_driver_json, 1023, "%s/%s", driver_paths[dir].c_str(), cur_ent->d_name);

                PrintBeginTableRow();
                PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                PrintTableElement(cur_ent->d_name);
                PrintTableElement("");
                PrintEndTableRow();

                if (ReadDriverJson(cur_vulkan_driver_json, found_this_lib)) {
                    found_json = true;
                    found_lib |= found_this_lib;
                }
            }
        }
    }

    PrintDriverEnvVarInfo("VK_DRIVER_FILES", found_json, found_lib);
    PrintDriverEnvVarInfo("VK_ICD_FILENAMES", found_json, found_lib);
    PrintDriverEnvVarInfo("VK_ADD_DRIVER_FILES", found_json, found_lib);

    PrintEndTable();

    if (!found_json) {
        result = VIA_MISSING_DRIVER_JSON;
    } else if (!found_lib) {
        result = VIA_MISSING_DRIVER_LIB;
    }

    return result;
}

// Print out all the runtime files found in a given location.  This way we
// capture the full state of the system.
ViaSystem::ViaResults ViaSystemMacOS::PrintRuntimesInFolder(std::string &folder_loc, std::string &object_name, bool print_header) {
    DIR *runtime_dir;
    ViaResults res = VIA_SUCCESSFUL;

    runtime_dir = opendir(folder_loc.c_str());
    if (NULL != runtime_dir) {
        bool file_found = false;
        uint32_t i = 0;
        dirent *cur_ent;
        std::string command_str;
        std::stringstream generic_str;

        if (print_header) {
            PrintBeginTableRow();
            PrintTableElement(folder_loc, VIA_ALIGN_RIGHT);
            PrintTableElement("");
            PrintTableElement("");
            PrintEndTableRow();
        }

        while ((cur_ent = readdir(runtime_dir)) != NULL) {
            std::string name_check = object_name + "dylib";
            if (NULL != strstr(cur_ent->d_name, name_check.c_str())) {
                char buffer[1023];
                std::string object_path = folder_loc + "/" + cur_ent->d_name;
                ssize_t len = readlink(object_path.c_str(), buffer, 1023);

                generic_str << "[" << i++ << "]";

                PrintBeginTableRow();
                PrintTableElement(generic_str.str(), VIA_ALIGN_RIGHT);

                file_found = true;

                if (len == -1) {
                    PrintTableElement(cur_ent->d_name);
                    PrintTableElement("Failed to retrieve symbolic link");
                    res = VIA_SYSTEM_CALL_FAILURE;
                } else {
                    buffer[len] = '\0';
                    if (len > 0) {
                        std::string trimmed_path = TrimWhitespace(buffer, " \t\n\r\'\"");

                        PrintTableElement(object_path);
                        PrintTableElement(trimmed_path);
                    } else {
                        PrintTableElement(cur_ent->d_name);
                        PrintTableElement("Failed to retrieve symbolic link");
                    }

                    PrintEndTableRow();
                }
            }
        }
        if (!file_found) {
            PrintBeginTableRow();
            PrintTableElement("");
            PrintTableElement("No " + object_name + "dylib files found");
            PrintTableElement("");
            PrintEndTableRow();
        }
        closedir(runtime_dir);
    } else {
        PrintBeginTableRow();
        PrintTableElement(folder_loc, VIA_ALIGN_RIGHT);
        PrintTableElement("No such folder");
        PrintTableElement("");
        PrintEndTableRow();
    }

    return res;
}

// Utility function to determine if a runtime exists in the folder
static bool CheckRuntime(ViaSystemMacOS *via_sys_macos, std::string &folder_loc, std::string &object_name) {
    return (ViaSystem::VIA_SUCCESSFUL == via_sys_macos->PrintRuntimesInFolder(folder_loc, object_name));
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemLoaderInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    const char vulkan_dylib_prefix[] = "libvulkan.1.";
    std::string path;
    std::string runtime_dir_name;
    std::string location;

    PrintBeginTable("Vulkan Runtimes", 3);

    PrintBeginTableRow();
    PrintTableElement("Possible Runtime Folders");
    PrintTableElement("");
    PrintTableElement("");
    PrintEndTableRow();

    if (!FindMacOSSystemObject(this, vulkan_dylib_prefix, location, CheckRuntime, false)) {
        result = VIA_VULKAN_CANT_FIND_RUNTIME;
    }

    uint32_t bufSize = 0;
    _NSGetExecutablePath(NULL, &bufSize);
    char buff[bufSize];
    int exe_found = _NSGetExecutablePath(buff, &bufSize);
    if (exe_found != -1) {
        std::string runtime_dir_id = "Runtime Folder Used By via";
        uint32_t image_count = _dyld_image_count();
        if (image_count == 0) {
            PrintBeginTableRow();
            PrintTableElement(runtime_dir_id);
            PrintTableElement("Failed to query via library info");
            PrintTableElement("");
            PrintEndTableRow();
            result = VIA_SYSTEM_CALL_FAILURE;
        } else {
            bool found = false;
            for (uint32_t i = 0; i < image_count; i++) {
                path = _dyld_get_image_name(i);
                if (NULL != strstr(path.c_str(), vulkan_dylib_prefix)) {
                    std::string after_arrow = path;
                    std::string before_slash = after_arrow.substr(0, after_arrow.rfind("/"));
                    std::string trimmed = TrimWhitespace(before_slash, " \t\n\r\'\"");

                    PrintBeginTableRow();
                    PrintTableElement(runtime_dir_id);
                    PrintTableElement(trimmed);
                    PrintTableElement("");
                    PrintEndTableRow();

                    std::string find_dylib = vulkan_dylib_prefix;
                    ViaResults temp_res = PrintRuntimesInFolder(trimmed, find_dylib, false);
                    if (!found) {
                        // If nothing's been found so far, save the current error message
                        result = temp_res;
                        // If the result was successful, then we did end up finding at least
                        // one.
                        if (result == VIA_SUCCESSFUL) {
                            found = true;
                        }
                        // We found at least one runtime already, clear any failures
                    } else if (found && result == VIA_VULKAN_CANT_FIND_RUNTIME) {
                        result = VIA_SUCCESSFUL;
                    }
                    break;
                }
            }
            if (!found) {
                PrintBeginTableRow();
                PrintTableElement(runtime_dir_id);
                PrintTableElement("Failed to find Vulkan dylib used for via");
                PrintTableElement("");
                PrintEndTableRow();
            }
        }
    }

    PrintEndTable();
    return result;
}

// Print out the explicit layers that are stored in any of the standard
// locations.
ViaSystem::ViaResults ViaSystemMacOS::PrintExplicitLayersInFolder(const std::string &id, std::string &folder_loc) {
    ViaResults res = VIA_SUCCESSFUL;
    DIR *layer_dir;

    layer_dir = opendir(folder_loc.c_str());
    if (NULL != layer_dir) {
        dirent *cur_ent;
        std::string cur_layer;
        char generic_string[1024];
        uint32_t i = 0;
        bool found_json = false;

        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement(id, VIA_ALIGN_RIGHT);
        PrintTableElement(folder_loc);
        PrintTableElement("");
        PrintEndTableRow();

        // Loop through each JSON in a given folder
        while ((cur_ent = readdir(layer_dir)) != NULL) {
            if (NULL != strstr(cur_ent->d_name, ".json")) {
                found_json = true;

                snprintf(generic_string, 1023, "[%d]", i++);
                cur_layer = folder_loc;
                cur_layer += "/";
                cur_layer += cur_ent->d_name;

                // Parse the JSON file
                std::ifstream *stream = NULL;
                stream = new std::ifstream(cur_layer, std::ifstream::in);
                if (nullptr == stream || stream->fail()) {
                    PrintBeginTableRow();
                    PrintTableElement("");
                    PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                    PrintTableElement(cur_ent->d_name);
                    PrintTableElement("ERROR reading JSON file!");
                    PrintEndTableRow();
                } else {
                    Json::Value root = Json::nullValue;
                    Json::Reader reader;
                    if (!reader.parse(*stream, root, false) || root.isNull()) {
                        // Report to the user the failure and their
                        // locations in the document.
                        PrintBeginTableRow();
                        PrintTableElement("");
                        PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                        PrintTableElement(cur_ent->d_name);
                        PrintTableElement(reader.getFormattedErrorMessages());
                        PrintEndTableRow();
                    } else {
                        PrintBeginTableRow();
                        PrintTableElement("");
                        PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                        PrintTableElement(cur_ent->d_name);
                        PrintTableElement("");
                        PrintEndTableRow();

                        // Dump out the standard explicit layer information.
                        GenerateExplicitLayerJsonInfo(cur_layer.c_str(), root);
                    }

                    stream->close();
                    delete stream;
                    stream = NULL;
                }
            }
        }
        if (!found_json) {
            PrintBeginTableRow();
            PrintTableElement("");
            PrintTableElement(id, VIA_ALIGN_RIGHT);
            PrintTableElement(folder_loc);
            PrintTableElement("No JSON files found");
            PrintEndTableRow();
        }
        closedir(layer_dir);
    } else {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement(id, VIA_ALIGN_RIGHT);
        PrintTableElement(folder_loc);
        PrintTableElement("No such folder");
        PrintEndTableRow();
    }

    return res;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemSdkInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    bool sdk_exists = false;
    bool is_system_installed_sdk = false;
    std::string sdk_path;
    std::string sdk_env_name;
    const char vulkan_so_prefix[] = "libvulkan.dylib.";
    DIR *sdk_dir;
    dirent *cur_ent;
    char *env_value;

    PrintBeginTable("Vulkan SDKs", 4);

    // First, try environmental variables
    sdk_env_name = "VULKAN_SDK";
    env_value = getenv(sdk_env_name.c_str());
    if (env_value != NULL) {
        sdk_path = env_value;
    } else {
        sdk_path = "/usr/local";
        is_system_installed_sdk = true;
    }
    PrintBeginTableRow();
    PrintTableElement(sdk_env_name);
    PrintTableElement("");
    PrintTableElement("");
    PrintTableElement("");
    PrintEndTableRow();

    const std::vector<const char *> explicit_layer_path_suffixes{"/etc/explicit_layer.d", "/etc/vulkan/explicit_layer.d",
                                                                 "/share/explicit_layer.d", "/share/vulkan/explicit_layer.d"};

    for (auto &explicit_layer_path_suffix : explicit_layer_path_suffixes) {
        std::string explicit_layer_path = sdk_path + explicit_layer_path_suffix;

        sdk_dir = opendir(explicit_layer_path.c_str());
        if (NULL != sdk_dir) {
            while ((cur_ent = readdir(sdk_dir)) != NULL) {
                if (NULL != strstr(cur_ent->d_name, vulkan_so_prefix) && strlen(cur_ent->d_name) == 14) {
                }
            }
            closedir(sdk_dir);

            result = PrintExplicitLayersInFolder("", explicit_layer_path);

            _is_system_installed_sdk = is_system_installed_sdk;
            _found_sdk = true;
            _sdk_path = sdk_path;
            sdk_exists = true;
            break;
        }
    }

    if (!sdk_exists) {
        PrintBeginTableRow();
        PrintTableElement("");
        PrintTableElement("");
        PrintTableElement("No installed SDKs found");
        PrintTableElement("");
        PrintEndTableRow();
    }

    PrintEndTable();
    return result;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemImplicitLayerInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    uint32_t i = 0;
    char generic_string[1024];
    char cur_vulkan_layer_json[1024];
    DIR *layer_dir;
    dirent *cur_ent;
    std::string layer_path;

    PrintBeginTable("Vulkan Implicit Layers", 4);

    // There are several folders implicit layers could be in.  So,
    // try all of them.
    for (uint32_t dir = 0; dir < 4; dir++) {
        std::string cur_layer_path;
        switch (dir) {
            case 0:
                cur_layer_path = "/usr/etc/vulkan/implicit_layer.d";
                break;
            case 1:
                cur_layer_path = "/usr/share/vulkan/implicit_layer.d";
                break;
            case 2:
                cur_layer_path = "/usr/local/etc/vulkan/implicit_layer.d";
                break;
            case 3:
                cur_layer_path = "/usr/local/share/vulkan/implicit_layer.d";
                break;
            case 4: {
                char *env_value = getenv("HOME");
                if (NULL == env_value) {
                    cur_layer_path = "~/.local/share/vulkan/implicit_layer.d";
                } else {
                    cur_layer_path = env_value;
                    cur_layer_path += "/.local/share/vulkan/implicit_layer.d";
                }
                break;
            }
            default:
                continue;
        }

        layer_dir = opendir(cur_layer_path.c_str());
        if (NULL != layer_dir) {
            PrintBeginTableRow();
            PrintTableElement(cur_layer_path, VIA_ALIGN_RIGHT);
            PrintTableElement("");
            PrintTableElement("");
            PrintTableElement("");
            PrintEndTableRow();
            while ((cur_ent = readdir(layer_dir)) != NULL) {
                if (NULL != strstr(cur_ent->d_name, ".json")) {
                    snprintf(generic_string, 1023, "[%d]", i++);
                    snprintf(cur_vulkan_layer_json, 1023, "%s/%s", cur_layer_path.c_str(), cur_ent->d_name);

                    PrintBeginTableRow();
                    PrintTableElement(generic_string, VIA_ALIGN_RIGHT);
                    PrintTableElement(cur_ent->d_name);
                    PrintTableElement("");
                    PrintTableElement("");
                    PrintEndTableRow();

                    std::ifstream *stream = NULL;
                    stream = new std::ifstream(cur_vulkan_layer_json, std::ifstream::in);
                    if (nullptr == stream || stream->fail()) {
                        PrintBeginTableRow();
                        PrintTableElement("");
                        PrintTableElement("ERROR reading JSON file!");
                        PrintTableElement("");
                        PrintTableElement("");
                        PrintEndTableRow();
                    } else {
                        Json::Value root = Json::nullValue;
                        Json::Reader reader;
                        if (!reader.parse(*stream, root, false) || root.isNull()) {
                            // Report to the user the failure and their
                            // locations in the document.
                            PrintBeginTableRow();
                            PrintTableElement("");
                            PrintTableElement("ERROR parsing JSON file!");
                            PrintTableElement(reader.getFormattedErrorMessages());
                            PrintTableElement("");
                            PrintEndTableRow();
                        } else {
                            GenerateImplicitLayerJsonInfo(cur_vulkan_layer_json, root, _layer_override_search_path);
                        }

                        stream->close();
                        delete stream;
                        stream = NULL;
                    }
                }
            }
            closedir(layer_dir);
        } else {
            PrintBeginTableRow();
            PrintTableElement(cur_layer_path, VIA_ALIGN_RIGHT);
            PrintTableElement("Directory does not exist");
            PrintTableElement("");
            PrintTableElement("");
            PrintEndTableRow();
        }
    }

    PrintEndTable();
    return result;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintLayerEnvVar(const char* var) {
    ViaResults result = VIA_SUCCESSFUL;

    // Look at the environment variable paths if it is set.
    char *env_value = getenv(var);
    std::string cur_json;
    if (NULL != env_value) {
        char *tok = strtok(env_value, ":");
        std::string explicit_layer_id = var;

        PrintBeginTableRow();
        PrintTableElement(var);
        PrintTableElement("");
        PrintTableElement("");
        PrintTableElement("");
        PrintEndTableRow();

        if (NULL != tok) {
            uint32_t offset = 0;
            std::stringstream cur_name;
            while (NULL != tok) {
                cur_json = tok;
                cur_name.str("");
                cur_name << "Path " << offset++;
                explicit_layer_id = cur_name.str();
                result = PrintExplicitLayersInFolder(explicit_layer_id, cur_json);
                tok = strtok(NULL, ":");
            }
        } else {
            cur_json = env_value;
            result = PrintExplicitLayersInFolder(explicit_layer_id, cur_json);
        }
    }
    return result;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemExplicitLayerInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    char *env_value = NULL;
    std::string explicit_layer_id;

    PrintBeginTable("Vulkan Explicit Layers", 4);

    if (0 != _layer_override_search_path.size()) {
        explicit_layer_id = "Override";

        PrintBeginTableRow();
        PrintTableElement("Override Paths");
        PrintTableElement("");
        PrintTableElement("");
        PrintTableElement("");
        PrintEndTableRow();

        for (uint32_t cur_path = 0; cur_path < _layer_override_search_path.size(); cur_path++) {
            result = PrintExplicitLayersInFolder(explicit_layer_id, _layer_override_search_path[cur_path]);
        }
    }

    ViaResults tmp_result = PrintLayerEnvVar("VK_LAYER_PATH");
    if (tmp_result != VIA_SUCCESSFUL) {
        result = tmp_result;
    }
    tmp_result = PrintLayerEnvVar("VK_ADD_LAYER_PATH");
    if (tmp_result != VIA_SUCCESSFUL) {
        result = tmp_result;
    }

    PrintBeginTableRow();
    PrintTableElement("Standard Paths");
    PrintTableElement("");
    PrintTableElement("");
    PrintTableElement("");
    PrintEndTableRow();

    // There are several folders explicit layers could be in.  So,
    // try all of them.
    for (uint32_t dir = 0; dir < 4; dir++) {
        std::string cur_layer_path;
        std::string explicit_layer_id;
        std::string explicit_layer_path = cur_layer_path;
        char *env_value = NULL;
        switch (dir) {
            case 0:
                cur_layer_path = "/etc/vulkan/explicit_layer.d";
                explicit_layer_id = "/etc/vulkan";
                break;
            case 1:
                cur_layer_path = "/usr/share/vulkan/explicit_layer.d";
                explicit_layer_id = "/usr/share/vulkan";
                break;
            case 2:
                cur_layer_path = "/usr/local/etc/vulkan/explicit_layer.d";
                explicit_layer_id = "/usr/local/etc/vulkan";
                break;
            case 3:
                cur_layer_path = "/usr/local/share/vulkan/explicit_layer.d";
                explicit_layer_id = "/usr/local/share/vulkan";
                break;
            case 4:
                explicit_layer_id = "$HOME/.local/share/vulkan/explicit_layer.d";
                env_value = getenv("HOME");
                if (NULL == env_value) {
                    cur_layer_path = "~/.local/share/vulkan/explicit_layer.d";
                } else {
                    cur_layer_path = env_value;
                    cur_layer_path += "/.local/share/vulkan/explicit_layer.d";
                }
                break;
            default:
                continue;
        }

        result = PrintExplicitLayersInFolder(explicit_layer_id, cur_layer_path);
    }

    PrintEndTable();
    return result;
}

bool ViaSystemMacOS::CheckExpiration(ViaSystem::OverrideExpiration expiration) {
    bool still_valid = false;
    uint16_t year = 0;
    uint8_t month = 0;
    uint8_t day = 0;
    uint8_t hour = 0;
    uint8_t minute = 0;

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    year = tm.tm_year + 1900;
    month = tm.tm_mon + 1;
    day = tm.tm_mday;
    hour = tm.tm_hour;
    minute = tm.tm_min;

    if (expiration.year > year || expiration.month > month || expiration.day > day || expiration.hour > hour ||
        expiration.minute > minute) {
        still_valid = true;
    }

    return still_valid;
}

ViaSystem::ViaResults ViaSystemMacOS::PrintSystemSettingsFileInfo() {
    ViaResults result = VIA_SUCCESSFUL;
    const char settings_file_name[] = "vk_layer_settings.txt";
    std::vector<std::string> settings_files;

    PrintBeginTable("Vulkan Layer Settings File", 4);

    // If the settings path environment variable is set, use that.

    char *settings_path = getenv("VK_LAYER_SETTINGS_PATH");
    if (NULL != settings_path) {
        std::string full_file = settings_path;
        full_file += '/';
        full_file += settings_file_name;

        PrintBeginTableRow();
        PrintTableElement("VK_LAYER_SETTINGS_PATH");
        PrintTableElement(settings_path);
        PrintTableElement("");
        PrintTableElement("");
        PrintEndTableRow();
        GenerateSettingsFileJsonInfo(full_file);
    } else {
        // There are several folders settings JSONs could be in.  So,
        // try all of them.
        std::string full_file = "/etc/vulkan/settings.d/";
        full_file += settings_file_name;
        settings_files.push_back(full_file);
        full_file = "/usr/share/vulkan/settings.d/";
        full_file += settings_file_name;
        settings_files.push_back(full_file);
        full_file = "/usr/local/etc/vulkan/settings.d/";
        full_file += settings_file_name;
        settings_files.push_back(full_file);
        full_file = "/usr/local/share/vulkan/settings.d/";
        full_file += settings_file_name;
        settings_files.push_back(full_file);
        const char *home_env_value = getenv("HOME");
        if (NULL == home_env_value) {
            full_file = "~/.local/share/vulkan/settings.d/";
            full_file += settings_file_name;
            settings_files.push_back(full_file);
        } else {
            full_file = home_env_value;
            full_file += "/.local/share/vulkan/settings.d/";
            full_file += settings_file_name;
            settings_files.push_back(full_file);
        }

        PrintBeginTableRow();
        PrintTableElement("VK_LAYER_SETTINGS_PATH");
        PrintTableElement("Not Defined");
        PrintTableElement("");
        PrintTableElement("");
        PrintEndTableRow();
        for (uint32_t set_file = 0; set_file < settings_files.size(); set_file++) {
            GenerateSettingsFileJsonInfo(settings_files[set_file]);
        }
    }

    PrintEndTable();

    return result;
}

std::string ViaSystemMacOS::GetEnvironmentalVariableValue(const std::string &env_var) {
    std::string return_value;
    char *env_return = getenv(env_var.c_str());
    if (NULL != env_return) {
        return_value = env_return;
    }
    return return_value;
}

bool ViaSystemMacOS::ExpandPathWithEnvVar(std::string &path) {
    // TBD
    (void)path;
    return true;
}

#endif  // VIA_MACOS_TARGET
