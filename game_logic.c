/*
============================
      game_logic.c
  Implements referee (parent) round logic:
  - Initializes GameState
  - Starts rounds (including reordering by energy -> factor)
  - Collects energies from players
  - Checks for a round winner
  - Ends the round
  - Determines if the game is over
============================
*/

#include "game_logic.h"
#include "parent.h"   // So we know about Player, gPlayers, NUM_PLAYERS
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

// We assume 8 players (4 per team)
#define NUM_PLAYERS 8

// If gPlayers is declared in parent.c, we can reference it here:
extern Player gPlayers[NUM_PLAYERS];
// Declare the external statusMessage variable

// ----------------------------
// initGameLogic
// ----------------------------
void initGameLogic(GameState* state) {
    state->roundNumber          = 0;
    state->scoreTeam1           = 0;
    state->scoreTeam2           = 0;
    state->consecutiveWinsTeam1 = 0;
    state->consecutiveWinsTeam2 = 0;

    // Example defaults
    state->winThreshold = 500;
    state->maxRounds    = 5;

    state->currentTime  = 0;
    state->sumTeam1     = 0;
    state->sumTeam2     = 0;

    printf("[Referee] Game logic initialized.\n");
}

// ----------------------------
// gatherTeam
// Collect the 4 players from a given team into a temporary array.
// Returns how many found (should be 4).
// ----------------------------
static int gatherTeam(Player dest[4], int team) {
    int count = 0;
    for (int i = 0; i < NUM_PLAYERS; i++) {
        if (gPlayers[i].team == team) {
            dest[count++] = gPlayers[i];
            if (count == 4) break;
        }
    }
    return count; // hopefully 4
}

// ----------------------------
// sortByEnergyAsc
// Sort an array of 4 players by ascending energy
// ----------------------------
static void sortByEnergyAsc(Player arr[4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = i+1; j < 4; j++) {
            if (arr[j].energy < arr[i].energy) {
                Player tmp = arr[i];
                arr[i] = arr[j];
                arr[j] = tmp;
            }
        }
    }
}

// ----------------------------
// reorderTeams
// - gather 4 players for Team1 => sort => assign factor=1..4
// - gather 4 players for Team2 => sort => assign factor=1..4
// - write them back to gPlayers
// ----------------------------
void reorderTeams() {
    Player t1[4], t2[4];

    // Gather players for Team 1 and Team 2
    gatherTeam(t1, 1);
    gatherTeam(t2, 2);

    // Sort each team's players by ascending energy
    sortByEnergyAsc(t1);
    sortByEnergyAsc(t2);

    // Assign factors 1..4 (lowest energy gets factor 1, highest gets factor 4)
    for (int i = 0; i < 4; i++) {
        t1[i].positionFactor = i + 1; 
        t2[i].positionFactor = i + 1;
    }

    // Debug prints to verify reordering and factor assignment
    for (int i = 0; i < 4; i++) {
        printf("[Referee] Team1 - Player %d: energy=%.2f, assigned factor=%d\n",
               t1[i].id, t1[i].energy, t1[i].positionFactor);
    }
    for (int i = 0; i < 4; i++) {
        printf("[Referee] Team2 - Player %d: energy=%.2f, assigned factor=%d\n",
               t2[i].id, t2[i].energy, t2[i].positionFactor);
    }

    // Map the new factors back to the global gPlayers array by matching IDs.
    for (int i = 0; i < 4; i++) {
        int id = t1[i].id;
        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (gPlayers[j].id == id) {
                gPlayers[j].positionFactor = t1[i].positionFactor;
                break;
            }
        }
    }
    for (int i = 0; i < 4; i++) {
        int id = t2[i].id;
        for (int j = 0; j < NUM_PLAYERS; j++) {
            if (gPlayers[j].id == id) {
                gPlayers[j].positionFactor = t2[i].positionFactor;
                break;
            }
        }
    }
}
        
// ----------------------------
// startRound
// Called by parent at the beginning of each round
// -> increments roundNumber
// -> reorder by energy => factor=1..4
// ----------------------------
void startRound(GameState* state) {
    state->roundNumber++;
    printf("\n[Referee] --- Starting Round %d ---\n", state->roundNumber);

    // Re-align players so lowest -> factor=1, highest -> factor=4
    reorderTeams();
}

// ----------------------------
// collectEnergies
// We read raw values from each pipe, multiply by the player's factor
// in the PARENT side, to truly reflect factor=1..4 advantage
// ----------------------------
 // Collect energies from the child processes
 void collectEnergies(GameState* state, int pipeFDsTeam1[], int pipeFDsTeam2[]) {
    int totalTeam1 = 0;
    int totalTeam2 = 0;

    for (int i = 0; i < 4; i++) {
        int rawVal = 0;
        if (read(pipeFDsTeam1[i], &rawVal, sizeof(rawVal)) > 0) {
            gPlayers[i].energy = (double) rawVal;  // already multiplied in player.c
            totalTeam1 += rawVal;
        }
    }

    for (int i = 0; i < 4; i++) {
        int rawVal = 0;
        if (read(pipeFDsTeam2[i], &rawVal, sizeof(rawVal)) > 0) {
            int idx = i + 4;
            gPlayers[idx].energy = (double) rawVal;
            totalTeam2 += rawVal;
        }
    }

    state->sumTeam1 = totalTeam1;
    state->sumTeam2 = totalTeam2;
    printf("[Referee] Collected energies => T1=%d, T2=%d\n", totalTeam1, totalTeam2);
}
// ----------------------------
// checkRoundWinner
// if sumTeam >= winThreshold => that team wins
// ----------------------------
int checkRoundWinner(GameState* state) {
    if (state->sumTeam1 >= state->winThreshold) return 1;
    if (state->sumTeam2 >= state->winThreshold) return 2;
    return 0;
}

// ----------------------------
// endRound
// update scores based on winner, reset sums
// ----------------------------
void endRound(GameState* state, int winningTeam) {
    if (winningTeam == 1) {
        state->scoreTeam1++;
        state->consecutiveWinsTeam1++;
        state->consecutiveWinsTeam2 = 0;
        state->ropeOffset -= 1;  // Move rope toward Team 1
        printf("[Referee] Team 1 wins Round %d!\n", state->roundNumber);

    } else if (winningTeam == 2) {
        state->scoreTeam2++;
        state->consecutiveWinsTeam2++;
        state->consecutiveWinsTeam1 = 0;
        state->ropeOffset += 1;  // Move rope toward Team 1
        printf("[Referee] Team 2 wins Round %d!\n", state->roundNumber);
    } else {
        printf("[Referee] Round %d ended with no winner.\n", state->roundNumber);
    }
    printf("[Referee] Current Score => Team1: %d, Team2: %d\n",
           state->scoreTeam1, state->scoreTeam2);

    // reset round sums
    state->sumTeam1 = 0;
    state->sumTeam2 = 0;
}

// ----------------------------
// isGameOver
// stop if maxRounds reached or consecutive wins =2
// ----------------------------
int isGameOver(GameState* state) {
    if (state->roundNumber >= state->maxRounds) {
        printf("[Referee] Maximum rounds reached.\n");
        return 1;
    }
    if (state->consecutiveWinsTeam1 >= 2 || state->consecutiveWinsTeam2 >= 2) {
        printf("[Referee] A team has won 2 consecutive rounds.\n");
        return 1;
    }
    return 0;
}

