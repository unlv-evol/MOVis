#!/bin/bash
set -euo pipefail

# Build a self-contained MOVis.app that can run on a Mac that does not have
# Qt or Node installed.
#
# Usage from qmake:
#   deploy_mac.sh <MOVis.app> <Qt bin directory>
#
# Backward-compatible manual usage is also supported:
#   deploy_mac.sh <MOVis.app> <optional deploy directory>
# In that case macdeployqt is resolved from PATH.

APP="${1:-}"
SECOND="${2:-}"
THIRD="${3:-}"

if [[ -z "$APP" || ! -d "$APP" ]]; then
    echo "ERROR: MOVis.app was not found: $APP" >&2
    exit 1
fi

QT_BIN_DIR=""
DEPLOY_DIR=""

if [[ -n "$SECOND" && -x "$SECOND/macdeployqt" ]]; then
    QT_BIN_DIR="$SECOND"
    DEPLOY_DIR="$THIRD"
else
    DEPLOY_DIR="$SECOND"
    QT_BIN_DIR="$THIRD"
fi

if [[ -n "$QT_BIN_DIR" && -x "$QT_BIN_DIR/macdeployqt" ]]; then
    MACDEPLOYQT="$QT_BIN_DIR/macdeployqt"
elif command -v macdeployqt >/dev/null 2>&1; then
    MACDEPLOYQT="$(command -v macdeployqt)"
else
    echo "ERROR: macdeployqt could not be found." >&2
    echo "Pass qmake's QT_INSTALL_BINS directory or add macdeployqt to PATH." >&2
    exit 1
fi

CONTENTS="$APP/Contents"
RESOURCES="$CONTENTS/Resources"
FRAMEWORKS="$CONTENTS/Frameworks"
PLUGINS="$CONTENTS/PlugIns"
APP_NAME="$(basename "$APP" .app)"
APP_EXE="$CONTENTS/MacOS/$APP_NAME"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QML_DIR="$PROJECT_ROOT/qml"

if [[ ! -x "$APP_EXE" ]]; then
    echo "ERROR: Application executable was not found: $APP_EXE" >&2
    exit 1
fi

mkdir -p "$RESOURCES"

echo "============================================================"
echo "Deploying Qt runtime into $APP"
echo "Using macdeployqt: $MACDEPLOYQT"
echo "============================================================"

# macdeployqt copies the Qt frameworks and the runtime plugins required by the
# application and rewrites their install names to bundle-relative @rpath paths.
"$MACDEPLOYQT" "$APP" -always-overwrite -verbose=2 -no-codesign \
    -qmldir="$QML_DIR"

# ---------------------------------------------------------------------------
# Bundle a native Apple Silicon Node/npm runtime when the project did not
# already provide one. MOVis uses it for npm dependency setup and JSCPD, so a
# clean end-user Mac must not depend on a system-wide Node installation.
# ---------------------------------------------------------------------------
NODE_VERSION="v22.23.2"
NODE_ARCHIVE="node-${NODE_VERSION}-darwin-arm64.tar.gz"
NODE_URL="https://nodejs.org/dist/${NODE_VERSION}/${NODE_ARCHIVE}"
NODE_SUMS_URL="https://nodejs.org/dist/${NODE_VERSION}/SHASUMS256.txt"
NODE_DEST="$RESOURCES/tools/node/macos/arm64"
NODE_EXE="$NODE_DEST/bin/node"
NPM_EXE="$NODE_DEST/bin/npm"

