#!/data/data/com.termux/files/usr/bin/bash
# build.sh — Build abt on Termux (Android 15)
# Run this once to compile the abt binary itself.
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build_abt"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

log_info()  { echo -e "${GREEN}[abt]${NC} $*"; }
log_warn()  { echo -e "${YELLOW}[abt]${NC} $*"; }
log_error() { echo -e "${RED}[abt]${NC} $*" >&2; }
log_step()  { echo -e "\n${CYAN}──── $* ────${NC}"; }

# ── Check dependencies ────────────────────────────────────────────────────────
log_step "Checking Termux dependencies"

MISSING=()
for pkg in cmake clang make python; do
    if ! command -v "$pkg" &>/dev/null; then
        MISSING+=("$pkg")
    fi
done

if [ ${#MISSING[@]} -gt 0 ]; then
    log_warn "Missing packages: ${MISSING[*]}"
    log_info "Installing with pkg..."
    pkg install -y "${MISSING[@]}"
fi

# Optional: libcurl for dependency fetching
if ! pkg list-installed 2>/dev/null | grep -q libcurl; then
    log_warn "libcurl not found — dependency fetching will be disabled"
    log_warn "Install with: pkg install libcurl"
    EXTRA_FLAGS="-DABT_USE_CURL=OFF"
fi

# ── Generate tiny_dex ─────────────────────────────────────────────────────────
log_step "Generating runtime assets"
python3 "$SCRIPT_DIR/runtime/tiny_dex/generate_tiny_dex.py"
log_info "classes.dex: OK"

# ── Configure ─────────────────────────────────────────────────────────────────
log_step "Configuring CMake"
mkdir -p "$BUILD_DIR"

cmake -B "$BUILD_DIR" \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=clang++ \
      -DCMAKE_C_COMPILER=clang \
      ${EXTRA_FLAGS} \
      "$SCRIPT_DIR"

# ── Build ─────────────────────────────────────────────────────────────────────
log_step "Building abt"
JOBS=$(nproc 2>/dev/null || echo 4)
cmake --build "$BUILD_DIR" --parallel "$JOBS"

# ── Verify ────────────────────────────────────────────────────────────────────
log_step "Verifying"
ABT_BIN="$SCRIPT_DIR/bin/abt"
if [ -f "$ABT_BIN" ]; then
    log_info "Binary: $ABT_BIN"
    ls -lh "$ABT_BIN"
    file "$ABT_BIN" 2>/dev/null || true
else
    log_error "Binary not found at $ABT_BIN"
    exit 1
fi

# ── PATH suggestion ────────────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}✓ Build successful!${NC}"
echo ""
echo "Add to your PATH:"
echo "  export PATH=\"$SCRIPT_DIR/bin:\$PATH\""
echo ""
echo "Then use:"
echo "  abt info                   # check environment"
echo "  abt build                  # build your project"
echo "  abt build --release        # release build"
echo "  abt watch                  # incremental rebuild"
echo "  abt install                # build + adb install"
echo ""
echo "Create a project file (AbtProject.cmake):"
echo '  abt_project(MyApp)'
echo '  abt_set_package(com.example.myapp)'
echo '  abt_set_min_sdk(24)'
echo '  abt_add_sources(src/main.cpp)'
echo '  abt_link_libraries(android log EGL GLESv3)'
