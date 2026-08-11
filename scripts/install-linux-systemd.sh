#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi

INSTALL_DIR="${INSTALL_DIR:-/opt/ringmabell}"
SERVICE_USER="${SERVICE_USER:-ringmabell}"
SERVICE_NAME="${SERVICE_NAME:-ringmabell.service}"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BIN_SOURCE="${SOURCE_DIR}/build/ringmabell"
if [[ ! -x "${BIN_SOURCE}" && -x "${SOURCE_DIR}/build/Release/ringmabell" ]]; then
    BIN_SOURCE="${SOURCE_DIR}/build/Release/ringmabell"
fi

if [[ ! -x "${BIN_SOURCE}" ]]; then
    echo "Built binary not found. Run: cmake -S . -B build && cmake --build build --config Release" >&2
    exit 1
fi

if ! command -v mpg123 >/dev/null 2>&1; then
    echo "Warning: mpg123 is not installed. Install it first, e.g.:" >&2
    echo "  sudo apt install mpg123" >&2
fi

if ! id "${SERVICE_USER}" >/dev/null 2>&1; then
    useradd --system --home-dir "${INSTALL_DIR}" --shell /usr/sbin/nologin "${SERVICE_USER}"
fi

if getent group audio >/dev/null 2>&1; then
    usermod -aG audio "${SERVICE_USER}"
fi

install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}/bin"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}/config"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}/sounds"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}/data"
install -d -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${INSTALL_DIR}/logs"

install -m 0755 -o root -g root "${BIN_SOURCE}" "${INSTALL_DIR}/bin/ringmabell"

if [[ ! -f "${INSTALL_DIR}/config/settings.conf" ]]; then
    install -m 0644 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${SOURCE_DIR}/config/settings.conf" "${INSTALL_DIR}/config/settings.conf"
else
    install -m 0644 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${SOURCE_DIR}/config/settings.conf" "${INSTALL_DIR}/config/settings.conf.example"
    echo "Existing settings.conf kept. New sample copied to settings.conf.example."
fi

if [[ ! -f "${INSTALL_DIR}/config/schedule.csv" ]]; then
    install -m 0644 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${SOURCE_DIR}/config/schedule.csv" "${INSTALL_DIR}/config/schedule.csv"
else
    install -m 0644 -o "${SERVICE_USER}" -g "${SERVICE_USER}" "${SOURCE_DIR}/config/schedule.csv" "${INSTALL_DIR}/config/schedule.csv.example"
    echo "Existing schedule.csv kept. New sample copied to schedule.csv.example."
fi

cp -n "${SOURCE_DIR}"/sounds/* "${INSTALL_DIR}/sounds/" 2>/dev/null || true
chown -R "${SERVICE_USER}:${SERVICE_USER}" "${INSTALL_DIR}/config" "${INSTALL_DIR}/sounds" "${INSTALL_DIR}/data" "${INSTALL_DIR}/logs"

cat > "/etc/systemd/system/${SERVICE_NAME}" <<UNIT
[Unit]
Description=RingMaBell school bell and air alert server
Wants=network-online.target
After=network-online.target sound.target

[Service]
Type=simple
User=${SERVICE_USER}
SupplementaryGroups=audio
WorkingDirectory=${INSTALL_DIR}
ExecStart=${INSTALL_DIR}/bin/ringmabell --config ${INSTALL_DIR}/config/settings.conf run
Restart=always
RestartSec=5
Environment=HOME=${INSTALL_DIR}
NoNewPrivileges=true
ProtectSystem=full
ProtectHome=true
ReadWritePaths=${INSTALL_DIR}/data ${INSTALL_DIR}/logs ${INSTALL_DIR}/config ${INSTALL_DIR}/sounds

[Install]
WantedBy=multi-user.target
UNIT

systemctl daemon-reload
systemctl enable "${SERVICE_NAME}"
systemctl restart "${SERVICE_NAME}"

echo "Installed and started ${SERVICE_NAME}."
echo "Status: sudo systemctl status ${SERVICE_NAME}"
echo "Logs:   sudo journalctl -u ${SERVICE_NAME} -f"
