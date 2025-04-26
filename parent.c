
/*
============================
         parent.c
   Main (referee) process
   - Reads configuration file for players
   - Forks 8 child processes (players) & sets up pipes for raw energy reporting
   - Uses game_logic to do startRound, collectEnergies, checkRoundWinner, etc.
   - Runs OpenGL to visualize the rope & players
============================
*/

#include <GL/glut.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>     // fork, pipe
#include <sys/types.h>
#include <signal.h>

#include "parent.h"
#include "game_logic.h"
// #include "config.h" // only if you want advanced config logic

#define NUM_PLAYERS 8 // 4 for Team1, 4 for Team2

// Global arrays for players & rope
Player    gPlayers[NUM_PLAYERS];
Rope      gRope;
GameState gState;   // Tracks round #, scores, sums, etc.

// We'll store child PIDs and the read ends of the pipes.
static int pipeEnergy[NUM_PLAYERS][2];  // child -> parent
static int pipeFactor[NUM_PLAYERS][2];  // parent -> child
static pid_t childPIDs[NUM_PLAYERS];
static int   pipeReadFDs[NUM_PLAYERS]; // parent reads from these

// For rope shift in updateScene()
float ropeShift = 0.0f;
float ropeCenterOffset = 0.0f;  // Horizontal offset of the rope center from the middle
float energyDiff; // Global declaration


// forward declarations
static void spawnPlayers();
static void timerRoundLogic(int val);
void idle() {
    static int lastTime = 0;
    int currentTime = glutGet(GLUT_ELAPSED_TIME);
    
    // Only update if enough time has passed (for smooth animation)
    if (currentTime - lastTime > 16) { // ~60fps
        updateScene();
        lastTime = currentTime;
    }
    glutPostRedisplay();
}
float ropeTargetShift = 0.0f;
float ropeVelocity = 0.0f;


// main ---------------------------------------------------------
int main(int argc, char** argv)
{
    // (1) Read configuration for players (IDs, teams, initial energies)
    const char* configFile = (argc > 1) ? argv[1] : "PlayersConfiguration.txt";
    readConfigFile(configFile, gPlayers, NUM_PLAYERS);

    // (2) Initialize game logic (roundNumber=0, threshold=500, etc.)
    initGameLogic(&gState);

    // (3) Fork child processes & create pipes
    spawnPlayers();

    // (4) Initialize GLUT / OpenGL
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Rope Pulling Game - Multi-Process");
    initOpenGL();
    glutIdleFunc(idle);


    // (5) Initialize player positions & rope
    initPlayers(gPlayers, NUM_PLAYERS);
    initRope(&gRope, 10, 350.0, 220.0, 300.0);
    glutTimerFunc(1500, timerRoundLogic, 0);

    // (6) Set up GLUT callbacks
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    // We'll do round logic once per second

    // (7) Enter main loop
    glutMainLoop();

    freeRope(&gRope);
    return 0;
}

// spawnPlayers in parent.c  --------------------------------------------------
static void spawnPlayers()
{
    for (int i=0; i<NUM_PLAYERS; i++) {
        // pipe for energy (child -> parent)
        int fdsEnergy[2];
        if (pipe(fdsEnergy)==-1) {
            perror("pipe energy");
            exit(1);
        }

        // pipe for factor (parent -> child)
        int fdsFactor[2];
        if (pipe(fdsFactor)==-1) {
            perror("pipe factor");
            exit(1);
        }

        pid_t pid = fork();
        if (pid<0) {
            perror("fork");
            exit(1);
        }
        if (pid==0) {
            // CHILD
            // close parent's ends
            close(fdsEnergy[0]);  // parent reads from here, child writes only
            close(fdsFactor[1]);  // parent writes to here, child reads only

            // Build arguments
            char argID[10], argTeam[10], argEnergy[20];
            char argWriteFD[10], argFactorReadFD[10];

            sprintf(argID, "%d", gPlayers[i].id);
            sprintf(argTeam, "%d", gPlayers[i].team);
            sprintf(argEnergy, "%.1f", gPlayers[i].energy);

            sprintf(argWriteFD, "%d", fdsEnergy[1]);
            sprintf(argFactorReadFD, "%d", fdsFactor[0]);

            // execl => 5 arguments in child: <playerID> <teamID> <initEnergy> <writeFD> <factorReadFD>
            execl("./player", "player",
                  argID, argTeam, argEnergy,
                  argWriteFD, argFactorReadFD,
                  (char*)NULL);

            perror("execl failed");
            exit(1);
        }
        else {
            // PARENT
            childPIDs[i] = pid;
            
            // keep track of these in global arrays if you want
            pipeEnergy[i][0] = fdsEnergy[0]; // parent read end
            pipeEnergy[i][1] = fdsEnergy[1]; // child write end
            pipeFactor[i][0] = fdsFactor[0]; // child read end
            pipeFactor[i][1] = fdsFactor[1]; // parent write end

            // close the ends the parent doesn't use
            close(fdsEnergy[1]);  // child writes
            close(fdsFactor[0]);  // child reads
        }
    }
}


