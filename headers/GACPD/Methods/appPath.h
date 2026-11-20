#ifndef APPPATH_H
#define APPPATH_H

#include <QString>

// Runtime root used by MOVis itself.
//
// macOS: <MOVis.app>/Contents/Resources
// Windows/Linux: directory containing the executable
QString movisLocalRoot();

// Folder containing the .app bundle on macOS. On Windows/Linux this is the
// same as movisLocalRoot().
QString movisExternalRoot();

// Writable private runtime area. A signed/translocated macOS application
// bundle must be treated as read-only, so mutable working data lives under
// ~/Library/Application Support/MOVis instead of Contents/Resources.
// Windows/Linux retain the existing executable-directory behavior.
QString movisWorkingRoot();

// Writable dependency fallback location. The shipped macOS app normally uses
// node_modules bundled in Contents/Resources. If the user invokes the manual
// dependency fallback, npm installs here so the application signature is not
// modified.
QString movisDependencyRoot();
QString movisWorkingPath(const QString &relativePath = QString());

// Public result directory. On macOS this intentionally stays outside the app
// bundle in the user's home folder so generated research data is easy to
// access and does not require macOS Files & Folders permission for Documents:
//     ~/MOVis/Results/Repos_results
// Windows/Linux retain the existing layout under the executable directory.
QString movisResultsRoot();

QString movisPath(const QString &relativePath);
QString movisResultsPath(const QString &relativePath = QString());

bool movisEnsureLocalRoot();
bool movisEnsureWorkingRoot();
bool movisEnsureResultsRoot();

// Verify that a directory can actually be written to, rather than only
// checking whether it already exists. This matters on macOS where protected
// folders can exist but still be denied to a standalone application by TCC.
bool movisTestWritable(const QString &directoryPath,
                       QString *errorMessage = nullptr);

#endif // APPPATH_H
