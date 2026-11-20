#include "headers/helpers/startupStorage.h"

#include "headers/GACPD/Methods/appPath.h"

#include <QDir>

StartupFailure prepareStorage()
{
    movisEnsureLocalRoot();

    if (!movisEnsureWorkingRoot() || !movisEnsureResultsRoot()) {
        return {
            "MOVis Storage Error",
            "MOVis could not create its writable runtime directories.\n\n"
            "Private working data should be stored in:\n" +
                movisWorkingRoot() +
            "\n\nResults should be stored in:\n" +
                movisResultsRoot()
        };
    }

    if (!QDir::setCurrent(movisWorkingRoot())) {
        return {
            "MOVis Storage Error",
            "MOVis could not activate its writable runtime directory:\n" +
                movisWorkingRoot()
        };
    }

    QString storageError;
    if (!movisTestWritable(movisWorkingRoot(), &storageError)) {
        return {
            "MOVis Storage Error",
            "MOVis cannot write to its private runtime directory:\n" +
                movisWorkingRoot() + "\n\n" + storageError
        };
    }

    storageError.clear();
    if (!movisTestWritable(movisResultsRoot(), &storageError)) {
        return {
            "MOVis Results Storage Error",
            "MOVis cannot write classification results to:\n" +
                movisResultsRoot() + "\n\n" + storageError +
            "\n\nOn macOS MOVis stores public results at "
            "~/MOVis/Results/Repos_results to avoid protected Documents "
            "folder permissions."
        };
    }

    return {};
}
