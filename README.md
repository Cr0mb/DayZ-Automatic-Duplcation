# DayZ Lag Duplication Glitch Documentation

This document explains the mechanics and execution of the "Lag Duplication Glitch" as implemented in `dub.cpp` and described in `lag.txt`.

## Overview
The glitch exploits desynchronization between the DayZ client and server. By blocking network traffic during the character logout sequence, the client can manipulate the server into keeping a "stale" version of the character (the clone) in the world while allowing the player to rejoin as a new session.

## Core Mechanics

### 1. Network Desynchronization (Lagswitching)
The process begins by enabling a Windows Firewall rule (named "Rule" in the C++ tool or "Lagger" in the AHK script) that blocks the game's executable (`DayZ_x64.exe`). This prevents the client from sending updates to the server.

### 2. State Inconsistency
While lagging, the client performs a series of "Exit" and "Cancel" actions.
- **The Loop:** Starting and cancelling the exit timer 3 times "confuses" the server's state machine for that session.
- **The Final Exit:** On the 4th attempt, the client sends an "Exit Now" command just as the connection is restored.

### 3. Clone Generation
Because the server receives the "Exit Now" command while its internal state for the player is inconsistent (due to the previous lag and cancelled exits), it fails to properly despawn the character.
- The server retains the character's last known state in the world.
- The player's session is terminated, allowing them to rejoin the lobby.

## Technical Implementation (`dub.cpp`)

The `dub.cpp` utility automates the sequence with precise timing:

| Stage | Action | Timing / Logic |
| :--- | :--- | :--- |
| **Activation** | Enable Firewall Rule | Blocks traffic; triggers "LAGGING" state. |
| **Desync Loop** | 3x Exit/Cancel | Opens menu (ESC), clicks "Exit", then "Cancel". |
| **Timed Exit** | 4th Exit Attempt | Initiated at `LagDuration - 700ms`. |
| **Completion** | "Exit Now" Click | Clicked at `LagDuration - 150ms`. |
| **Restoration** | Disable Firewall Rule | Restores traffic; player is kicked to lobby. |
| **Rejoin** | Wait & Click Play | Waits `g_LobbyWait` (~9s) then clicks "Play". |

### Coordinate Scaling
The tool uses resolution-independent clicking by scaling base 1600x900 coordinates to the user's current screen resolution:
```cpp
float sx = (float)w / 1600;
float sy = (float)h / 900;
g_ExitX = (int)(BaseExitX * sx);
// ... etc
```

## Step-by-Step Procedure

1.  **Preparation:**
    - Ensure the firewall rule for `DayZ_x64.exe` is created but disabled.
    - Run the utility (`dub.exe`) as Administrator.
2.  **Execution:**
    - Press **CapsLock** to start the sequence while in-game.
    - The tool will automatically perform the lag, exit loops, and final "Exit Now" click.
3.  **Lobby Wait:**
    - Wait in the lobby for approximately 9-10 seconds. Rejoining too quickly results in a "UID already in use" error.
4.  **Verification:**
    - Upon rejoining, a **35-second timer** usually indicates success.
    - A 15-second timer indicates a higher chance of failure.
5.  **Securing the Loot:**
    - Locate your clone (it should be standing where you "logged out").
    - Kill the clone and loot the duplicated items.
    - **Crucial:** Wait 1–2 minutes before logging out again to allow the server to fully sync the new inventory and prevent rollbacks.

## Troubleshooting

- **No Clone:** Rejoin speed might be too slow, or the `LagDuration` (default 6000ms) needs adjustment based on server latency.
- **Red Chains:** Ensure the firewall rule is actually blocking traffic. If "red chains" don't appear in-game during the lag, the rule is misconfigured.
- **Kicked Immediately:** Ensure you were logged into the server for at least 30 seconds before attempting the dupe.

---
*Disclaimer: This documentation is for educational purposes regarding game engine vulnerabilities and network synchronization.*