if [[ ! -x "$NODE_EXE" || ! -e "$NPM_EXE" ]]; then
    echo "Bundled Apple Silicon Node runtime is missing; staging ${NODE_VERSION}..."

    if ! command -v curl >/dev/null 2>&1; then
        echo "ERROR: curl is required to stage the bundled Node runtime." >&2
        exit 1
    fi

    TMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/movis-node.XXXXXX")"
    cleanup_node_tmp() {
        rm -rf "$TMP_DIR"
    }
    trap cleanup_node_tmp EXIT

    curl --fail --location --retry 3 --silent --show-error \
        "$NODE_URL" -o "$TMP_DIR/$NODE_ARCHIVE"
    curl --fail --location --retry 3 --silent --show-error \
        "$NODE_SUMS_URL" -o "$TMP_DIR/SHASUMS256.txt"

    EXPECTED_SUM="$(awk -v file="$NODE_ARCHIVE" '$2 == file { print $1; exit }' "$TMP_DIR/SHASUMS256.txt")"
    if [[ -z "$EXPECTED_SUM" ]]; then
        echo "ERROR: Could not find $NODE_ARCHIVE in Node's SHASUMS256.txt." >&2
        exit 1
    fi

    ACTUAL_SUM="$(shasum -a 256 "$TMP_DIR/$NODE_ARCHIVE" | awk '{print $1}')"
    if [[ "$EXPECTED_SUM" != "$ACTUAL_SUM" ]]; then
        echo "ERROR: Node archive checksum verification failed." >&2
        exit 1
    fi

    tar -xzf "$TMP_DIR/$NODE_ARCHIVE" -C "$TMP_DIR"
    EXTRACTED="$TMP_DIR/node-${NODE_VERSION}-darwin-arm64"

    rm -rf "$NODE_DEST"
    mkdir -p "$NODE_DEST"
    rsync -a "$EXTRACTED/" "$NODE_DEST/"

    cleanup_node_tmp
    trap - EXIT
fi

if [[ ! -x "$NODE_EXE" ]]; then
    echo "ERROR: Bundled Node executable is missing after deployment: $NODE_EXE" >&2
    exit 1
fi

if [[ ! -e "$NPM_EXE" ]]; then
    echo "ERROR: Bundled npm is missing after deployment: $NPM_EXE" >&2
    exit 1
fi

# Pre-install JSCPD into Contents/Resources on the build Mac. This means the
# normal clean-Mac launch path finds node_modules immediately and does not have
# to mutate the app bundle. The in-app automatic/manual installers remain as a
# fallback if node_modules is later removed.
if [[ -f "$RESOURCES/package.json" ]]; then
    echo "Installing pinned JavaScript dependencies into the app bundle..."
    export PATH="$NODE_DEST/bin:$PATH"
    (
        cd "$RESOURCES"
        "$NPM_EXE" ci --omit=dev --no-audit --no-fund
    )
else
    echo "ERROR: package.json is missing from $RESOURCES" >&2
    exit 1
fi

# ---------------------------------------------------------------------------
# Validate the bundle. Fail the build rather than producing an app that only
# works on the Qt development machine.
# ---------------------------------------------------------------------------
REQUIRED_FRAMEWORKS=(QtCore QtGui QtQml QtQuick QtQuickControls2 QtNetwork QtConcurrent)
for framework in "${REQUIRED_FRAMEWORKS[@]}"; do
    if [[ ! -d "$FRAMEWORKS/${framework}.framework" ]]; then
        echo "ERROR: Required Qt framework was not bundled: ${framework}.framework" >&2
        exit 1
    fi
done

COCOA_PLUGIN="$PLUGINS/platforms/libqcocoa.dylib"
if [[ ! -f "$COCOA_PLUGIN" ]]; then
    echo "ERROR: Qt Cocoa platform plugin was not bundled: $COCOA_PLUGIN" >&2
    exit 1
fi

# MOVis talks to GitHub over HTTPS. On macOS the native Qt TLS backend is
# Secure Transport, so verify that the plugin needed by QtNetwork is present.
SECURE_TRANSPORT_PLUGIN="$PLUGINS/tls/libqsecuretransportbackend.dylib"
if [[ ! -f "$SECURE_TRANSPORT_PLUGIN" ]]; then
    echo "ERROR: Qt Secure Transport TLS plugin was not bundled: $SECURE_TRANSPORT_PLUGIN" >&2
    exit 1
fi

if [[ ! -x "$RESOURCES/node_modules/.bin/jscpd" ]]; then
    echo "ERROR: JSCPD executable is missing from bundled node_modules." >&2
    exit 1
fi

