# Rope Pulling Game - Multi-Process Simulation

## Overview

This project is a **multi-process rope-pulling game** where **eight players** (child processes) simulate a tug-of-war competition.  
Each player consumes energy over time and reports its status to a central **referee** (the main parent process), who uses the reported energies to **update the rope position** in real time using **OpenGL visualization**.

The game demonstrates concepts such as:

- **Interprocess Communication (IPC)** using pipes
- **Process Synchronization** using signals
- **Simple physics animation** using OpenGL
- **Game State Management** across multiple rounds

## How It Works

### 1. Initialization
- The **parent process** (`parent.c`) reads a configuration file (default: `PlayersConfiguration.txt`) to initialize the players:
  - Each player has an ID, a team number (1 or 2), and an initial energy value.
- It forks **eight child processes** — each one representing a player.

### 2. Communication Setup
- Two **pipes** are created for each player:
  - One for **sending energy values** from player to parent.
  - One for **sending position factors** from parent to player.
- Signals (`SIGUSR1`, `SIGUSR2`, `SIGALRM`) are used to control player behavior during each game round.

### 3. Game Loop
- The game proceeds through multiple rounds managed by a timer.
- **Each round:**
  - The referee signals all players to **start depleting energy**.
  - Players reorder based on their remaining energy.
  - New **position factors** are sent to players.
  - The referee collects updated energy values from players once per second.
  - The **rope** shifts towards the stronger team based on the energy difference.
  - A team wins the round if the rope is pulled far enough.
  - After a team wins a set number of rounds, the game ends.

### 4. OpenGL Visualization
- The rope and players are drawn on a simple OpenGL 2D canvas.
- The rope **smoothly animates** towards the side with greater cumulative energy.
- The animation runs independently of the round logic for a smoother display.

## Project Structure

| File | Description |
|:-----|:------------|
| `parent.c` | Main referee process: game logic, OpenGL setup, player management |
| `player.c` | Player process: receives factors, depletes energy, reports to parent |
| `game_logic.c/.h` | Manages round logic, reordering, checking winners |
| `PlayersConfiguration.txt` | Example configuration file for player setup |
| `Makefile` | (optional) Compile both parent and player executables easily |

## Key Technologies

- **C Programming** (Processes, Pipes, Signals)
- **OpenGL (GLUT)** for simple 2D animation
- **UNIX System Calls**: `fork()`, `pipe()`, `execl()`, `kill()`, `usleep()`

## How to Build and Run

1. **Install OpenGL and GLUT libraries**  
   (on Ubuntu/Debian):
   ```bash
   sudo apt-get install freeglut3-dev
   ```

2. **Compile**
   ```bash
   gcc parent.c game_logic.c -o parent -lGL -lGLU -lglut
   gcc player.c -o player
   ```

3. **Run the Parent Process**
   ```bash
   ./parent
   ```

   *(The parent will automatically spawn 8 child players.)*

4. **(Optional) Edit the Player Configuration**  
   Update `PlayersConfiguration.txt` to customize player stats.

## Notes

- If any player process crashes or a pipe breaks, the parent will output an error.
- The rope movement is **animated smoothly** toward the new target every frame for a natural effect.
- Signals and pipes work together: **signals tell players when to act**, **pipes carry the data**.


## Future Improvements
- Add **fancier graphics** (rope tension, animated pulling).
- Add a **winning animation** or **round summary screen**.
- Expand the configuration file to support **different strategies** or **AI-controlled players**.

