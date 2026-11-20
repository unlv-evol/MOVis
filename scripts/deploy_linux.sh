#!/usr/bin/env bash
set -euo pipefail

APP_NAME="MOVis"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/Desktop_Qt_6_11_0-Release"
DEPLOY_DIR="${PROJECT_ROOT}/dist/MOVis-Linux"

QT_DIR="${HOME}/Qt/6.11.0/gcc_64"
QMAKE="${QT_DIR}/bin/qmake"

rm -rf "$BUILD_DIR" "$DEPLOY_DIR"
mkdir -p "$BUILD_DIR" "$DEPLOY_DIR"

cd "$BUILD_DIR"

"$QMAKE" "${PROJECT_ROOT}/MOVis.pro" CONFIG+=release
make -j"$(nproc)"

cd "$PROJECT_ROOT"

cp "$BUILD_DIR/MOVis" "$DEPLOY_DIR/MOVis"

cp -r "$PROJECT_ROOT/assets" "$DEPLOY_DIR/assets"
cp -r "$PROJECT_ROOT/include" "$DEPLOY_DIR/include"
cp -r "$PROJECT_ROOT/logs" "$DEPLOY_DIR/logs"
cp -r "$PROJECT_ROOT/tools" "$DEPLOY_DIR/tools"
cp "$PROJECT_ROOT/package.json" "$DEPLOY_DIR/package.json"
cp "$PROJECT_ROOT/package-lock.json" "$DEPLOY_DIR/package-lock.json"

mkdir -p "$DEPLOY_DIR/lib"
mkdir -p "$DEPLOY_DIR/plugins/platforms"
mkdir -p "$DEPLOY_DIR/plugins/imageformats"
mkdir -p "$DEPLOY_DIR/plugins/styles"
mkdir -p "$DEPLOY_DIR/plugins/iconengines"
mkdir -p "$DEPLOY_DIR/qml"

cp "$QT_DIR/lib/libQt6Core.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6Gui.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6Qml.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6QmlModels.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6Quick.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6QuickControls2.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6QuickTemplates2.so.6" "$DEPLOY_DIR/lib/"
cp "$QT_DIR/lib/libQt6QuickDialogs2.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true
cp "$QT_DIR/lib/libQt6QuickLayouts.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true
cp "$QT_DIR/lib/libQt6OpenGL.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true
cp "$QT_DIR/lib/libQt6Network.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true
cp "$QT_DIR/lib/libQt6DBus.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true
cp "$QT_DIR/lib/libQt6XcbQpa.so.6" "$DEPLOY_DIR/lib/" 2>/dev/null || true

cp "$QT_DIR/plugins/platforms/libqxcb.so" "$DEPLOY_DIR/plugins/platforms/"
cp "$QT_DIR/plugins/platforms/libqoffscreen.so" "$DEPLOY_DIR/plugins/platforms/" 2>/dev/null || true
cp "$QT_DIR/plugins/imageformats/"*.so "$DEPLOY_DIR/plugins/imageformats/" 2>/dev/null || true
cp "$QT_DIR/plugins/styles/"*.so "$DEPLOY_DIR/plugins/styles/" 2>/dev/null || true
cp "$QT_DIR/plugins/iconengines/"*.so "$DEPLOY_DIR/plugins/iconengines/" 2>/dev/null || true
cp -r "$QT_DIR/qml/." "$DEPLOY_DIR/qml/"

cat > "$DEPLOY_DIR/qt.conf" <<EOF
[Paths]
Plugins = plugins
Qml2Imports = qml
EOF

cat > "$DEPLOY_DIR/run.sh" <<'EOF'
#!/usr/bin/env bash
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export LD_LIBRARY_PATH="$HERE/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$HERE/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
export QML2_IMPORT_PATH="$HERE/qml"

cd "$HERE"
exec "$HERE/MOVis" "$@"
EOF

chmod +x "$DEPLOY_DIR/run.sh"
chmod +x "$DEPLOY_DIR/MOVis"

echo "Portable Linux build created at:"
echo "$DEPLOY_DIR"