# Remove build-machine-only LC_RPATH entries before validating/signing. qmake
# may leave the selected Qt kit directory in the executable in addition to the
# correct bundle-relative @executable_path/../Frameworks rpath. The bundled
# copy must never rely on /Users/<builder>/Qt, Homebrew, Xcode, etc.
while IFS= read -r -d '' candidate; do
    if ! file "$candidate" | grep -q 'Mach-O'; then
        continue
    fi

    while IFS= read -r rpath; do
        [[ -z "$rpath" ]] && continue
        case "$rpath" in
            /Users/*|/opt/homebrew/*|/usr/local/*|/Applications/*)
                echo "Removing build-machine LC_RPATH from ${candidate#$APP/}: $rpath"
                install_name_tool -delete_rpath "$rpath" "$candidate"
                ;;
        esac
    done < <(
        otool -l "$candidate" 2>/dev/null |
        awk '/cmd LC_RPATH/ { in_rpath=1; next } in_rpath && $1 == "path" { print $2; in_rpath=0 }' |
        sort -u
    )
done < <(find "$CONTENTS" -type f -print0)

# An M-series build must contain an arm64 application executable.
APP_ARCHS="$(lipo -archs "$APP_EXE" 2>/dev/null || true)"
if [[ " $APP_ARCHS " != *" arm64 "* ]]; then
    echo "ERROR: MOVis is not an arm64 build. Found architectures: ${APP_ARCHS:-unknown}" >&2
    echo "Build with an Apple Silicon Qt kit before copying MOVis to the M3 Mac." >&2
    exit 1
fi

# Check every bundled Mach-O image. Qt and Node must be arm64-capable, and no
# file may retain an absolute dependency on the developer's Qt/Homebrew/user
# directory. System libraries under /System/Library and /usr/lib are expected.
DEPENDENCY_ERRORS=0
MISSING_BUNDLED_DEPENDENCY_ERRORS=0
ARCH_ERRORS=0
RPATH_ERRORS=0
MANIFEST="$RESOURCES/mac_bundle_validation.txt"
: > "$MANIFEST"
{
    echo "MOVis macOS bundle validation"
    echo "Application: $APP"
    echo "Application architectures: $APP_ARCHS"
    echo "macdeployqt: $MACDEPLOYQT"
    echo "Node: $($NODE_EXE --version)"
    echo "npm: $($NPM_EXE --version)"
    echo
} >> "$MANIFEST"

while IFS= read -r -d '' candidate; do
    if ! file "$candidate" | grep -q 'Mach-O'; then
        continue
    fi

    relative="${candidate#$APP/}"
    archs="$(lipo -archs "$candidate" 2>/dev/null || true)"

    # For dylibs/framework binaries, otool -L includes LC_ID_DYLIB in the same
    # list as linked libraries.  The ID describes the binary itself; it is NOT
    # a dependency that dyld needs to resolve.  macdeployqt can legitimately
    # leave that self-ID as the absolute path of the copy inside MOVis.app.
    # Treating it as an external dependency caused valid deployments to fail
    # with one false-positive error for almost every bundled Qt binary/plugin.
    install_id="$(otool -D "$candidate" 2>/dev/null | tail -n +2 | head -n 1 || true)"

    echo "[$relative] architectures: $archs" >> "$MANIFEST"
    if [[ -n "$install_id" ]]; then
        echo "  LC_ID_DYLIB: $install_id" >> "$MANIFEST"
    fi

    if [[ " $archs " != *" arm64 "* ]]; then
        echo "ERROR: Bundled Mach-O file does not contain arm64: $relative ($archs)" >&2
        ARCH_ERRORS=$((ARCH_ERRORS + 1))
    fi

    while IFS= read -r dep; do
        [[ -z "$dep" ]] && continue

        # otool -L prints the image's own install-name as its first entry for
        # dylibs/frameworks/plugins. On current macdeployqt builds that self-ID
        # can be emitted as the absolute path of the copy *inside this app*.
        # otool -D is not consistent enough across all Qt plugin/framework
        # images to be our only way of identifying that entry, so also compare
        # the dependency directly to the candidate (and by file identity).
        # This is safe: an image never needs dyld to resolve itself as a
        # dependency.
        self_dependency=false
        if [[ "$dep" == "$candidate" ]]; then
            self_dependency=true
        elif [[ "$dep" == /* && -e "$dep" && "$dep" -ef "$candidate" ]]; then
            self_dependency=true
        elif [[ -n "$install_id" && "$dep" == "$install_id" ]]; then
            self_dependency=true
        fi

        if [[ "$self_dependency" == true ]]; then
            echo "  [self] $dep" >> "$MANIFEST"
            continue
        fi

        echo "  $dep" >> "$MANIFEST"

        case "$dep" in
            @rpath/*)
                # macdeployqt places Qt private frameworks/dylibs in
                # Contents/Frameworks and rewrites their references to
                # @rpath. Make sure the referenced file is actually there.
                rpath_rel="${dep#@rpath/}"
                if [[ ! -e "$FRAMEWORKS/$rpath_rel" ]]; then
                    echo "ERROR: Missing bundled @rpath dependency for $relative: $dep" >&2
                    MISSING_BUNDLED_DEPENDENCY_ERRORS=$((MISSING_BUNDLED_DEPENDENCY_ERRORS + 1))
                fi
                ;;
            @loader_path/*)
                loader_rel="${dep#@loader_path/}"
                loader_target="$(cd "$(dirname "$candidate")" && pwd)/$loader_rel"
                if [[ ! -e "$loader_target" ]]; then
                    echo "ERROR: Missing bundled @loader_path dependency for $relative: $dep" >&2
                    MISSING_BUNDLED_DEPENDENCY_ERRORS=$((MISSING_BUNDLED_DEPENDENCY_ERRORS + 1))
                fi
                ;;
            @executable_path/*)
                executable_rel="${dep#@executable_path/}"
                executable_target="$CONTENTS/MacOS/$executable_rel"
                if [[ ! -e "$executable_target" ]]; then
                    echo "ERROR: Missing bundled @executable_path dependency for $relative: $dep" >&2
                    MISSING_BUNDLED_DEPENDENCY_ERRORS=$((MISSING_BUNDLED_DEPENDENCY_ERRORS + 1))
                fi
                ;;
            /System/Library/*|/usr/lib/*)
                ;;
            "$APP"/*)
                # A non-self absolute path into the current bundle would work
                # only at this exact build location and would break after the
                # app is moved to /Applications or another Mac. Convert known
                # framework references to @rpath instead of silently accepting
                # them. Other absolute in-bundle references remain an error so
                # the validator never certifies a non-relocatable app.
                if [[ "$dep" == "$FRAMEWORKS"/* ]]; then
                    framework_rel="${dep#$FRAMEWORKS/}"
                    new_dep="@rpath/$framework_rel"
                    echo "Rewriting absolute in-bundle dependency in $relative:" >&2
                    echo "  $dep -> $new_dep" >&2
                    install_name_tool -change "$dep" "$new_dep" "$candidate"
                    echo "  [rewritten] $new_dep" >> "$MANIFEST"
                    if [[ ! -e "$FRAMEWORKS/$framework_rel" ]]; then
                        echo "ERROR: Rewritten framework dependency does not exist for $relative: $new_dep" >&2
                        MISSING_BUNDLED_DEPENDENCY_ERRORS=$((MISSING_BUNDLED_DEPENDENCY_ERRORS + 1))
                    fi
                else
                    echo "ERROR: Non-relocatable absolute in-bundle dependency remains in $relative: $dep" >&2
                    DEPENDENCY_ERRORS=$((DEPENDENCY_ERRORS + 1))
                fi
                ;;
            *)
                echo "ERROR: External non-system dependency remains in $relative: $dep" >&2
                DEPENDENCY_ERRORS=$((DEPENDENCY_ERRORS + 1))
                ;;
        esac
    done < <(otool -L "$candidate" 2>/dev/null | tail -n +2 | awk '{print $1}')

    while IFS= read -r rpath; do
        [[ -z "$rpath" ]] && continue
        echo "  LC_RPATH: $rpath" >> "$MANIFEST"
        case "$rpath" in
            @executable_path/*|@loader_path/*|@rpath/*|/System/Library/*|/usr/lib/*)
                ;;
            *)
                echo "ERROR: External LC_RPATH remains in $relative: $rpath" >&2
                RPATH_ERRORS=$((RPATH_ERRORS + 1))
                ;;
        esac
    done < <(
        otool -l "$candidate" 2>/dev/null |
        awk '/cmd LC_RPATH/ { in_rpath=1; next } in_rpath && $1 == "path" { print $2; in_rpath=0 }' |
        sort -u
    )

    echo >> "$MANIFEST"
done < <(find "$CONTENTS" -type f -print0)

if [[ "$ARCH_ERRORS" -ne 0 ]]; then
    echo "ERROR: $ARCH_ERRORS bundled Mach-O file(s) are not Apple Silicon compatible." >&2
    exit 1
fi

if [[ "$MISSING_BUNDLED_DEPENDENCY_ERRORS" -ne 0 ]]; then
    echo "ERROR: $MISSING_BUNDLED_DEPENDENCY_ERRORS bundle-relative library reference(s) do not resolve." >&2
    echo "See $MANIFEST for the complete dependency listing." >&2
    exit 1
fi

if [[ "$DEPENDENCY_ERRORS" -ne 0 ]]; then
    echo "ERROR: $DEPENDENCY_ERRORS external non-system library reference(s) remain." >&2
    echo "See $MANIFEST for the complete dependency listing." >&2
    exit 1
fi

if [[ "$RPATH_ERRORS" -ne 0 ]]; then
    echo "ERROR: $RPATH_ERRORS external runtime search path(s) remain." >&2
    echo "See $MANIFEST for the complete dependency listing." >&2
    exit 1
fi

# Verify that the executable has a bundle-relative framework search path.
if ! otool -l "$APP_EXE" | grep -A2 LC_RPATH | grep -q '@executable_path/../Frameworks'; then
    echo "ERROR: MOVis executable is missing @executable_path/../Frameworks LC_RPATH." >&2
    exit 1
fi

# macdeployqt normally ad-hoc signs by default. We deliberately disabled that
# above because Node and node_modules are added afterwards; modifying Resources
# after signing would invalidate the application signature. Sign only after the
# complete bundle is finalized.
if ! command -v codesign >/dev/null 2>&1; then
    echo "ERROR: codesign is required to finalize the macOS application bundle." >&2
    exit 1
fi

echo "Applying final ad-hoc signature to the complete app bundle..."
codesign --force --deep --sign - "$APP"

if ! codesign --verify --deep --strict --verbose=2 "$APP"; then
    echo "ERROR: Final MOVis.app code-signature verification failed." >&2
    exit 1
fi

# Remove quarantine metadata that may have leaked from copied development
# resources. This does not bypass Gatekeeper on the destination Mac; it simply
# avoids shipping stale quarantine xattrs from the build tree.
xattr -cr "$APP" 2>/dev/null || true

echo "============================================================"
echo "MOVis standalone macOS bundle validation PASSED"
echo "Qt frameworks: bundled"
echo "Cocoa plugin: bundled"
echo "Secure Transport TLS plugin: bundled"
echo "Apple Silicon architecture: verified"
echo "Bundled Node/npm: verified"
echo "JSCPD/node_modules: verified"
echo "Bundle-relative dylib references: resolved"
echo "External non-system dylib references: none"
echo "External build-machine LC_RPATH entries: none"
echo "Final code signature: verified"
echo "Validation manifest: $MANIFEST"
echo "============================================================"

if [[ -n "$DEPLOY_DIR" ]]; then
    mkdir -p "$DEPLOY_DIR"
    rm -rf "$DEPLOY_DIR/$(basename "$APP")"
    cp -R "$APP" "$DEPLOY_DIR/"
    echo "Copied standalone app to: $DEPLOY_DIR/$(basename "$APP")"
fi
