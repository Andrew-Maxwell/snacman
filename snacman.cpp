#include <algorithm>
#include <cassert>
#include <climits>
#include <fstream>
#include <iostream>
#include <list>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "raylib.h"
#include "raymath.h"
#if defined(PLATFORM_WEB)
#include "emscripten.h"
#endif

#define WIDTH 800
#define HEIGHT 600
#define GRID 40
#define INDICATOR_THICKNESS 10
#define SLOWTICK 24
#define FASTTICK 12

#define WALL '#'
#define SNAKE 'S'
#define PATHWALL '$'
#define PATH 's'
#define EMPTY '.'
#define APPLE 'A'

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

    V2 get(V2& current, int offset) {
        int index = -1;
        for (int i = 0; i < 4; i++) {
            if (cardinal[i] == current) {
                index = i;
            }
        }
        index = ((index + offset * clockwise) + 4) % 4;
        return cardinal[index];
    }

    int cardinalToDegrees(V2& current) {
        if (current == cardinal[0]) {
            return 270;
        } else if (current == cardinal[1]) {
            return 0;
        } else if (current == cardinal[2]) {
            return 90;
        } else {
            return 180;
        }
    }
};

struct segment {
    V2 pos; //Absolute world position
    V2 forward; //Direction from previous segment to this segment
    V2 down;    //Direction from this segment to wall it's against

    segment() {}
    segment(V2 newPos, V2 newForward, V2 newDown) : pos(newPos), forward(newForward), down(newDown) {}
};

struct critter {
    compass c;
    vector<string> moveMap;
    list<segment> segments;

    bool ok(V2 v) {
        return v.y >= 0 && v.y < moveMap.size() && v.x >= 0 && v.x < moveMap[v.y].size();
    }

    // Generate which tiles the snake can traverse (adjacent to wall)
    void makeMoveMap(V2 start, vector<string>& map) {
        moveMap = map;
        list<V2> Q;
        Q.push_back(start);
        moveMap[start.y][start.x] = PATHWALL;
        while (!Q.empty()) {
            V2 next = *Q.begin();
            Q.pop_front();
            for (int i = 0; i < 4; i++) {
                V2 adj = next + c.cardinal[i];
                if (ok(adj)) {
                    if (moveMap[adj.y][adj.x] == WALL) {
                        moveMap[adj.y][adj.x] = PATHWALL;
                        Q.push_back(adj);
                    }
                    else if (moveMap[adj.y][adj.x] != PATHWALL) {
                        moveMap[adj.y][adj.x] = PATH;
                    }
                }
            }
        }
        for (int row = 0; row < moveMap.size(); row++) {
            for (int col = 0; col < moveMap[row].size(); col++) {
                V2 pos(col, row);
                if (moveMap[pos.y][pos.x] == APPLE || moveMap[pos.y][pos.x] == EMPTY) {
                    for (V2 diag : {V2(1, 1), V2(1, -1), V2(-1, 1), V2(-1, -1)}) {
                        V2 adj = pos + diag;
                        if (ok(adj) && moveMap[adj.y][adj.x] == PATHWALL) {
                            moveMap[pos.y][pos.x] = PATH;
                        }
                    }
                }
            }
        }
    }

    segment getNextSegment() {
        int score = 0;
        segment next;
        for (int i = -1; i < 3; i++) {
            V2 nextForward = c.get(segments.begin()->forward, i);
            V2 nextDown = c.get(nextForward, -1);
            V2 nextPos = segments.begin()->pos + nextForward;
            V2 nextWall = nextPos + nextDown;
            if (moveMap[nextPos.y][nextPos.x] == PATH) {
                // +8 points for not going backwards
                int newScore = i != 2 ? 8 : 0;
                // +8 points for not overlapping previous snake
                newScore += 8;
                for (segment& otherS : segments) {
                    if (otherS.pos == nextPos) {
                        newScore -= 8;
                        break;
                    }
                }
                // *4 points for adhering to wall
                newScore += moveMap[nextWall.y][nextWall.x] == PATHWALL ? 4 : 0;
                if (newScore > score) {
                    next = segment(nextPos, nextForward, nextDown);
                    score = newScore;
                }
            }
        }
        assert(score > 0);
        return next;
    }

