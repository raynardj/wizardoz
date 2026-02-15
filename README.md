# ESP32-S3 based projects
> Voice AI agent driven device control.

Experimental ESP32-S3 based projects.

## 🚀 Small Projects
* ✨ [Simplest ESP32-S3 project](./src/main.cpp)
* 🎮 [8-Bit Music Play with PCM5102A DAC](./templates/8bit-music)

## ⛳️🏰 Target
To intergrate with voice AI agent, see [this reference repo](https://github.com/livekit/client-sdk-esp32)

## 📋 Template Management

Use `manage_template.py` to backup and restore project templates:

```bash
# Backup current src/ as a template
python manage_template.py backup my-project-name

# Backup with description
python manage_template.py backup my-game -d "Platformer with jump mechanics"

# List available templates
python manage_template.py list

# Restore template to src/
python manage_template.py select voice-jump-game

# Force restore (no prompts)
python manage_template.py select voice-jump-game -f

# Delete a template
python manage_template.py delete old-template

# Show current status
python manage_template.py status
```

### Available Templates

| Template | Description |
|----------|-------------|
| [voice-jump-game](./templates/voice-jump-game/) | Voice-controlled princess jumping game |
| [voice-capture](./templates/voice-capture/) | Audio recorder with waveform display |
| [8bit-music](./templates/8bit-music/) | 8-bit music player with PCM5102A DAC |
| [visualizer](./templates/visualizer/) | Audio visualizer |