#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${BUILD_DIR:-$ROOT/build-linux-test}"
OUT="$ROOT/dist/linux-test"
mkdir -p "$BUILD" "$OUT"
for part in third_party/qt-toast libUtil2 libExprEval libCwp libSegD GxLib SeiSeeMp; do
    mkdir -p "$BUILD/$part"
    (cd "$BUILD/$part"; qmake "$ROOT/$part/$(basename "$part").pro" CONFIG+=release; make -j"${JOBS:-2}")
done
mkdir -p "$BUILD/Regression"
(cd "$BUILD/Regression"; qmake "$ROOT/SeiSeeMp/SeiSeeMp.pro" CONFIG+=release CONFIG+=metrics_regression; make -j"${JOBS:-2}")
for scale in 1 2; do
    mkdir -p "$OUT/qa/scale$scale"
    QT_QPA_PLATFORM=offscreen QT_SCALE_FACTOR="$scale" SEISEE_TEST_OUT="$OUT/qa/scale$scale" \
        "$BUILD/Regression/metrics_regression" -o "$OUT/qa/scale$scale/results.txt,txt"
done
APP="$OUT/seisee-test-portable"
mkdir -p "$APP/usr/bin" "$APP/usr/share/applications" "$APP/usr/share/icons/hicolor/256x256/apps"
install -m755 "$BUILD/SeiSeeMp/seisee-test" "$APP/usr/bin/seisee-test"
install -m644 "$ROOT/SeiSeeMp/images/SeiSeeMp.png" "$APP/usr/share/icons/hicolor/256x256/apps/seisee-test.png"
cat > "$APP/usr/share/applications/seisee-test.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=seisee-test
Exec=seisee-test %F
Icon=seisee-test
Categories=Science;Education;
Terminal=false
EOF
export APPIMAGE_EXTRACT_AND_RUN=1 QMAKE="$(command -v qmake)"
linuxdeploy --appdir "$APP" --executable "$APP/usr/bin/seisee-test" \
    --desktop-file "$APP/usr/share/applications/seisee-test.desktop" \
    --icon-file "$APP/usr/share/icons/hicolor/256x256/apps/seisee-test.png" --plugin qt
cat > "$APP/seisee-test" <<'EOF'
#!/bin/sh
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec "$HERE/AppRun" "$@"
EOF
chmod 755 "$APP/seisee-test"
install -m644 "$ROOT/docs/fk-filter.md" "$APP/FK-guide.md"
cp "$ROOT/docs/linux-test.md" "$APP/README.md"
file "$APP/usr/bin/seisee-test" > "$OUT/qa/elf.txt"
LD_LIBRARY_PATH="$APP/usr/lib" ldd "$APP/usr/bin/seisee-test" > "$OUT/qa/ldd.txt"
if grep -q 'not found' "$OUT/qa/ldd.txt"; then cat "$OUT/qa/ldd.txt"; exit 1; fi
# The application should stay in its event loop without FUSE or Qt from the host.
set +e
QT_QPA_PLATFORM=offscreen timeout 5 "$APP/seisee-test" > "$OUT/qa/portable-startup.log" 2>&1
status=$?
set -e
test "$status" -eq 124
ARCH=x86_64 appimagetool "$APP" "$OUT/seisee-test"
chmod 755 "$OUT/seisee-test"
set +e
QT_QPA_PLATFORM=offscreen timeout 5 "$OUT/seisee-test" --appimage-extract-and-run > "$OUT/qa/appimage-startup.log" 2>&1
status=$?
set -e
test "$status" -eq 124
tar -czf "$OUT/seisee-test-linux-x86_64.tar.gz" -C "$OUT" seisee-test-portable
(cd "$OUT"; sha256sum seisee-test seisee-test-linux-x86_64.tar.gz > SHA256SUMS)
