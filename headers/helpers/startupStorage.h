#ifndef STARTUP_STORAGE_H
#define STARTUP_STORAGE_H

#include <QString>

// Checking that MOVis can write where it needs to, before anything else runs.
//
// The three roots it needs are described in GACPD/Methods/appPath.h. On macOS
// any of them can exist and still be denied to the application, so they are
// tested by writing rather than by checking for existence.

// Empty when everything is in order; otherwise the title and message to show
// the user instead of the main window.
struct StartupFailure {
    QString title;
    QString message;

    explicit operator bool() const { return !message.isEmpty(); }
};

// Creates the runtime directories, makes the working root the current
// directory, and verifies both writable roots.
StartupFailure prepareStorage();

#endif // STARTUP_STORAGE_H
