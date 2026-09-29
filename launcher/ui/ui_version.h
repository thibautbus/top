/* The version the home screen shows at its bottom left, "v1.0.0 (be8571e)" (config/version.json's and the commit's
 * short hash, the version alone on its release tag), and the build a session's report gives, `git describe`: both set
 * at CMake's configure (CMakeLists.txt); these defaults serve a build without it. */
#ifndef ORACLES_UI_VERSION_H
#define ORACLES_UI_VERSION_H

#ifndef ORACLES_VERSION
#define ORACLES_VERSION "v1.0.0"
#endif
#ifndef ORACLES_BUILD
#define ORACLES_BUILD ORACLES_VERSION
#endif

#endif
