#ifndef DEPENDENCY_SETUP_H
#define DEPENDENCY_SETUP_H

#include <QString>

// MOVis runs clone detection through JSCPD, pinned to one exact version so a
// newer release cannot quietly change classification results.

// The version package.json pins and the launch check looks for.
QString pinnedJscpdVersion();

// "jscpd.cmd" on Windows, "jscpd" elsewhere.
QString jscpdExecutableName();

// True when `root` already holds a node_modules with exactly the pinned JSCPD
// installed, so there is nothing to install.
bool hasPinnedJscpd(const QString &root);

// Where npm should install to on this platform. A signed macOS bundle is
// read-only, so it installs beside the app instead of inside it.
QString dependencyInstallRoot();

#endif // DEPENDENCY_SETUP_H
