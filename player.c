/*
============================
         player.c
   Code for individual player processes.
   - Sets up signal handlers.
   - Responds to parent's signals:
       SIGUSR1: GET_READY  (prepare/re-align; read updated factor)
       SIGUSR2: START_PULLING (begin or resume pulling, deplete energy)
       SIGALRM: REPORT_ENERGY (report effective energy via pipe)
       SIGBUS:  FALL         (simulate falling: energy becomes 0)
   - Updates global gEnergy and writes reported energy (if pipe is set).
============================
*/

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

// Global variables for the player's state
static int    gPlayerID       = 0;
static int    gTeamID         = 0;
static double gEnergy         = 100.0;  // Initial energy; can be overridden by config
static int    gPositionFactor = 1;      // Factor: 1, 2, 3, or 4 (based on alignment)
static int    gFallen         = 0;      // 0: active, 1: fallen

// File descriptor for writing effective energy to parent (child -> parent)
static int gWriteFD = -1;
// File descriptor for reading updated factor from parent (parent -> child)
static int gFactorReadFD = -1;

// ----------------------------
// Signal Handler: GET_READY (SIGUSR1)
// Child reads new factor from the factor pipe and updates gPositionFactor.
// ----------------------------
static void handleGetReady(int signum) {
    printf("[Player %d] Received GET_READY signal.\n", gPlayerID);
    int newFactor = 0;
    int bytesRead = read(gFactorReadFD, &newFactor, sizeof(newFactor));
    printf("[Player %d] read %d bytes from factor pipe.\n", gPlayerID, bytesRead);
    if (bytesRead > 0) {
        gPositionFactor = newFactor;
        fprintf(stderr, "[Player %d] Updated factor => %d\n", gPlayerID, gPositionFactor);
    } else {
        fprintf(stderr, "[Player %d] Failed to update factor (bytesRead=%d).\n", gPlayerID, bytesRead);
    }
}


// ----------------------------
// Signal Handler: START_PULLING (SIGUSR2)
// Child simulates pulling by depleting energy.
// ----------------------------
static void handleStartPulling(int signum) {
    printf("[Player %d] Received START_PULLING signal. Beginning to pull...\n", gPlayerID);
    if (!gFallen) {
        int decrease = rand() % 10 + 5;  // Decrease energy by 5 to 14 units.
        gEnergy -= decrease;
        if (gEnergy < 0)
            gEnergy = 0;
        printf("[Player %d] gEnergy now: %.2f\n", gPlayerID, gEnergy);
    }
}

// ----------------------------
// Signal Handler: REPORT_ENERGY (SIGALRM)
// Child multiplies its energy by gPositionFactor and writes the result to the parent.
// ----------------------------
static void handleReportEnergy(int signum) {
    double effective = gFallen ? 0 : (gEnergy * gPositionFactor);
    int reportValue = (int) effective;
    printf("[Player %d] Reporting effective energy: %d (gEnergy: %.2f, Factor: %d)\n",
           gPlayerID, reportValue, gEnergy, gPositionFactor);
    if (gWriteFD != -1) {
        if (write(gWriteFD, &reportValue, sizeof(reportValue)) == -1) {
            perror("[Player] write error");
        }
    }
}

// ----------------------------
// Signal Handler: FALL (SIGBUS)
// Simulate a fall by setting energy to 0.
// ----------------------------
static void handleFall(int signum) {
    printf("[Player %d] Fell! gEnergy set to 0.\n", gPlayerID);
    gFallen = 1;
    gEnergy = 0;
}

// ----------------------------
// Main Function
// Expected arguments: <playerID> <teamID> <initialEnergy> <writeFD> <factorReadFD>
// ----------------------------
int main(int argc, char* argv[]) {
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <playerID> <teamID> <initialEnergy> <writeFD> <factorReadFD>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    gPlayerID     = atoi(argv[1]);
    gTeamID       = atoi(argv[2]);
    gEnergy       = atof(argv[3]);
    gWriteFD      = atoi(argv[4]);
    gFactorReadFD = atoi(argv[5]);

    gPositionFactor = 1;  // Default factor (will be updated via factor pipe)
    gFallen         = 0;

    srand(time(NULL) + gPlayerID);

    printf("[Player %d] Starting. Team=%d, gEnergy=%.2f, gWriteFD=%d, gFactorReadFD=%d\n",
           gPlayerID, gTeamID, gEnergy, gWriteFD, gFactorReadFD);

    // Set up signal handlers.
    struct sigaction sa;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    // GET_READY: SIGUSR1
    sa.sa_handler = handleGetReady;
    sigaction(SIGUSR1, &sa, NULL);

    // START_PULLING: SIGUSR2
    sa.sa_handler = handleStartPulling;
    sigaction(SIGUSR2, &sa, NULL);

    // REPORT_ENERGY: SIGALRM
    sa.sa_handler = handleReportEnergy;
    sigaction(SIGALRM, &sa, NULL);

    // FALL: SIGBUS
    sa.sa_handler = handleFall;
    sigaction(SIGBUS, &sa, NULL);

    // Main loop: wait indefinitely for signals.
    while (1) {
        pause();
    }

    return 0;
}