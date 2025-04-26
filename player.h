#ifndef PLAYER_H
#define PLAYER_H

/*
  player.h
  --------
  Header file for individual player processes.
  Contains declarations and data structures related to a single player's logic.
*/

#ifdef __cplusplus
extern "C" {
#endif

// Setup signal handlers for GET_READY, START_PULLING, REPORT_ENERGY, FALL
void handleGetReady(int signum);
void handleStartPulling(int signum);
void handleReportEnergy(int signum);
void handleFall(int signum);

int main(int argc, char* argv[]);

#ifdef __cplusplus
}
#endif

#endif // PLAYER_H