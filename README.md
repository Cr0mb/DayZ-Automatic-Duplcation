# DayZ Lag Duplication Glitch Documentation


## Overview
The glitch exploits a synchronization vulnerability in the DayZ server session management. By blocking network traffic during the character logout sequence, the client forces the server into an inconsistent state where it fails to despawn the original character, effectively leaving a "clone" in the world while allowing the player to rejoin.

---

## How the Glitch Works

The duplication glitch takes advantage of desynchronization between your game client and the server.

### 1. Network Desynchronization (Lagswitching)
By enabling a firewall rule that blocks the game's network traffic, you create a state where your client is "lagging." During this period, the server stops receiving updates about your actions but doesn't immediately kick you.

### 2. State Inconsistency (The Loop)
While lagging, the utility repeatedly starts and cancels the logout timer.
- **Client-Side:** Your game thinks it is trying to leave and then staying.
- **Server-Side:** Because of the lag, the server receives a backlog of conflicting "Exit" and "Cancel" requests all at once when the lag ends. This "confuses" the server's session management logic.

### 3. Clone Generation (The Final Exit)
On the 4th attempt, the utility clicks "Exit Now" precisely as the connection is restored.
- The server receives the "Exit Now" command while it is still processing the previous cancelled logout attempts.
- Due to this race condition, the server terminates your active session but fails to trigger the despawn routine for your character model.
- Your character remains in the world as a "stale" instance (the clone), while you are free to rejoin as a new session.

### 4. Technical Sequence
The utility automates this desynchronization with millisecond precision:

| Stage | Action | Timing / Logic |
| :--- | :--- | :--- |
| **Desync Loop** | 3x Exit/Cancel | Confuses the server's state machine. |
| **Timed Exit** | 4th Exit Attempt | Initiated at `LagDuration - 700ms`. |
| **Finalization** | "Exit Now" Click | Clicked at `LagDuration - 150ms`. |

---

## Configuration & Tuning

To achieve the highest success rate (standard is ~80%), you must tune the utility parameters based on your network conditions.

### 1. Lag Duration (Tuning for Ping)
The `Lag Duration` is the total time the firewall blocks the game's traffic. This must be long enough for the server to register a "lost connection" but short enough to avoid a complete session timeout.

| Server Ping | Recommended Lag Duration |
| :--- | :--- |
| **Low (<50ms)** | 4,000ms - 5,500ms |
| **Medium (50ms - 150ms)** | 6,000ms (Default) |
| **High (>150ms)** | 7,000ms - 8,500ms |

*   **If you are timed out entirely:** Decrease the duration.
*   **If no "red chains" appear or the server doesn't desync:** Increase the duration.

### 2. Lobby Wait (Rejoin Timing)
The `Lobby Wait` is the time the utility waits in the main menu before clicking "Play" to rejoin the server. This is the most sensitive parameter.

*   **Default:** 9,000ms (9 seconds).
*   **Error: "User with same UID is already in server":** This means you are rejoining too fast. **Increase** the wait time by 500ms increments.
*   **Success Indicator:** When rejoining, you should see a **35-second timer**.
    *   **35s Timer:** High success rate.
    *   **15s Timer:** Likely failed (you rejoined too late); **Decrease** the wait time by 500ms increments.

---

## Step-by-Step Procedure

### 1. Preparation
1.  **Firewall Setup:** Ensure you have an Inbound and Outbound rule named **"Rule"** that blocks `DayZ_x64.exe`.
2.  **Admin Rights:** Run `dub.exe` as Administrator.
3.  **In-Game:** Be logged into the server for at least **30 seconds** before starting. Stand in a safe, hidden location.

### 2. Execution
1.  Press **CapsLock**.
2.  **Hands off:** The utility will automatically:
    - Activate the lagswitch.
    - Perform 3 cycles of starting and cancelling the "Exit to Lobby" timer.
    - Perform a final "Exit Now" click precisely as the lag ends.
3.  The game will return you to the main menu/lobby.

### 3. Rejoining
1.  The utility will wait for the `Lobby Wait` duration and then click **PLAY**.
2.  Check your spawn timer:
    - **35 Seconds:** Perfect. The clone is likely in the world.
    - **15 Seconds:** You may have been too slow or the server synced too quickly.

### 4. Securing the Loot
1.  Locate your clone at the exact spot you logged out.
2.  Kill the clone and loot the items.
3.  **IMPORTANT:** Wait **1 to 2 minutes** after killing the clone before logging out or performing another dupe. This ensures the server fully synchronizes your new inventory and prevents "inventory rollbacks" or character death upon re-login.

---

## Compilation
To compile the project using `g++` (MinGW), run the following command:

```bash
g++ dub.cpp -o dub.exe -lcomctl32 -lole32 -loleaut32 -luuid -lshell32 -ladvapi32 -mwindows
```

## Usage
1. Compile the code to generate `dub.exe`.
2. Ensure you have firewall rules named **"Rule"** (both Inbound and Outbound) created and pointing to your game executable.
3. Run `dub.exe`.
4. Use **CapsLock** to toggle the lag/sequence.
5. Use **End** to exit the application.

## Troubleshooting

*   **Tool Clicks Wrong Spots:** The tool scales coordinates based on your resolution. Ensure you are running the game in **Borderless** or **Windowed** mode at your native desktop resolution for best results.
*   **No Red Chains:** Your firewall rule name must be exactly **"Rule"**.
*   **Kicked to Lobby Instantly:** The `Lag Duration` is too short for the desync loop to complete. Increase it to at least 4000ms.

---
*Disclaimer: This documentation is for educational purposes regarding network synchronization vulnerabilities.*
