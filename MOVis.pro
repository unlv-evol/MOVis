QT       += core gui network widgets concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20
QMAKE_CXXFLAGS += -std=c++20

# Include paths for thirdparty, SV_FLATBUFFERS, and images
INCLUDEPATH += $$PWD/include
INCLUDEPATH += $$PWD/logs
INCLUDEPATH += $$PWD/Pareco
INCLUDEPATH += $$PWD/assets

# Add files to DISTFILES for deployment
DISTFILES += $$files($$PWD/include/*, true)
DISTFILES += $$files($$PWD/logs/*, true)
DISTFILES += $$files($$PWD/Pareco/*, true)
DISTFILES += $$files($$PWD/assets/*, true)

# Enable compatibility for building across platforms
win32 {
    CONFIG += windows
    CONFIG -= debug_and_release debug_and_release_target
    QMAKE_HOST.arch:equals(x86_64) {
        DEFINES += ARCH_64
        message("Building for 64-bit Windows")
    } else {
        DEFINES += ARCH_32
        message("Building for 32-bit Windows")
    }

    RC_FILE = app_icon.rc

    # Paths for source and destination directories
    INCLUDE_SRC = $$PWD/include
    LOGS_SRC = $$PWD/logs
    Pareco_SRC = $$PWD/Pareco
    IMAGES_SRC = $$PWD/assets

    # Destination directory adjusted for release/debug
    CONFIG(debug, debug|release) {
        BUILD_DIR = $$OUT_PWD
    } else {
        BUILD_DIR = $$OUT_PWD
    }

    INCLUDE_DEST = $$BUILD_DIR/include
    LOGS_DEST = $$BUILD_DIR/logs
    Pareco_DEST = $$BUILD_DIR/Pareco
    IMAGES_DEST = $$BUILD_DIR/assets

    SOURCE_INCLUDE_WIN = $$replace(INCLUDE_SRC, /, \\)
    DEST_INCLUDE_WIN = $$replace(INCLUDE_DEST, /, \\)
    DEST_LOGS_WIN = $$replace(LOGS_DEST, /, \\)
    SOURCE_LOGS_WIN = $$replace(LOGS_SRC, /, \\)
    DEST_Pareco_WIN = $$replace(Pareco_DEST, /, \\)
    SOURCE_Pareco_WIN = $$replace(Pareco_SRC, /, \\)
    SOURCE_IMAGE_WIN = $$replace(IMAGES_SRC, /, \\)
    DEST_IMAGE_WIN = $$replace(IMAGES_DEST, /, \\)

    copy_third.commands = \
        $$QMAKE_COPY_DIR \"$$SOURCE_INCLUDE_WIN\" \"$$DEST_INCLUDE_WIN\" && \
        $$QMAKE_COPY_DIR \"$$SOURCE_IMAGE_WIN\" \"$$DEST_IMAGE_WIN\" && \
        $$QMAKE_COPY_DIR \"$$SOURCE_LOGS_WIN\" \"$$DEST_LOGS_WIN\" && \
        $$QMAKE_COPY_DIR \"$$SOURCE_Pareco_WIN\" \"$$DEST_Pareco_WIN\"


    QMAKE_EXTRA_TARGETS += copy_third
    PRE_TARGETDEPS += copy_third
    CONFIG(debug, debug|release) {
        BUILD_DIR = $$OUT_PWD
    } else {
        DEPLOY_PATH = $$OUT_PWD
        DEPLOY_SCRIPT = $$PWD/scripts/deploy_windows.bat
        post_build.commands = $$DEPLOY_SCRIPT $$OUT_PWD/MOVis.exe $$DEPLOY_PATH
        QMAKE_EXTRA_TARGETS += post_build
        QMAKE_POST_LINK += $(MAKE) -f Makefile post_build
    }
}

macx { # TODO-ADD Pareco
    QMAKE_CXXFLAGS += -arch arm64
    QMAKE_LFLAGS += -arch arm64
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 11.0
    CONFIG -= x86_64
    CONFIG += macx
    contains(QMAKE_CXXFLAGS, -arch\\s*x86_64): DEFINES += MAC_INTEL
    contains(QMAKE_CXXFLAGS, -arch\\s*arm64): DEFINES += MAC_ARM

    # Paths for source and destination directories
    INCLUDE_SRC = $$PWD/include
    INCLUDE_DEST = $$OUT_PWD/include
    LOGS_DEST = $$OUT_PWD/logs
    LOGS_SRC = $$PWD/logs
    IMAGES_SRC = $$PWD/assets
    IMAGES_DEST = $$OUT_PWD/assets

    # Custom command to copy the include folder
    copy_include.commands = mkdir -p $$INCLUDE_DEST && rsync -a $$INCLUDE_SRC/. $$INCLUDE_DEST
    QMAKE_EXTRA_TARGETS += copy_include

    # Custom command to copy the images folder
    copy_images.commands = mkdir -p $$IMAGES_DEST && rsync -a $$IMAGES_SRC/. $$IMAGES_DEST
    QMAKE_EXTRA_TARGETS += copy_images

    # Custom command to copy the logs folder
    copy_logs.commands = mkdir -p $$LOGS_DEST && rsync -a $$LOGS_SRC/. $$LOGS_DEST
    QMAKE_EXTRA_TARGETS += copy_logs

    # Ensure the copy commands are executed before building
    PRE_TARGETDEPS += copy_logs copy_include copy_images

    # Deployment for macOS using macdeployqt
    DEPLOY_PATH = $$OUT_PWD/deploy
    DEPLOY_SCRIPT = $$PWD/scripts/deploy_mac.sh
    post_build.commands = bash $$DEPLOY_SCRIPT $$OUT_PWD/MOVis.app $$DEPLOY_PATH
    QMAKE_EXTRA_TARGETS += post_build
    QMAKE_POST_LINK += $(MAKE) -f Makefile post_build
}

unix:!macx {
    CONFIG += linux

    # Paths for source and destination directories
    INCLUDE_SRC = $$PWD/include
    LOGS_SRC    = $$PWD/logs
    Pareco_SRC  = $$PWD/Pareco
    IMAGES_SRC  = $$PWD/assets

    # Destination directory adjusted for release/debug (same as Windows; both are $$OUT_PWD)
    CONFIG(debug, debug|release) {
        BUILD_DIR = $$OUT_PWD
    } else {
        BUILD_DIR = $$OUT_PWD
    }

    INCLUDE_DEST = $$BUILD_DIR/include
    LOGS_DEST    = $$BUILD_DIR/logs
    Pareco_DEST  = $$BUILD_DIR/Pareco
    IMAGES_DEST  = $$BUILD_DIR/assets

    # One target that copies everything (mirror Windows copy_third)
    copy_third.commands = \
        mkdir -p "$$INCLUDE_DEST" && cp -R "$$INCLUDE_SRC/." "$$INCLUDE_DEST/" && \
        mkdir -p "$$IMAGES_DEST"  && cp -R "$$IMAGES_SRC/."  "$$IMAGES_DEST/"  && \
        mkdir -p "$$LOGS_DEST"    && cp -R "$$LOGS_SRC/."    "$$LOGS_DEST/"    && \
        mkdir -p "$$Pareco_DEST"  && cp -R "$$Pareco_SRC/."  "$$Pareco_DEST/"

    QMAKE_EXTRA_TARGETS += copy_third
    PRE_TARGETDEPS += copy_third

    contains(QMAKE_CXX, g++) {
        message("Using g++ on Linux")
        DEFINES += LINUX_GCC
    } else: contains(QMAKE_CXX, clang++) {
        message("Using clang++ on Linux")
        DEFINES += LINUX_CLANG
    }

    # Deployment for Linux using linuxdeployqt
    # Uncomment the next lines if you have Ubuntu 20.04 (Focal Fossa) with glibc 2.31
    # DEPLOY_PATH = $$OUT_PWD/deploy
    # DEPLOY_SCRIPT = $$PWD/scripts/deploy_linux.sh
    # post_build.commands = bash $$DEPLOY_SCRIPT $$OUT_PWD/release/MOVis $$DEPLOY_PATH
    # QMAKE_EXTRA_TARGETS += post_build
    # QMAKE_POST_LINK += $(MAKE) -f Makefile post_build

}

# Subfolders visibility in the project sidebar
VPATH += $$unique($$dirname($$files(src/*, true)))
VPATH += $$unique($$dirname($$files(headers/*, true)))
VPATH += $$unique($$dirname($$files(forms/*, true)))

# Recursively include all .cpp and .h files from src and headers directories
SOURCES += $$files(src/*.cpp, true)

HEADERS += $$files(headers/*.h, true)

FORMS += $$files(forms/*.ui, true)

# Deployment paths
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

# Handle platform-specific libraries and dependencies
macx {
    LIBS += -framework Cocoa
}

win32 {
    LIBS += -luser32 -lgdi32 -lcomdlg32
}

unix:!macx {
    LIBS += -lpthread
}

# Additional build configurations
DEFINES += QT_DEPRECATED_WARNINGS
