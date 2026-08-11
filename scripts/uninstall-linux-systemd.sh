#!/usr/bin/env bash
set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi

SERVICE_NAME="${SERVICE_NAME:-ringmabell.service}"

systemctl disable --now "${SERVICE_NAME}" 2>/dev/null || true
rm -f "/etc/systemd/system/${SERVICE_NAME}"
systemctl daemon-reload

echo "Removed ${SERVICE_NAME}. Installed files under /opt/ringmabell were left intact."
