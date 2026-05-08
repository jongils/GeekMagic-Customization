#!/bin/bash
# install.sh — Weather Clock 설치 및 서비스 등록

GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

# 스크립트가 있는 디렉토리를 프로젝트 루트로 자동 감지
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_PYTHON="$PROJECT_DIR/venv/bin/python3"
SERVICE_NAME="weather-clock"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"
CURRENT_USER=$(whoami)

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}  Weather Clock — 서비스 설치${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# ── venv 확인 ───────────────────────────────────────────────
if [ ! -f "$VENV_PYTHON" ]; then
  echo -e "${RED}❌ venv 없음 — setup_pi.sh 먼저 실행하세요${NC}"
  exit 1
fi
echo -e "${GREEN}✅ venv 확인: $VENV_PYTHON${NC}"
echo ""

# ── config.json에 API Key 입력 ──────────────────────────────
CONFIG_FILE="$PROJECT_DIR/config.json"
echo -e "${BLUE}OpenWeatherMap API Key 설정${NC}"
if [ -f "$CONFIG_FILE" ]; then
  CURRENT_KEY=$(python3 -c "import json; d=json.load(open('$CONFIG_FILE')); print(d.get('api_key',''))" 2>/dev/null)
  if [ -n "$CURRENT_KEY" ]; then
    echo -e "  현재 API Key: ${YELLOW}${CURRENT_KEY:0:8}...${NC}"
    read -p "  새로운 API Key 입력 (엔터 = 기존 유지): " NEW_KEY
    if [ -n "$NEW_KEY" ]; then
      python3 -c "
import json
with open('$CONFIG_FILE') as f: d = json.load(f)
d['api_key'] = '$NEW_KEY'
with open('$CONFIG_FILE','w') as f: json.dump(d, f, indent=2)
print('  API Key 저장 완료')
"
    fi
  else
    read -p "  OpenWeatherMap API Key 입력: " NEW_KEY
    if [ -n "$NEW_KEY" ]; then
      python3 -c "
import json
with open('$CONFIG_FILE') as f: d = json.load(f)
d['api_key'] = '$NEW_KEY'
with open('$CONFIG_FILE','w') as f: json.dump(d, f, indent=2)
print('  API Key 저장 완료')
"
    fi
  fi
fi
echo ""

# ── systemd 서비스 파일 생성 ─────────────────────────────────
echo -e "${BLUE}systemd 서비스 파일 생성${NC}"
sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=Weather Clock — GeekMagic Image Push Service
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${CURRENT_USER}
WorkingDirectory=${PROJECT_DIR}
ExecStart=${VENV_PYTHON} ${PROJECT_DIR}/main.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
Environment=PYTHONUNBUFFERED=1

[Install]
WantedBy=multi-user.target
EOF

echo -e "  ${GREEN}✅ 서비스 파일 생성: $SERVICE_FILE${NC}"
echo ""

# ── 서비스 등록 및 시작 ──────────────────────────────────────
echo -e "${BLUE}서비스 등록 및 시작${NC}"
sudo systemctl daemon-reload
sudo systemctl enable "$SERVICE_NAME"
sudo systemctl restart "$SERVICE_NAME"
sleep 2

STATUS=$(sudo systemctl is-active "$SERVICE_NAME")
if [ "$STATUS" = "active" ]; then
  echo -e "  ${GREEN}✅ 서비스 실행 중 (active)${NC}"
else
  echo -e "  ${RED}❌ 서비스 상태: $STATUS${NC}"
  echo -e "  로그 확인: ${YELLOW}journalctl -u $SERVICE_NAME -n 30${NC}"
fi
echo ""

# ── 완료 ──────────────────────────────────────────────────────
PI_IP=$(hostname -I | awk '{print $1}')
echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}  ✅ 설치 완료!${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""
echo -e "  웹 설정 UI : ${YELLOW}http://${PI_IP}:8080${NC}"
echo -e "  로그 확인  : ${YELLOW}journalctl -u $SERVICE_NAME -f${NC}"
echo -e "  서비스 중지: ${YELLOW}sudo systemctl stop $SERVICE_NAME${NC}"
echo -e "  서비스 재시작: ${YELLOW}sudo systemctl restart $SERVICE_NAME${NC}"
echo ""