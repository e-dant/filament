/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "CommandlineConfig.h"

#include <private/filament/Variant.h>

#include <getopt/getopt.h>

#include <utils/Path.h>

#include <istream>
#include <sstream>
#include <string>

using namespace utils;

namespace matc {

static void usage(char* name) {
    std::string exec_name(utils::Path(name).getName());
    std::string usage(
            "MATC is a command-line tool to compile material definition.\n"
            "Usages:\n"
            "    MATC [options] <input-file>\n"
            "\n"
            "Supported input formats:\n"
            "    Filament material definition (.mat)\n"
            "\n"
            "Options:\n"
            "   --help, -h\n"
            "       Print this message\n\n"
            "   --license\n"
            "       Print copyright and license information\n\n"
            "   --output, -o\n"
            "       Specify path to output file\n\n"
            "   --platform, -p\n"
            "       Shader family to generate: desktop, mobile or all (default)\n\n"
            "   --optimize-size, -S\n"
            "       Optimize generated shader code for size instead of just performance\n\n"
            "   --api, -a\n"
            "       Specify the target API: opengl (default), vulkan, metal, or all\n"
            "       This flag can be repeated to individually select APIs for inclusion:\n"
            "           MATC --api opengl --api metal ...\n\n"
            "   --define, -D\n"
            "       Add a preprocessor define macro via <macro>=<value>. <value> defaults to 1 if omitted.\n"
            "       Can be repeated to specify multiple definitions:\n"
            "           MATC -Dfoo=1 -Dbar -Dbuzz=100 ...\n\n"
            "   --reflect, -r\n"
            "       Reflect the specified metadata as JSON: parameters\n\n"
            "   --variant-filter=<filter>, -V <filter>\n"
            "       Filter out specified comma-separated variants:\n"
            "           directionalLighting, dynamicLighting, shadowReceiver, skinning, vsm, fog\n"
            "       This variant filter is merged with the filter from the material, if any\n\n"
            "   --version, -v\n"
            "       Print the material version number\n\n"
            "Internal use and debugging only:\n"
            "   --optimize-none, -g\n"
            "       Disable all shader optimizations, for debugging\n\n"
            "   --preprocessor-only, -E\n"
            "       Optimize shaders by running only the preprocessor\n\n"
            "   --raw, -w\n"
            "       Compile a raw GLSL shader into a SPIRV binary chunk\n\n"
            "   --output-format, -f\n"
            "       Specify output format: blob (default) or header\n\n"
            "   --debug, -d\n"
            "       Generate extra data for debugging\n\n"
            "   --print, -t\n"
            "       Print generated shaders for debugging\n\n"
    );
    const std::string from("MATC");
    for (size_t pos = usage.find(from); pos != std::string::npos; pos = usage.find(from, pos)) {
        usage.replace(pos, from.length(), exec_name);
    }
    printf("%s", usage.c_str());
}

static void license() {
    static const char *license[] = {
        #include "licenses/licenses.inc"
        nullptr
    };

    const char **p = &license[0];
    while (*p)
        std::cout << *p++ << std::endl;
}

static uint8_t parseVariantFilter(const std::string& arg) {
    std::stringstream ss(arg);
    std::string item;
    uint8_t variantFilter = 0;
    while (std::getline(ss, item, ',')) {
        if (item == "directionalLighting") {
            variantFilter |= filament::Variant::DIR;
        } else if (item == "dynamicLighting") {
            variantFilter |= filament::Variant::DYN;
        } else if (item == "shadowReceiver") {
            variantFilter |= filament::Variant::SRE;
        } else if (item == "skinning") {
            variantFilter |= filament::Variant::SKN;
        } else if (item == "vsm") {
            variantFilter |= filament::Variant::VSM;
        } else if (item == "fog") {
            variantFilter |= filament::Variant::FOG;
        }
    }
    return variantFilter;
}

CommandlineConfig::CommandlineConfig(int argc, char** argv) : Config(), mArgc(argc), mArgv(argv) {
    mIsValid = parse();
}

static void parseDefine(std::string defineString,
        std::unordered_map<std::string, std::string>& defines) {
    const char* const defineArg = defineString.c_str();
    const size_t length = defineString.length();

    const char* p = defineArg;
    const char* end = p + length;

    while (p < end && *p != '=') {
        p++;
    }

    if (*p == '=') {
        if (p == defineArg || p + 1 >= end) {
            // Edge-cases, missing define name or value.
            return;
        }
        std::string def(defineArg, p - defineArg);
        defines.emplace(def, p + 1);
        return;
    }

    // No explicit assignment, use a default value of 1.
    std::string def(defineArg, p - defineArg);
    defines.emplace(def, "1");
}

bool CommandlineConfig::parse() {
    static constexpr const char* OPTSTR = "hlxo:f:dm:a:p:D:OSEr:vV:gtw";
    static const struct option OPTIONS[] = {
            { "help",                    no_argument, nullptr, 'h' },
            { "license",                 no_argument, nullptr, 'l' },
            { "output",            required_argument, nullptr, 'o' },
            { "output-format",     required_argument, nullptr, 'f' },
            { "debug",                   no_argument, nullptr, 'd' },
            { "variant-filter",    required_argument, nullptr, 'V' },
            { "platform",          required_argument, nullptr, 'p' },
            { "optimize",                no_argument, nullptr, 'x' }, // for backward compatibility
            { "optimize",                no_argument, nullptr, 'O' }, // for backward compatibility
            { "optimize-size",           no_argument, nullptr, 'S' },
            { "optimize-none",           no_argument, nullptr, 'g' },
            { "preprocessor-only",       no_argument, nullptr, 'E' },
            { "api",               required_argument, nullptr, 'a' },
            { "define",            required_argument, nullptr, 'D' },
            { "reflect",           required_argument, nullptr, 'r' },
            { "print",                   no_argument, nullptr, 't' },
            { "version",                 no_argument, nullptr, 'v' },
            { "raw",                     no_argument, nullptr, 'w' },
            { nullptr, 0, nullptr, 0 }  // termination of the option list
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(mArgc, mArgv, OPTSTR, OPTIONS, &option_index)) >= 0) {
        std::string arg(optarg ? optarg : "");
        switch (opt) {
            default:
            case 'h':
                usage(mArgv[0]);
                exit(0);
                break;
            case 'l':
                license();
                exit(0);
                break;
            case 'o':
                mOutput = new FilesystemOutput(arg.c_str());
                break;
            case 'f':
                if (arg == "blob") {
                    mOutputFormat = OutputFormat::BLOB;
                } else if (arg == "header") {
                    mOutputFormat = OutputFormat::C_HEADER;
                } else {
                    std::cerr << "Unrecognized output format flag. Must be 'blob'|'header'."
                            << std::endl;
                   return false;
                }
                break;
            case 'd':
                mDebug = true;
                break;
            case 'p':
                if (arg == "desktop") {
                   mPlatform = Platform::DESKTOP;
                } else if (arg == "mobile") {
                    mPlatform = Platform::MOBILE;
                } else if (arg == "all") {
                    mPlatform = Platform::ALL;
                } else {
                    std::cerr << "Unrecognized platform. Must be 'desktop'|'mobile'|'all'."
                            << std::endl;
                    return false;
                }
                break;
            case 'a':
                if (arg == "opengl") {
                    mTargetApi |= TargetApi::OPENGL;
                } else if (arg == "vulkan") {
                    mTargetApi |= TargetApi::VULKAN;
                } else if (arg == "metal") {
                    mTargetApi |= TargetApi::METAL;
                } else if (arg == "all") {
                    mTargetApi |= TargetApi::ALL;
                } else {
                    std::cerr << "Unrecognized target API. Must be 'opengl'|'vulkan'|'metal'|'all'."
                            << std::endl;
                    return false;
                }
                break;
            case 'D':
                parseDefine(arg, mDefines);
                break;
            case 'v':
                // Similar to --help, the --version command does an early exit in order to avoid
                // subsequent error spew such as "Missing input filename" etc.
                std::cout << filament::MATERIAL_VERSION << std::endl;
                exit(0);
                break;
            case 'V':
                mVariantFilter = parseVariantFilter(arg);
                break;
            // These 2 flags are supported for backward compatibility
            case 'O':
            case 'x':
                mOptimizationLevel = Optimization::PERFORMANCE;
                break;
            case 'S':
                mOptimizationLevel = Optimization::SIZE;
                break;
            case 'E':
                mOptimizationLevel = Optimization::PREPROCESSOR;
                break;
            case 'g':
                mOptimizationLevel = Optimization::NONE;
                break;
            case 'r':
                mReflectionTarget = Metadata::PARAMETERS;
                break;
            case 't':
                mPrintShaders = true;
                break;
            case 'w':
                mRawShaderMode = true;
                break;
        }
    }

    if (mArgc - optind > 1) {
        std::cerr << "Only one input file should be specified on the command line." << std::endl;
        return false;
    }
    if (mArgc - optind > 0) {
        mInput = new FilesystemInput(mArgv[optind]);
    }

    return true;
}

} // namespace matc