// Round logic ---------------------------------------------------
static int roundInProgress = 0;
static int secondCount     = 0;

static void timerRoundLogic(int val)
{
    
    
    if (isGameOver(&gState)) {
        printf("[Referee] Game Over => Final Score: Team1=%d, Team2=%d\n",
               gState.scoreTeam1, gState.scoreTeam2);
        return;
    }

    // Start a new round if none in progress
    if (!roundInProgress) {
        gState.roundNumber++;
        printf("\n[Referee] --- Starting Round %d ---\n", gState.roundNumber);
        secondCount = 0;
        roundInProgress = 1;

        // Signal START_PULLING to all players to deplete energy
        for (int i = 0; i < NUM_PLAYERS; i++) {
            kill(childPIDs[i], SIGUSR2);
        }

        // Delay a little to let energy decrease (optional: add usleep)
        usleep(10000); // 10 ms pause

        // * Reorder players based on depleted energy *
        reorderTeams();

        // Send updated position factors to each child
        for (int i = 0; i < NUM_PLAYERS; i++) {
            int factor = gPlayers[i].positionFactor;
            if (write(pipeFactor[i][1], &factor, sizeof(factor)) == -1) {
                perror("write factor pipe");
            } else {
                printf("[Referee] Wrote factor %d to child %d\n", factor, gPlayers[i].id);
            }
        }

        // Signal GET_READY so each player reads its factor
        for (int i = 0; i < NUM_PLAYERS; i++) {
            kill(childPIDs[i], SIGUSR1);
        }
    }

    // Each second, ask players to report energy
    for (int i = 0; i < NUM_PLAYERS; i++) {
        kill(childPIDs[i], SIGALRM);
    }

    // Collect reported energies
    int pipesTeam1[4], pipesTeam2[4];
    for (int i = 0; i < 4; i++) {
        pipesTeam1[i] = pipeEnergy[i][0];
        pipesTeam2[i] = pipeEnergy[i + 4][0];
    }
    collectEnergies(&gState, pipesTeam1, pipesTeam2);
    //shifts rope towards the winning team
    double diff = (double)gState.sumTeam2 - (double)gState.sumTeam1;
    ropeShift = diff * 0.08;
    printf("sum1: %d, sum2: %d, ropeShift: %f\n", gState.sumTeam1, gState.sumTeam2, ropeShift);

    // Check for round winner
    int winner = checkRoundWinner(&gState);
    if (winner) {
        endRound(&gState, winner);
        roundInProgress = 0;
        if (isGameOver(&gState)) {
            printf("[Referee] Game Over => Final Score: Team1=%d, Team2=%d\n",
                   gState.scoreTeam1, gState.scoreTeam2);
            return;
        }
    } else {
        secondCount++;
        if (secondCount >= 10) {
            endRound(&gState, 0); // No winner
            roundInProgress = 0;
        }
    }
    ropeTargetShift = diff * 0.08;  // Target offset from center

    glutTimerFunc(1000, timerRoundLogic, 0);
    
}


// ============================
// OpenGL / GLUT Callback Functions
// ============================
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawScene();
    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
}

void initOpenGL() {
    glClearColor(1, 1, 1, 1);
    glMatrixMode(GL_PROJECTION);
    gluOrtho2D(0, 800, 0, 600);
}

// updateScene: Computes ropeShift from energy sums and updates the rope.
void updateScene() {
    // Smoothly animate towards target
    float animationSpeed = 0.1f;
    ropeShift += (ropeTargetShift - ropeShift) * animationSpeed;
    
    // Update rope physics
    updateRope(&gRope);
}



void drawScene() {
    drawPlayers(gPlayers, NUM_PLAYERS);
    drawRope(&gRope);

}

// ============================
// Player and Rope Functions (as previously defined)
// ============================

void initPlayers(Player players[], int count) {
    double centerY = 300.0;
    double spacing = 50.0;
    int countTeam1 = 0, countTeam2 = 0;
    for (int i = 0; i < count; i++) {
        if (players[i].team == 1) {
            players[i].position.x = 50.0 + (countTeam1 * spacing);
            players[i].position.y = centerY;
            countTeam1++;
        } else {
            players[i].position.x = 750.0 - (countTeam2 * spacing);
            players[i].position.y = centerY;
            countTeam2++;
        }
        players[i].positionFactor = 1; // Default factor; could be updated by reordering.
        players[i].fallen = 0;
    }
}

static void setColorForEnergy(double energy) {
    if (energy >= 250) {
        glColor3f(0.0f, 1.0f, 0.0f);  // green
    } else if (energy >= 200) {
        glColor3f(1.0f, 0.65f, 0.0f); // orange
    } else if (energy >= 150) {
        glColor3f(1.0f, 1.0f, 0.0f);  // yellow
    } else {
        glColor3f(0.0f, 0.0f, 0.0f);  // black
    }
}