    virtual void render(bool debug) {}
};


struct snake : public critter {
    list<segment> moveQueue;
    unordered_map<string, Texture2D> textures;
    int snakeSize = 1;

    void initTextures() {
        for (const string& name : {"head_0", "head_1", "body", "tail"}) {
            stringstream filename;
            filename << "assets/" << name << ".png";
            Image img = LoadImage(filename.str().c_str());
            ImageResize(&img, GRID, GRID);
            Texture2D tex = LoadTextureFromImage(img);
            textures[name] = tex;
            UnloadImage(img);
        }
    }

    snake() {}

    snake(V2 head, vector<string>& map) {
        segments.push_front(segment(head, V2(0, 0), V2(0, 0)));
        for (int i = 0; i < 4; i++) {
            V2 adj = segments.begin()->pos + c.cardinal[i];
            if (map[adj.y][adj.x] == WALL) {
                makeMoveMap(adj, map);
                segments.begin()->down = c.cardinal[i];
                segments.begin()->forward = c.get(segments.begin()->down, 1);
            }
        }
        initTextures();
    }

    void doTick(vector<string>& map) {
        //Snake movement: Wall following
        if (!moveQueue.empty()) {
            //Move queue is filled when we start crossing a gap
            segments.push_front(*moveQueue.begin());
            moveQueue.pop_front();
        }
        else {  //Wall following
            segment next = getNextSegment();
            segments.push_front(next);
        }
        V2 head = segments.begin()->pos;
        // found apple, add a new segment
        if (map[head.y][head.x] == APPLE) {
            snakeSize++;
            map[head.y][head.x] = EMPTY;
            // remove apple from map
        }
        // Remove any extra segments (Every tick unless found apple)
        while (segments.size() > snakeSize) {
            segments.pop_back();
        }
    }

    void handleInput(vector<string>& map) {
        V2 head = segments.begin()->pos;
        if (IsKeyPressed(KEY_SPACE) && moveQueue.empty()) {
            //Crossing to opposite wall
            V2 up = c.get(segments.begin()->down, 2);
            bool canCross = false;
            for (int i = 1; i < snakeSize + 1; i++) {
                V2 swapWall = head + up * i;
                for (segment& s : segments) {
                    if (s.pos == swapWall) {
                        break;
                    }
                }
                if (map[swapWall.y][swapWall.x] == WALL) {
                    canCross = true;
                    makeMoveMap(swapWall, map);
                    //Following opposite wall now
                    c.reverse();
                    break;
                }
                else {
                    moveQueue.push_back(segment(swapWall, segments.begin()->forward, V2()));
                }
            }
            if (!canCross) {
                moveQueue.clear();
            }
        }
    }

