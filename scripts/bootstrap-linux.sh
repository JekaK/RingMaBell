#!/usr/bin/env bash
set -euo pipefail

REPO_URL="${REPO_URL:-https://github.com/JekaK/RingMaBell.git}"
BRANCH="${BRANCH:-master}"
SOURCE_DIR="${SOURCE_DIR:-/opt/ringmabell-src}"
BUILD_DIR="${BUILD_DIR:-${SOURCE_DIR}/build}"
INSTALL_DIR="${INSTALL_DIR:-/opt/ringmabell}"
TIMEZONE="${TIMEZONE:-Europe/Kyiv}"
GITHUB_TOKEN="${GITHUB_TOKEN:-${GH_TOKEN:-}}"
PRIVATE_REPO="${PRIVATE_REPO:-true}"

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run as root: sudo bash $0" >&2
    exit 1
fi

if ! command -v apt-get >/dev/null 2>&1; then
    echo "This bootstrap script supports Debian/Ubuntu systems with apt-get." >&2
    exit 1
fi

if ! command -v systemctl >/dev/null 2>&1; then
    echo "systemd is required. Install a Debian/Ubuntu system with systemd enabled." >&2
    exit 1
fi

export DEBIAN_FRONTEND=noninteractive

git_with_auth() {
    if [[ -n "${GITHUB_TOKEN}" ]]; then
        git -c "http.https://github.com/.extraheader=AUTHORIZATION: bearer ${GITHUB_TOKEN}" "$@"
    else
        git "$@"
    fi
}

needs_private_repo_token() {
    [[ "${PRIVATE_REPO}" != "false" && "${PRIVATE_REPO}" != "0" && "${PRIVATE_REPO}" != "no" ]]
}

if [[ -z "${GITHUB_TOKEN}" ]] && needs_private_repo_token; then
    if [[ -t 0 ]]; then
        echo "Private GitHub repository access requires a read-only token."
        read -r -s -p "GitHub token: " GITHUB_TOKEN
        echo
    fi

    if [[ -z "${GITHUB_TOKEN}" ]]; then
        echo "GITHUB_TOKEN is required for private repository access." >&2
        echo "Use a fine-grained token with Contents: Read-only for JekaK/RingMaBell." >&2
        exit 1
    fi
fi

echo "Installing base packages..."
apt-get update
apt-get install -y \
    ca-certificates \
    curl \
    sudo \
    tzdata \
    git \
    build-essential \
    cmake \
    pkg-config \
    mpg123 \
    alsa-utils

if apt-cache show systemd-timesyncd >/dev/null 2>&1; then
    apt-get install -y systemd-timesyncd
    systemctl enable --now systemd-timesyncd.service || true
fi

if command -v timedatectl >/dev/null 2>&1; then
    timedatectl set-timezone "${TIMEZONE}" || true
fi

if [[ -d "${SOURCE_DIR}/.git" ]]; then
    echo "Updating existing source tree in ${SOURCE_DIR}..."
    git_with_auth -C "${SOURCE_DIR}" fetch --all --prune
    git_with_auth -C "${SOURCE_DIR}" checkout "${BRANCH}"
    git_with_auth -C "${SOURCE_DIR}" pull --ff-only origin "${BRANCH}"
elif [[ -e "${SOURCE_DIR}" ]] && [[ -n "$(find "${SOURCE_DIR}" -mindepth 1 -print -quit 2>/dev/null)" ]]; then
    echo "${SOURCE_DIR} exists and is not an empty git repository." >&2
    echo "Move it away or set SOURCE_DIR=/another/path and run this script again." >&2
    exit 1
else
    echo "Cloning ${REPO_URL} into ${SOURCE_DIR}..."
    install -d "$(dirname "${SOURCE_DIR}")"
    git_with_auth clone --branch "${BRANCH}" "${REPO_URL}" "${SOURCE_DIR}"
fi

unset GITHUB_TOKEN GH_TOKEN

echo "Building RingMaBell..."
JOBS="$(nproc 2>/dev/null || echo 2)"
cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release --parallel "${JOBS}"

echo "Installing systemd service..."
INSTALL_DIR="${INSTALL_DIR}" "${SOURCE_DIR}/scripts/install-linux-systemd.sh"

echo
echo "Done."
echo "Put MP3 files into: ${INSTALL_DIR}/sounds"
echo "Check service: sudo systemctl status ringmabell"
echo "Follow logs:    sudo journalctl -u ringmabell -f"
