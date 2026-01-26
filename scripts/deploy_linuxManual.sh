#!/usr/bin/env bash
set -euo pipefail

# -------- Config (edit if needed) ----------
APP_NAME="MOVis"
BUILD_DIR="../build/Desktop_Qt_6_8_3-Release"
OUT_DIR="./MOVis-Release"

QT_ROOT="$HOME/Qt/6.8.3/gcc_64"
QT_LIB_DIR="$QT_ROOT/lib"
QT_PLUGIN_DIR="$QT_ROOT/plugins"

# These are runtime folders your app needs next to the executable
RUNTIME_ITEMS=( "Pareco" "assets" "include" "logs" )

# ------------------------------------------
die(){ echo "Error: $*" >&2; exit 1; }
need(){ command -v "$1" >/dev/null 2>&1 || die "Missing command: $1"; }

need rsync
need ldd
need patchelf
need readelf
need find
need awk
need sort

[[ -f "$BUILD_DIR/$APP_NAME" ]] || die "Executable not found: $BUILD_DIR/$APP_NAME"
[[ -d "$QT_LIB_DIR" ]] || die "Qt lib dir not found: $QT_LIB_DIR"
[[ -d "$QT_PLUGIN_DIR" ]] || die "Qt plugin dir not found: $QT_PLUGIN_DIR"

echo "== Deploying $APP_NAME"
echo "Build dir: $BUILD_DIR"
echo "Output:    $OUT_DIR"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"/{libs,plugins}

# 1) Copy executable
rsync -a "$BUILD_DIR/$APP_NAME" "$OUT_DIR/$APP_NAME"
chmod +x "$OUT_DIR/$APP_NAME"

# 2) Copy runtime folders/files (Pareco/assets/include/logs)
echo "== Copying runtime items"
for item in "${RUNTIME_ITEMS[@]}"; do
  if [[ -e "$BUILD_DIR/$item" ]]; then
    echo "  + $item"
    if [[ -d "$BUILD_DIR/$item" ]]; then
      mkdir -p "$OUT_DIR/$item"
      rsync -a --delete "$BUILD_DIR/$item/" "$OUT_DIR/$item/"
    else
      rsync -a "$BUILD_DIR/$item" "$OUT_DIR/"
    fi
  else
    echo "  ! WARNING: '$item' not found in $BUILD_DIR (check your .pro copy_third or paths)"
  fi
done

# If linuxdeployqt exists, prefer it (best distribution story)
if command -v linuxdeployqt >/dev/null 2>&1; then
  echo "== Using linuxdeployqt (preferred)"
  # Create a minimal desktop file so linuxdeployqt is happy
  cat > "$OUT_DIR/$APP_NAME.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
Exec=$APP_NAME
Icon=$APP_NAME
Categories=Utility;
EOF

  # Provide a dummy icon if you don’t have one yet
  mkdir -p "$OUT_DIR/icons"
  : > "$OUT_DIR/$APP_NAME.png"

  # linuxdeployqt expects to run in the AppDir root typically
  pushd "$OUT_DIR" >/dev/null
  linuxdeployqt "./$APP_NAME" -qmake="$QT_ROOT/bin/qmake" -no-translations -bundle-non-qt-libs || true
  popd >/dev/null

  echo
  echo "Done (linuxdeployqt). Try:"
  echo "  $OUT_DIR/$APP_NAME"
  echo "or"
  echo "  QT_QPA_PLATFORM=xcb $OUT_DIR/$APP_NAME"
  exit 0
fi

echo "== linuxdeployqt not found -> manual bundle mode"

# -------- Manual bundling below --------
collect_deps() {
  local bin="$1"
  ldd "$bin" | awk '
    $3 ~ /^\// {print $3}
    $1 ~ /^\// {print $1}
  ' | sort -u
}

copy_real_lib() {
  local src="$1"
  local dst_dir="$2"
  local real
  real="$(readlink -f "$src")" || return 0
  [[ -f "$real" ]] || return 0
  local realname base
  realname="$(basename "$real")"
  base="$(basename "$src")"
  rsync -aL "$real" "$dst_dir/$realname"
  ln -sf "$realname" "$dst_dir/$base"
}

# 3) Copy shared libs (closure: deps of deps)
echo "== Copying shared library dependencies into libs/ (dereference symlinks)"
declare -A seen=()
queue=("$OUT_DIR/$APP_NAME")

while ((${#queue[@]})); do
  target="${queue[0]}"
  queue=("${queue[@]:1}")

  while IFS= read -r lib; do
    [[ -z "$lib" ]] && continue
    [[ -e "$lib" ]] || continue

    real="$(readlink -f "$lib")" || continue
    realname="$(basename "$real")"

    if [[ -z "${seen[$realname]:-}" ]]; then
      seen["$realname"]=1
      copy_real_lib "$lib" "$OUT_DIR/libs"
      echo "  + $(basename "$lib")"
      queue+=("$OUT_DIR/libs/$realname")
    fi
  done < <(collect_deps "$target")
done

# 4) Copy ALL Qt plugins (don’t miss anything)
echo "== Copying Qt plugins (entire tree)"
rsync -aL --delete "$QT_PLUGIN_DIR/" "$OUT_DIR/plugins/"

# 5) Set RPATH
echo "== Setting RPATH"
patchelf --force-rpath --set-rpath '$ORIGIN/libs' "$OUT_DIR/$APP_NAME"

# Make plugins find libs too
find "$OUT_DIR/plugins" -type f -name "*.so" -print0 | while IFS= read -r -d '' sofile; do
  patchelf --force-rpath --set-rpath '$ORIGIN/../../libs' "$sofile" 2>/dev/null || true
done

# 6) Create run script (forces xcb by default)
cat > "$OUT_DIR/run.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"

export QT_PLUGIN_PATH="$HERE/plugins"
export LD_LIBRARY_PATH="$HERE/libs:${LD_LIBRARY_PATH:-}"

# Force X11/xcb to avoid Wayland plugin load issues by default
export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"

exec "$HERE/MOVis" "$@"
EOF
chmod +x "$OUT_DIR/run.sh"

cp -L "$HOME/Qt/6.8.3/gcc_64/lib/libQt6XcbQpa.so.6.8.3" ./MOVis-Release/libs/
ln -sf libQt6XcbQpa.so.6.8.3 ./MOVis-Release/libs/libQt6XcbQpa.so.6


echo "== Verify"
echo "-- RPATH/RUNPATH:"
readelf -d "$OUT_DIR/$APP_NAME" | grep -E 'RPATH|RUNPATH' || true
echo "-- Missing libs:"
ldd "$OUT_DIR/$APP_NAME" | awk '/not found/{print $1}' || true

echo
echo "Done (manual). Run with:"
echo "  $OUT_DIR/run.sh"
echo
echo "If it fails on xcb plugin deps, run:"
echo "  ldd $OUT_DIR/plugins/platforms/libqxcb.so | grep 'not found' || true"