    void render(bool debug) {
        if (debug) {
            for (int row = 0; row < moveMap.size(); row++) {
                for (int col = 0; col < moveMap[row].size(); col++) {
                    if (moveMap[row][col] == PATHWALL) {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, BLUE);
                    }
                    else if (moveMap[row][col] == PATH) {
                        DrawRectangle(GRID * col, GRID * row, GRID, GRID, Fade(GREEN, 0.5));
                    }
                }
            }
        }
        // draw snake
        for (segment& s : segments) {
            Vector2 center = {s.pos.x * GRID, s.pos.y * GRID};
            Texture2D* tex = nullptr;
            if (&s == &(segments.front())) {
                tex = snakeSize > 1 ? &textures["head_1"] : &textures["head_0"];
            }
            else if (&s == &(segments.back())) {
                tex = &textures["tail"];
            } else {
                tex = &textures["body"];
            }
            int rotation = c.cardinalToDegrees(s.forward);
            Rectangle sourceRec = {0, 0, tex->width, tex->height * -c.clockwise};
            Rectangle destRec = {center.x + tex->width/2, center.y + tex->width/2, tex->width, tex->height};

            DrawTexturePro(*tex, sourceRec, destRec, { tex->width/2, tex->height/2 }, rotation, WHITE);
            if (debug) {
                Vector2 down = Vector2Add(center, Vector2Scale((Vector2){s.down.x, s.down.y}, GRID));
                DrawLineV(center, down, GREEN);
                Vector2 forward = Vector2Add(center, Vector2Scale((Vector2){s.forward.x, s.forward.y}, GRID));
                DrawLineV(center, forward, RED);
            }
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
                /* if (moveMap[logicalPos.y][logicalPos.x] == WALL || */
                /*     moveMap[logicalPos.y][logicalPos.x] == PATHWALL) { */
                /*     DrawRectangle(x, y , w, h, BLUE); */
                if (moveMap[logicalPos.y][logicalPos.x] == PATHWALL) {
                    DrawRectangle(x, y , w, h, YELLOW);
                }
            }
        }
    }

    V2 head() {
        return segments.begin()->pos;
    }

};

struct mainData {

    vector<string> map;
    int mapWidth = 0;
    int tickCount = 0;
    bool pause = false;
    RenderTexture2D canvas;
    Vector2 camera = {0, 0};
    bool moveCameraX, moveCameraY;
    snake s;

    char& at(V2 v) {
        return map[v.y][v.x];
    }

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
        V2 newSnakeHead;
        while (getline(level, line)) {
            mapWidth = max((int)line.size(), mapWidth);
            int snakePos = line.find(SNAKE);
            if (snakePos != string::npos) {
                newSnakeHead = V2(snakePos, map.size());
            }
            map.push_back(line);
        }
        level.close();
        canvas = LoadRenderTexture(mapWidth * GRID, map.size() * GRID);
        moveCameraX = mapWidth * GRID > WIDTH;
        moveCameraY = map.size() * GRID > HEIGHT;
        s = snake(newSnakeHead, map);
    }

    void render(bool debug) {
        BeginTextureMode(canvas);
        ClearBackground(BLACK);
        // draw map
        for (int row = 0; row < map.size(); row++) {
            for (int col = 0; col < map[row].size(); col++) {
                if (map[row][col] == WALL) {
                    DrawRectangle(GRID * col, GRID * row, GRID, GRID, GRAY);
                }
                if (map[row][col] == APPLE) {
                    DrawCircle((col + 0.5) * GRID, (row + 0.5) * GRID, GRID / 2, RED);
                }
            }
        }
        // draw snake
        s.render(debug);
        EndTextureMode();
    }

    void mainLoop() {
        BeginDrawing();
        //DO THE FOLLOWING AT TICK RATE
        if (tickCount % tickRate() == 0) {
            s.doTick(map);
            render(true);
        }
        //DO THE FOLLOWING AT 60FPS
        s.handleInput(map);
        // debug: pause the game if we press backspace
        if (IsKeyPressed(KEY_BACKSPACE)) {
            // toggle pause
            pause = !pause;
            render(true);
        }
        //Update camera position to keep snake head near center of screen
        Vector2 targetCamera = camera;
        if (moveCameraX) {
            targetCamera.x = min(mapWidth * GRID - WIDTH, max(0, int((s.head().x + 0.5) * GRID - WIDTH / 2)));
        }
        if (moveCameraY) {
            targetCamera.y = min((int)map.size() * GRID - HEIGHT, max(0, int((s.head().y + 0.5) * GRID - HEIGHT / 2)));
        }
        Vector2 cameraMove = Vector2Subtract(targetCamera, camera);
        camera = Vector2Add(camera, Vector2Scale(cameraMove, 0.02));
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
