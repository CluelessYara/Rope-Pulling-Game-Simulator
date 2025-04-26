#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H



// ============================
// GameState Structure
// Tracks round number, team scores, consecutive wins,
// win threshold, and energy sums for each round.
// ============================
typedef struct {
    int roundNumber;             // Current round number
    int scoreTeam1;              // Total rounds won by Team 1
    int scoreTeam2;              // Total rounds won by Team 2
    int consecutiveWinsTeam1;    // Consecutive rounds won by Team 1
    int consecutiveWinsTeam2;    // Consecutive rounds won by Team 2
    int winThreshold;            // Effort threshold for winning a round
    int maxRounds;               // Maximum number of rounds before game over
    int currentTime;             // Total elapsed time (if needed)
    int sumTeam1;                // Sum of effective energies for Team 1 in the current round
    int sumTeam2;                // Sum of effective energies for Team 2 in the current round
    int ropeOffset;
} GameState;

// ============================
// Function Prototypes for Game Logic
// ============================
void initGameLogic(GameState* state);
void startRound(GameState* state);
void reorderTeams(); 
void collectEnergies(GameState* state, int pipeFDsTeam1[], int pipeFDsTeam2[]);
int checkRoundWinner(GameState* state);
void endRound(GameState* state, int winningTeam);
int isGameOver(GameState* state);


#endif // GAME_LOGIC_H

