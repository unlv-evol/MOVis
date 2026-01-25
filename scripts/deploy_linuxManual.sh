#!/usr/bin/env bash
set -euo pipefail

# -----------------------------
# Config (edit if needed)
# -----------------------------
APP_NAME="MOVis"
BUILD_DIR="../build/Desktop_Qt_6_8_3-Release"   # where the built binary is
DEPLOY_DIR="./MOVis-Release"                   # output folder

QT_ROOT="$HOME/Qt/6.8.3/gcc_64"
QT_LIB_DIR="$QT_ROOT/lib"
QT_PLUGIN_DIR="$QT_ROOT/plugins"

# Your app runtime folders (live in repo root; copied next to executable)
SRC_ROOT=".."   # scripts/ is one level under repo root
RUNTIME_ITEMS=( "Pareco" "assets" "include" "logs" )

# -----------------------------
# Helpers
# -----------------------------
need() { command -v "$1" >/dev/null 2>&1 || { echo "Error: Missing command: $1"; exit 1; }; }

need rsync
need ldd
need awk
need patchelf
need readlink
need mkdir
need cp
need rm

if [[ ! -f "$BUILD_DIR/$APP_NAME" ]]; then
  echo "Error: Application executable not found: $BUILD_DIR/$APP_NAME"
  exit 1
fi

if [[ ! -d "$QT_LIB_DIR" ]]; then
  echo "Error: Qt lib dir not found: $QT_LIB_DIR"
  exit 1
fi

if [[ ! -d "$QT_PLUGIN_DIR" ]]; then
  echo "Error: Qt plugin dir not found: $QT_PLUGIN_DIR"
  exit 1
fi

echo "== Deploying $APP_NAME"
echo "Build:  $BUILD_DIR/$APP_NAME"
echo "Deploy: $DEPLOY_DIR"

# -----------------------------
# Create deployment structure
# -----------------------------
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"/{libs,plugins/platforms,plugins/imageformats}

# -----------------------------
# Copy executable
# -----------------------------
echo "== Copying executable"
cp -L "$BUILD_DIR/$APP_NAME" "$DEPLOY_DIR/"
chmod +x "$DEPLOY_DIR/$APP_NAME"

# -----------------------------
# Copy runtime folders
# -----------------------------
echo "== Copying runtime folders"
for item in "${RUNTIME_ITEMS[@]}"; do
  if [[ -d "$SRC_ROOT/$item" ]]; then
    mkdir -p "$DEPLOY_DIR/$item"
    rsync -a --delete "$SRC_ROOT/$item/" "$DEPLOY_DIR/$item/"
    echo "  + $item"
  else
    echo "  ! WARNING: $SRC_ROOT/$item not found"
  fi
done

# -----------------------------
# Copy libraries (with deps)
# -----------------------------
copy_library() {
  local lib="$1"
  [[ -f "$lib" ]] || return 0

  # Dereference symlinks to copy the real file
  local real
  real="$(readlink -f "$lib" || true)"
  [[ -n "$real" && -f "$real" ]] || real="$lib"

  local base
  base="$(basename "$lib")"
  local realname
  realname="$(basename "$real")"

  # Copy real file (avoid duplicates)
  if [[ ! -f "$DEPLOY_DIR/libs/$realname" ]]; then
    cp -L "$real" "$DEPLOY_DIR/libs/"
  fi

  # Ensure the SONAME symlink exists (e.g. libQt6XcbQpa.so.6 -> libQt6XcbQpa.so.6.8.3)
  ln -sf "$realname" "$DEPLOY_DIR/libs/$base" 2>/dev/null || true

  # Copy direct dependencies (recursively via repeated calls from outer loops)
  ldd "$real" | awk '/=> \// {print $3} /^\// {print $1}' | while read -r dep; do
    [[ -f "$dep" ]] || continue
    local dep_real dep_name
    dep_real="$(readlink -f "$dep" || true)"
    [[ -n "$dep_real" && -f "$dep_real" ]] || dep_real="$dep"
    dep_name="$(basename "$dep_real")"
    if [[ ! -f "$DEPLOY_DIR/libs/$dep_name" ]]; then
      cp -L "$dep_real" "$DEPLOY_DIR/libs/"
    fi
  done
}

echo "== Copying app shared libs from ldd"
ldd "$BUILD_DIR/$APP_NAME" | awk '/=> \// {print $3} /^\// {print $1}' | while read -r lib; do
  copy_library "$lib"
done

echo "== Explicitly copying Qt libs (includes libQt6XcbQpa)"
for lib in \
  libQt6Core.so.6 \
  libQt6Gui.so.6 \
  libQt6Widgets.so.6 \
  libQt6Network.so.6 \
  libQt6Concurrent.so.6 \
  libQt6XcbQpa.so.6
do
  if [[ -f "$QT_LIB_DIR/$lib" ]]; then
    copy_library "$QT_LIB_DIR/$lib"
    echo "  + $lib"
  else
    echo "  ! WARNING: $QT_LIB_DIR/$lib not found"
  fi
done

# -----------------------------
# Copy Qt platform plugin (xcb) + imageformats
# -----------------------------
echo "== Copying Qt platform plugin (xcb)"
QXCB="$QT_PLUGIN_DIR/platforms/libqxcb.so"
if [[ ! -f "$QXCB" ]]; then
  echo "Error: libqxcb.so not found at: $QXCB"
  exit 1
fi
cp -L "$QXCB" "$DEPLOY_DIR/plugins/platforms/"
copy_library "$QXCB"

echo "== Copying Qt imageformats plugins"
if compgen -G "$QT_PLUGIN_DIR/imageformats/*.so" > /dev/null; then
  cp -L "$QT_PLUGIN_DIR/imageformats/"*.so "$DEPLOY_DIR/plugins/imageformats/" || true
else
  echo "  ! WARNING: no imageformat plugins found"
fi

# -----------------------------
# RPATH: make executable/plugins find bundled libs
# -----------------------------
echo "== Setting RPATH"
patchelf --set-rpath '$ORIGIN/libs' "$DEPLOY_DIR/$APP_NAME" || true
patchelf --set-rpath '$ORIGIN/../../libs' "$DEPLOY_DIR/plugins/platforms/libqxcb.so" || true

# -----------------------------
# qt.conf: make ./MOVis find plugins without env vars (optional but recommended)
# -----------------------------
cat > "$DEPLOY_DIR/qt.conf" <<'EOF'
[Paths]
Plugins = plugins
EOF

# -----------------------------
# run.sh: wrapper to set env vars (recommended)
# -----------------------------
echo "== Writing run.sh"
cat > "$DEPLOY_DIR/run.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

export LD_LIBRARY_PATH="$HERE/libs:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$HERE/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

exec "$HERE/MOVis" "$@"
EOF
chmod +x "$DEPLOY_DIR/run.sh"

# -----------------------------
# Sanity checks
# -----------------------------
echo "== Sanity check: missing deps for libqxcb.so (should be none of Qt libs)"
ldd "$DEPLOY_DIR/plugins/platforms/libqxcb.so" | grep "not found" || echo "  (none)"

echo "== Done"
echo "Run with: $DEPLOY_DIR/run.sh"
echo
echo "NOTE: If you see an error about xcb-cursor0/libxcb-cursor0 on another machine,"
echo "install: sudo apt install -y libxcb-cursor0"
