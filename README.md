# EV3GPT — Ω Bot v3
*Active Development — Core EV3 firmware complete, Jetson AI pipeline in progress*

Ω Bot is an autonomous robot series built iteratively across three versions. v1 was a basic EV3 tank with a control loop. v2 added Bluetooth and a camera for wireless control. v3 — EV3GPT — offloads all AI and perception to an NVIDIA Jetson Orin Nano, turning the EV3 into an intelligent autonomous platform.

---

## Architecture

The system is split across two devices connected over WiFi UDP:

```
┌─────────────────────────────────┐        9-byte UDP packets          ┌─────────────────────────────────────┐
│           LEGO EV3              │  ──── Sensor Telemetry ────►       │        NVIDIA Jetson Orin Nano      │
│         (ev3dev Linux)          │  ◄─── Motor Commands ────          │                                     │
│                                 │                                    │                                     │
│  • C/C++ bare-metal firmware    │                                    │  • 3-stage CV pipeline              │
│  • sysfs motor/sensor control   │                                    │  • Biometric profile system         │
│  • Custom UDP protocol (CRC8)   │                                    │  • Voice command chains             │
│  • pthread sensor/motor loop    │                                    │  • Gemini LLM agent                 │
└─────────────────────────────────┘                                    └─────────────────────────────────────┘
```

The EV3 handles all time-sensitive hardware control. The Jetson handles all compute-intensive AI tasks. Neither side blocks the other.

---

## Communication Protocol

Both sides speak the same custom 9-byte UDP packet format, validated with CRC8 (CDMA2000 polynomial). Struct sizes are verified at compile time with `static_assert`.

**EV3 → Jetson (Sensor Telemetry)**
```
[0: Header] [1: Ultrasonic] [2: Color] [3-4: Gyro (12.4 fixed-point, big-endian)]
[5: IR Proximity] [6: IR Angle] [7: Timestamp] [8: CRC8]
```

**Jetson → EV3 (Motor Commands)**
```
[0: Header] [1: Motor A] [2: Motor B] [3: Motor C] [4: Motor D]
[5-6: Duration ms (big-endian)] [7: Stop mode] [8: CRC8]
```

The gyro is packed as a 12.4 fixed-point integer (1/16 degree resolution) to fit in 2 bytes without losing precision. The header byte encodes port selection, sync mode, and gyro mode via bitmask.

---

## EV3 Firmware ✅

Written in C/C++ on ev3dev (embedded Linux). No EV3 SDK — motor and sensor control goes directly through the sysfs kernel interface.

- **Motor control** — file descriptors opened to `/sys/class/tacho-motor/*/duty_cycle_sp` and `/command`. Port auto-discovery by reading address files at startup.
- **Sensor reads** — `lseek` back to offset 0 before each read (required for sysfs character devices). Supports color, gyro, ultrasonic, and IR sensors simultaneously.
- **Gyro calibration** — mode-switch reset sequence with settle delays to zero drift before operation.
- **Threading** — two pthreads: one listening for incoming motor packets, one executing motor commands. Main thread handles sensor telemetry loop.
- **Handshake** — 30-second timeout waiting for first valid packet before entering the main loop.

---

## Jetson AI Pipeline 🚧

### Computer Vision (3-Stage)

| Stage | Model | Status |
|---|---|---|
| Object Detection | YOLOv8 | Planned |
| Depth Estimation | Depth Anything V2 (ViT-S) | ✅ Implemented |
| Traversability Segmentation | Custom UNet (distilled SAM) | Planned |

Depth maps run every other frame to reduce compute load. The three outputs are fused into a dynamic traversability map — combining what's there, how far it is, and whether the robot can drive over it. Camera mounting height is used to convert relative depth to real-world distance estimates.

### Biometrics

Face and voice embeddings are generated for registered users and stored in `.safetensors` format. At runtime, incoming audio/video is matched against stored profiles via cosine similarity — no raw data is stored, only embeddings.

- **Face** — InsightFace `tiny_face` model, 512-dim embeddings
- **Voice** — SpeechBrain ECAPA-VOXCELEB, verified against stored speaker embeddings
- **Matching** — cosine similarity with 0.7 threshold, supports face-only, voice-only, or fused scoring

### Voice Command System

Natural language commands are parsed into `CommandChain` structures at runtime — no recompilation needed to change robot behavior.

- Commands support sequential (`then`) and parallel (`and`) execution
- Chains have pause/resume via `asyncio.Event`
- Index-based flow control with jump exhaustion limits enables basic loops and branching
- VAD (Silero) gates the pipeline so Whisper only runs when speech is detected

### LLM Agent

Gemini handles open-ended conversation and function dispatch. Speaker identity is passed with each message so the agent maintains per-user context. Responses that trigger function calls route to the command system rather than TTS.

---

## Tech Stack

**EV3:** C · C++ · ev3dev · POSIX sockets · pthreads · sysfs

**Jetson:** Python · PyTorch · Depth Anything V2 · YOLOv8 · InsightFace · SpeechBrain · Faster Whisper · Silero VAD · Gemini API · asyncio
