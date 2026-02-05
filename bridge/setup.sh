#!/bin/bash
# OpenClaw Cardputer Bridge - Setup Script

set -e

cd "$(dirname "$0")"

echo "🐾 Installing Python dependencies..."
# Use system Python 3.12 with venv
/usr/bin/python3.12 -m venv .venv 2>/dev/null || echo "✓ venv already exists"
.venv/bin/pip install -r requirements.txt

echo "📝 Creating environment file..."
if [ ! -f .env ]; then
    cp .env.example .env
    echo "⚠️  Please edit .env with your configuration:"
    echo "   - OPENCLAW_GATEWAY_URL (usually http://localhost:18789)"
    echo "   - OPENCLAW_API_KEY (generate a secure random key)"
    echo "   - WEBSOCKET_PORT (default 8765)"
else
    echo "✓ .env already exists, skipping..."
fi

echo "🔧 Creating systemd service..."
sudo tee /etc/systemd/system/openclaw-bridge.service > /dev/null <<EOF
[Unit]
Description=OpenClaw Cardputer Bridge
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=$(pwd)
Environment="PATH=/root/.local/bin:/usr/bin"
ExecStart=$(pwd)/.venv/bin/uvicorn main:app --host 0.0.0.0 --port 8765
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

echo "🔄 Reloading systemd..."
sudo systemctl daemon-reload

echo "✅ Setup complete!"
echo ""
echo "Next steps:"
echo "1. Edit .env with your configuration"
echo "2. Start the bridge: sudo systemctl start openclaw-bridge"
echo "3. Enable auto-start: sudo systemctl enable openclaw-bridge"
echo "4. Check status: sudo systemctl status openclaw-bridge"
echo ""
echo "Or run manually for testing: uvicorn main:app --host 0.0.0.0 --port 8765"
