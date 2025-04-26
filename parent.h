#ifndef PARENT_H
#define PARENT_H

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
// parent.c
extern float ropeShift;
extern float ropeTargetShift;
extern float ropeVelocity;

// ============================
// Basic 2D vector
// ============================
typedef struct {
    double x;
    double y;
} Vec2D;

// ============================
// Player Structure
// - id: Unique player identifier (from config)
// - team: 1 for Team 1, 2 for Team 2
// - energy: Initial/current energy level
// - position: Screen coordinates for visualization
// - positionFactor: Factor (1,2,3,4) used for energy weighting (e.g., lowest energy gets factor 1)
// - fallen: 0 = active, 1 = fallen
// ============================
typedef struct {
    int    id;
    int    team;
    double energy;
    Vec2D  position;
    int    positionFactor;
    int    fallen;
} Player;

// ============================
// Rope Node and Rope Structure
// - The rope is made up of nodes connected in a chain.
// ============================
typedef struct Node {
    Vec2D location;
    Vec2D velocity;
    int   isFixed;
    struct Node* above;
    struct Node* below;
} Node;

typedef struct {
    Node*  nodes;
    int    numNodes;
    double maxStretch;
} Rope;

// ============================
// Function Prototypes for OpenGL & Scene Management
// ============================
void initOpenGL();
void display();
void reshape(int w, int h);
void timerFunc(int value);
void updateScene();
void drawScene();

// ============================
// Function Prototypes for Player Management
// ============================
void initPlayers(Player players[], int count);
void drawPlayers(const Player players[], int count);
void readConfigFile(const char* filename, Player players[], int maxPlayers);

// ============================
// Function Prototypes for Rope Management
// ============================
void initRope(Rope* rope, int numNodes, double totalLength, double startX, double startY);
void updateRope(Rope* rope);
void drawRope(const Rope* rope);
void freeRope(Rope* rope);

#endif // PARENT_H
