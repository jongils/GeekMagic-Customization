#!/bin/bash
# camera_server/install.sh — Pi 125에서 카메라 서버를 systemd 서비스로 등록
# 실행: bash install.sh

set -e
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$PROJECT_DIR/venv"
SERVICE_FILE="/etc/systemd/system/camera-server.service"
SERVICE_USER="${SUDO_USER:-$(whoami)}"

echo "=== Camera Server 설치 ==="
echo "경로: $PROJECT_DIR"
echo "사용자: $SERVICE_USER"

# fswebcam 설치 확인
if ! command -v fswebcam &>/dev/null; then
    echo "▶ fswebcam 설치 중..."
    sudo apt-get install -y fswebcam
fi

# USB 카메라 장치 확인
echo ""
echo "=== 감지된 비디오 장치 ==="
ls /dev/video* 2>/dev/null || echo "(비디오 장치 없음)"
echo ""

# 기본 카메라 장치 설정
read -rp "USB 카메라 장치 경로 [기본값: /dev/video0]: " CAM_DEV
CAM_DEV="${CAM_DEV:-/dev/video0}"

# API 인증 토큰 설정 (필수)
read -rsp "카메라 API 토큰 입력: " CAMERA_API_TOKEN
echo ""
if [ -z "$CAMERA_API_TOKEN" ]; then
    echo "❌ 카메라 API 토큰은 비워둘 수 없습니다"
    exit 1
fi

# venv 생성
if [ ! -d "$VENV_DIR" ]; then
    echo "▶ 가상환경 생성 중..."
    python3 -m venv "$VENV_DIR"
fi
"$VENV_DIR/bin/pip" install flask --quiet
echo "✅ 패키지 설치 완료"

# systemd 서비스 파일 생성
sudo tee "$SERVICE_FILE" > /dev/null <<EOF
[Unit]
Description=GeekMagic Camera Server
After=network.target

[Service]
Type=simple
User=$SERVICE_USER
WorkingDirectory=$PROJECT_DIR
Environment="CAMERA_DEV=$CAM_DEV"
Environment="PORT=5050"
Environment="CAMERA_API_TOKEN=$CAMERA_API_TOKEN"
ExecStart=$VENV_DIR/bin/python3 $PROJECT_DIR/camera_server.py
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable camera-server
sudo systemctl restart camera-server

echo ""
echo "✅ 설치 완료"
echo "상태 확인: sudo systemctl status camera-server"
echo "서버 주소: http://$(hostname -I | awk '{print $1}'):5050/health"