void drawPlayers(const Player players[], int count) {
    float playerOffsetX = ropeShift * 0.01f;

    for (int i = 0; i < count; i++) {
        glPushMatrix();
        glTranslatef(players[i].position.x + ropeShift * 0.05f, players[i].position.y, 0.0f);


        if (players[i].fallen) {
            // Draw fallen players smaller and grayed out
            glColor3f(0.5f, 0.5f, 0.5f); // gray
            glScalef(0.7f, 0.7f, 1.0f);  // smaller size
            glRotatef(30.0f, 0.0f, 0.0f, 1.0f); // tilted
        } else {
            setColorForEnergy(players[i].energy);
        }

        if (players[i].team == 1) { // triangle
            glBegin(GL_TRIANGLES);
                glVertex2f(-10.0f, -10.0f);
                glVertex2f( 10.0f, -10.0f);
                glVertex2f(  0.0f,  10.0f);
            glEnd();
        } else { // circle
            glBegin(GL_POLYGON);
            for (int j = 0; j < 360; j++) {
                float degRad = j * (M_PI / 180.0f);
                float x = cosf(degRad) * 10.0f;
                float y = sinf(degRad) * 10.0f;
                glVertex2f(x, y);
            }
            glEnd();
        }

        glPopMatrix();
    }
}



void readConfigFile(const char* filename, Player players[], int maxPlayers) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        perror("Could not open config file");
        exit(EXIT_FAILURE);
    }
    char line[256];
    int idx = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        int pid, tid;
        double eng;
        if (sscanf(line, "%d %d %lf", &pid, &tid, &eng) == 3) {
            if (idx < maxPlayers) {
                players[idx].id = pid;
                players[idx].team = tid;
                players[idx].energy = eng;
                idx++;
            }
        }
    }
    fclose(fp);
}

// Rope functions
void initRope(Rope* rope, int numNodes, double totalLength, double startX, double startY) {
    rope->numNodes = numNodes;
    rope->nodes = (Node*)calloc(numNodes, sizeof(Node));
    rope->maxStretch = totalLength / (numNodes - 1);
    double spacing = totalLength / (numNodes - 1);
    for (int i = 0; i < numNodes; i++) {
        rope->nodes[i].location.x = startX + i * spacing;
        rope->nodes[i].location.y = startY;
        rope->nodes[i].velocity.x = 0.0;
        rope->nodes[i].velocity.y = 0.0;
        rope->nodes[i].isFixed = (i == 0);
        rope->nodes[i].above = (i > 0) ? &rope->nodes[i - 1] : NULL;
        rope->nodes[i].below = NULL;
        if (i > 0) {
            rope->nodes[i - 1].below = &rope->nodes[i];
        }
    }
}

void updateRope(Rope* rope) {
    if (!rope || !rope->nodes) return;
    //pe->nodes[0].location.x = 180.0f + ropeShift;  // 350 is your original startX
    // Apply tension between nodes.
    for (int i = 0; i < rope->numNodes; i++) {
        Node* n = &rope->nodes[i];
        if (n->isFixed) {
            n->velocity.x = 0.0;
            n->velocity.y = 0.0;
            continue;
        }
        if (n->above) {
            double dx = n->above->location.x - n->location.x;
            double dy = n->above->location.y - n->location.y;
            double dist = sqrt(dx * dx + dy * dy);
            if (dist > rope->maxStretch) {
                double overshoot = dist - rope->maxStretch;
                double ratio = overshoot / dist;
                double pullX = dx * ratio * 0.5;
                double pullY = dy * ratio * 0.5;
                n->velocity.x += pullX;
                n->velocity.y += pullY;
                if (!n->above->isFixed) {
                    n->above->velocity.x -= pullX;
                    n->above->velocity.y -= pullY;
                }
            }
        }
    }
    // Update node positions with friction and ropeShift.
    for (int i = 0; i < rope->numNodes; i++) {
        Node* n = &rope->nodes[i];
        if (!n->isFixed) {
            n->location.x += n->velocity.x + ropeShift * 0.05;
            n->location.y += n->velocity.y;
            n->velocity.x *= 0.97;
            n->velocity.y *= 0.97;
        }
    }
}

void drawRope(const Rope* rope) {
    if (!rope || !rope->nodes) return;
    glColor3f(1.0, 0.0, 0.0);
    glLineWidth(2.0f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < rope->numNodes; i++) {
        glVertex2f(rope->nodes[i].location.x, rope->nodes[i].location.y);
    }
    glEnd();
    glLineWidth(1.0f);
    for (int i = 0; i < rope->numNodes; i++) {
        Node* n = &rope->nodes[i];
        glBegin(GL_POLYGON);
        float radius = 5.0f;
        for (int j = 0; j < 360; j++) {
            float degRad = j * (M_PI / 180.0f);
            float x = n->location.x + cosf(degRad) * radius;
            float y = n->location.y + sinf(degRad) * radius;
            glVertex2f(x, y);
        }
        glEnd();
    }
}

void freeRope(Rope* rope) {
    if (rope && rope->nodes) {
        free(rope->nodes);
        rope->nodes = NULL;
    }
    rope->numNodes = 0;
}