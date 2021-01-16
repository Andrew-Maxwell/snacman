#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <list>
#include <raylib.h>
#include <raymath.h>
#include <vector>
#if defined(PLATFORM_WEB)
#include <emscripten.h>
#endif

#define WIDTH 800
#define HEIGHT 600
#define GRID 40
#define INDICATOR_THICKNESS 10
#define SLOWTICK 24
#define FASTTICK 12

using namespace std;

struct V2 {
    int x, y;

    V2(int newX, int newY) : x(newX), y(newY) {}
    V2() {x = y = 0;}

    bool operator==(const V2& other) {
        return x == other.x && y == other.y;
    }

    bool operator!=(const V2& other) {
        return x != other.x || y != other.y;
    }

    V2 operator+(const V2& other) {
        return V2(x + other.x, y + other.y);
    }

    V2 operator-(const V2& other) {
        return V2(x - other.x, y - other.y);
    }

    V2 operator*(int scalar) {
        return V2(x * scalar, y * scalar);
    }
};

struct spider {

};

struct compass {
    int clockwise = -1;
    V2 cardinal[4] = {V2(0, -1), V2(1, 0), V2(0, 1), V2(-1, 0)};

    void reverse() {
        clockwise *= -1;
    }

    V2 get(V2 current, int offset) {
        int index = -1;
        for (int i = 0; i < 4; i++) {
            if (cardinal[i] == current) {
                index = i;
            }
        }
        index = ((index + offset * clockwise) + 4) % 4;
        return cardinal[index];
    }
};

struct segment {
    V2 pos; //Absolute world position
    V2 forward; //Direction from previous segment to this segment
    V2 down;    //Direction from this segment to wall it's against

    segment(V2 newPos, V2 newForward, V2 newDown) : pos(newPos), forward(newForward), down(newDown) {}
};

struct mainData {

    vector<string> map;
    int mapWidth = 0;
    list<segment> snake;
    list<segment> moveQueue;
    list<spider> spiders;
    int snakeSize = 1;
    int tickCount = 0;
    compass c;
    bool pause = false;
    RenderTexture2D canvas;
    Vector2 camera = {0, 0};
    bool cameraX, cameraY;

    // update rate for game logic is 60fps/tickRate()
    int tickRate() {
        if (pause) { return INT_MAX; }
        else if (IsKeyDown(KEY_LEFT_SHIFT)) { return FASTTICK; }
        else { return SLOWTICK; }
    }

    void readLevel(string levelName) {
        ifstream level(levelName);
        if (!level) {
            cerr << "Couldn't open " << levelName << endl;
            exit(EXIT_FAILURE);
        }
        string line;
        while (getline(level, line)) {
            mapWidth = max((int)line.size(), mapWidth);
            int snakePos = line.find('S');
            if (snakePos != string::npos) {
                snake.push_front(segment(V2(snakePos, map.size()), V2(1, 0), V2(0, 0)));
            }
            map.push_back(line);
        }
        level.close();
        canvas = LoadRenderTexture(mapWidth * GRID, map.size() * GRID);
        cameraX = mapWidth * GRID > WIDTH;
        cameraY = map.size() * GRID > HEIGHT;
    }

    void mainLoop() {
        BeginDrawing();
        //DO THE FOLLOWING AT TICK RATE
        if (tickCount % tickRate() == 0) {
            //Snake movement: Wall following
            if (!moveQueue.empty()) {
                //Move queue is filled when we start crossing a gap
                snake.push_front(*moveQueue.begin());
                moveQueue.pop_front();
            }
            else {  //Wall following
                for (int i = -1; i < 3; i++) {
                    V2 nextForward = c.get(snake.begin()->forward, i);
                    V2 nextPos = snake.begin()->pos + nextForward;
                    V2 nextDown = c.get(nextForward, -1);
                    if (map[nextPos.y][nextPos.x] != '#') {
                        segment next(nextPos, nextForward, nextDown);
                        snake.push_front(next);
                        break;
                    }
                }
            }
            V2 head = snake.begin()->pos;
            // found apple, add a new segment
            if (map[head.y][head.x] == 'A') {
                snakeSize++;
                // remove apple from map
                map[head.y][head.x] = '.';
            }
            // Remove any extra segments (Every tick unless found apple)
            while (snake.size() > snakeSize) {
                snake.pop_back();
            }
            BeginTextureMode(canvas);
            ClearBackground(BLACK);
            // draw map
            for (int row = 0; row < map.size(); row++) {
                for (int col = 0; col < map[row].size(); col++) {
                    if (map[row][col] == '#') {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, GRAY);
                    }
                    if (map[row][col] == 'A') {
                        DrawCircle((col + 0.5) * GRID, (row + 0.5) * GRID, GRID / 2, RED);
                    }
                }
            }
            // draw snake
            int segCount = 0;
            for (segment & s : snake) {
                DrawCircle((s.pos.x + 0.5) * GRID, (s.pos.y + 0.5) * GRID, GRID / 2, Fade(YELLOW, float(snakeSize - segCount) / snakeSize));
                // draw indicator of which side we are on
                for (V2 adj : {s.down, s.forward}) {
                    if ((adj.x == 0) && (adj.y == 0)) continue;
                    int w = adj.x != 0 ? INDICATOR_THICKNESS : GRID;
                    int h = adj.y != 0 ? INDICATOR_THICKNESS : GRID;

                    V2 logicalPos = s.pos + adj;
                    // draw the indicator on the inside of the wall we are on
                    int x = logicalPos.x * GRID + (adj.x < 0 ? GRID - INDICATOR_THICKNESS : 0);
                    int y = logicalPos.y * GRID + (adj.y < 0 ? GRID - INDICATOR_THICKNESS : 0);
                    // only draw on tiles that would be floor
                    if (map[logicalPos.y][logicalPos.x] == '#') {
                        DrawRectangle(x, y , w, h, BLUE);
                    }
                }
            }
            EndTextureMode();
        }
        //DO THE FOLLOWING AT 60FPS

        if (IsKeyPressed(KEY_SPACE) && moveQueue.empty()) {
            //Crossing to opposite wall
            V2 pos = snake.begin()->pos;
            V2 up = c.get(snake.begin()->down, 2);
            bool canCross = false;
            for (int i = 1; i < snakeSize + 1; i++) {
                V2 swapWall = pos + up * i;
                if (map[swapWall.y][swapWall.x] == '#') {
                    canCross = true;
                    //Following opposite wall now
                    c.reverse();
                    break;
                }
                else {
                    moveQueue.push_back(segment(swapWall, snake.begin()->forward, V2()));
                }
            }
            if (!canCross) {
                moveQueue.clear();
            }
        }
        // debug: pause the game if we press backspace
        if (IsKeyPressed(KEY_BACKSPACE)) {
            // toggle pause
            pause = !pause;
        }
        //Update camera position to keep snake head near center of screen
        V2 pos = snake.begin()->pos;
        float cameraSpeed = (float)GRID / tickRate();
        Vector2 targetCamera = camera;
        if (cameraX) {
            targetCamera.x = min(mapWidth * GRID - WIDTH, max(0, int((pos.x + 0.5) * GRID - WIDTH / 2)));
        }
        if (cameraY) {
            targetCamera.y = min((int)map.size() * GRID - HEIGHT, max(0, int((pos.y + 0.5) * GRID - HEIGHT / 2)));
        }
        if (Vector2Length(Vector2Subtract(targetCamera, camera)) < cameraSpeed) {
            camera = targetCamera;
        }
        else {
            camera = Vector2Add(camera, Vector2Scale(Vector2Normalize(Vector2Subtract(targetCamera, camera)), cameraSpeed));
        }
        ClearBackground(BLACK);
        Texture* t = &canvas.texture;
        Rectangle source = {0, 0, (float)t->width, -1 * (float)t->height};
        Rectangle dest = {0, 0, t->width, t->height};
        float rotation = 0;
        DrawTexturePro(canvas.texture, source, dest, camera, rotation, WHITE);

        EndDrawing();
        tickCount++;
    }

};

mainData everything;
void doEverything() {
    everything.mainLoop();
}

int main(int argc, char** argv) {

    if (argc != 2) {
        cerr << "Usage: " << argv[0] << "<level file>\n";
        exit(EXIT_FAILURE);
    }

    InitWindow(WIDTH, HEIGHT, "snacman");
    everything.readLevel(argv[1]);
#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(doEverything, 6, 1);
#else
    SetTargetFPS(60);
    while (!WindowShouldClose()) {
        everything.mainLoop();
    }
#endif
}
