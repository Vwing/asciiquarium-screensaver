// AsciiquariumApp.cpp
// GDI-based ASCII aquarium screensaver renderer
// Launched as a child process by AsciiquariumWrapper.scr

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <map>
#include <sstream>
#include "AsciiAssets.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

// Colors

const COLORREF C_BLK = RGB(0,   0,   0  );
const COLORREF C_CYN = RGB(0,   170, 170);
const COLORREF C_CYB = RGB(0,   255, 255);
const COLORREF C_GRN = RGB(0,   170, 0  );
const COLORREF C_GRB = RGB(0,   255, 0  );
const COLORREF C_WHT = RGB(255, 255, 255);
const COLORREF C_GRY = RGB(170, 170, 170);
const COLORREF C_YLW = RGB(170, 170, 0  );
const COLORREF C_YLB = RGB(255, 255, 0  );
const COLORREF C_RED = RGB(170, 0,   0  );
const COLORREF C_REB = RGB(255, 0,   0  );
const COLORREF C_MAG = RGB(170, 0,   170);
const COLORREF C_MAB = RGB(255, 0,   255);
const COLORREF C_BLU = RGB(0,   0,   170);
const COLORREF C_BLB = RGB(0,   0,   255);

// Types

struct Cell {
    char     ch  = ' ';
    COLORREF col = C_BLK;
};

// A frame of ASCII art. '?' is always transparent. For auto_trans-style art,
// spaces connected to the outside are transparent, while enclosed spaces erase
// lower layers like the terminal version's opaque interior.
struct ArtFrame {
    std::vector<std::string> rows;
    std::vector<std::string> maskRows;
    std::vector<std::string> outsideSpace;
    COLORREF color = C_WHT;
    int w = 0, h = 0;
    bool transparentSpaces = false;

    ArtFrame() = default;
    ArtFrame(const char* art, COLORREF c, bool transSpaces = false) : ArtFrame(std::string(art), std::string(), c, transSpaces) {}
    ArtFrame(const std::string& art, COLORREF c, bool transSpaces = false) : ArtFrame(art, std::string(), c, transSpaces) {}
    ArtFrame(const std::string& art, const std::string& mask, COLORREF c, bool transSpaces = false)
        : color(c), transparentSpaces(transSpaces) {
        parseLines(art, rows);
        parseLines(mask, maskRows);
        for (const auto& line : rows) {
            if ((int)line.size() > w) w = (int)line.size();
        }
        h = (int)rows.size();
        buildOutsideSpace();
    }

    static void parseLines(const std::string& text, std::vector<std::string>& out) {
        const char* p = text.c_str();
        std::string line;
        while (*p) {
            if (*p == '\n') {
                out.push_back(line);
                line.clear();
            } else {
                line += *p;
            }
            ++p;
        }
        if (!line.empty()) {
            out.push_back(line);
        }
    }

    char charAt(int x, int y) const {
        if (x < 0 || y < 0 || y >= h || x >= w) return ' ';
        const std::string& line = y < (int)rows.size() ? rows[y] : "";
        return x < (int)line.size() ? line[x] : ' ';
    }

    bool isOutsideSpace(int x, int y) const {
        if (x < 0 || y < 0 || y >= h || x >= w) return true;
        return !outsideSpace.empty() && outsideSpace[y][x] != 0;
    }

    bool isTransparent(int x, int y) const {
        char c = charAt(x, y);
        if (c == '?') return isOutsideSpace(x, y);
        if (c != ' ') return false;
        return transparentSpaces || isOutsideSpace(x, y);
    }

    void buildOutsideSpace() {
        outsideSpace.assign(h, std::string(w, 0));
        if (w <= 0 || h <= 0) return;
        std::vector<std::pair<int, int>> stack;
        auto enqueue = [&](int x, int y) {
            if (x < 0 || y < 0 || x >= w || y >= h || outsideSpace[y][x]) return;
            char c = charAt(x, y);
            if (c != ' ' && c != '?') return;
            outsideSpace[y][x] = 1;
            stack.push_back(std::make_pair(x, y));
        };
        for (int x = 0; x < w; ++x) {
            enqueue(x, 0);
            enqueue(x, h - 1);
        }
        for (int y = 0; y < h; ++y) {
            enqueue(0, y);
            enqueue(w - 1, y);
        }
        while (!stack.empty()) {
            auto p = stack.back();
            stack.pop_back();
            enqueue(p.first + 1, p.second);
            enqueue(p.first - 1, p.second);
            enqueue(p.first, p.second + 1);
            enqueue(p.first, p.second - 1);
        }
    }
};

struct Entity {
    bool        alive   = false;
    std::string tag;

    float x = 0, y = 0;    // position in cell units
    float vx = 0, vy = 0;  // velocity in cells / animation tick
    int   depth = 10;       // higher = drawn first (background)

    std::vector<ArtFrame> frames;
    int   frame     = 0;
    int   frameTick = 0;
    int   frameRate = 0;    // ticks per frame (0 = static)
    float frameAcc  = 0.0f;
    float framePeriod = 0.0f;
    std::vector<int> frameSequence;
    int   frameSeqIndex = 0;

    bool  dieOff = false;   // die when fully off screen
    int   life   = -1;      // ticks remaining (-1 = infinite)
    bool  spawnOnDeath = false;
    bool  countAsFish = false;
    bool  hidden = false;
    bool  retracting = false;
    bool  spawnOnRetractDeath = false;
    bool  dolphinLead = false;
    int   group = 0;

    bool  hasPath = false;
    std::vector<std::pair<float, float>> path;
    int   pathDelay = 0;
    int   pathIndex = 0;
    float pathAcc = 0.0f;
    float pathPeriod = 1.0f;

    bool  submarine = false;
    int   subDelay = 0;

    int fw() const { return frames.empty() ? 0 : frames[frame].w; }
    int fh() const { return frames.empty() ? 0 : frames[frame].h; }
};

// Globals

static HWND   g_hwnd      = NULL;
static HWND   g_ownerHwnd = NULL;
static bool   g_previewMode = false;
static HFONT  g_font      = NULL;
static int    g_cW        = 8;
static int    g_cH        = 16;
static int    g_cols      = 80;
static int    g_rows      = 24;
static int    g_sW        = 0;
static int    g_sH        = 0;

static std::vector<std::vector<Cell>> g_buf;
static std::vector<Entity>            g_entities;

static HANDLE g_exitEvt    = NULL;
static DWORD  g_startTick  = 0;

static int    g_tick            = 0;
static int    g_fishCount       = 0;
static bool   g_spawningCreature = false;
static int    g_nextGroup       = 1;

static const int kTimerMs = 100;
static const float kTickSeconds = (float)kTimerMs / 1000.0f;
static const int kTargetScreenRows = 96;

// Buffer

static void clearBuf() {
    for (auto& row : g_buf)
        for (auto& cell : row) { cell.ch = ' '; cell.col = C_BLK; }
}

static COLORREF maskColor(char m, COLORREF fallback) {
    switch (m) {
    case 'w': return C_GRY;
    case 'W': return C_WHT;
    case 'c': return C_CYN;
    case 'C': return C_CYB;
    case 'r': return C_RED;
    case 'R': return C_REB;
    case 'y': return C_YLW;
    case 'Y': return C_YLB;
    case 'b': return C_BLU;
    case 'B': return C_BLB;
    case 'g': return C_GRN;
    case 'G': return C_GRB;
    case 'm': return C_MAG;
    case 'M': return C_MAB;
    default:  return fallback;
    }
}

static std::string randomizeMask(std::string mask, bool whiteEye = false) {
    static const char colors[] = "cCrRyYbBgGmM";
    std::map<char, char> digitColors;
    for (char d = '1'; d <= '9'; ++d) {
        digitColors[d] = colors[rand() % (sizeof(colors) - 2)];
    }
    if (whiteEye) digitColors['4'] = 'W';
    for (char& c : mask) {
        if (c >= '1' && c <= '9')
            c = digitColors[c];
    }
    return mask;
}

static void drawArt(const ArtFrame& f, int bx, int by, bool protectExisting = false) {
    for (int row = 0; row < f.h; ++row) {
        int gy = by + row;
        if (gy < 0 || gy >= g_rows) continue;
        const std::string& line = (row < (int)f.rows.size()) ? f.rows[row] : "";
        const std::string& mask = (row < (int)f.maskRows.size()) ? f.maskRows[row] : "";
        for (int col = 0; col < f.w; ++col) {
            int gx = bx + col;
            if (gx < 0 || gx >= g_cols) continue;
            if (protectExisting && g_buf[gy][gx].ch != ' ') continue;
            char c = (col < (int)line.size()) ? line[col] : ' ';
            if (f.isTransparent(col, row)) continue;
            char m = (col < (int)mask.size()) ? mask[col] : ' ';
            g_buf[gy][gx].ch  = (c == ' ') ? '\0' : c;
            g_buf[gy][gx].col = maskColor(m, f.color);
        }
    }
}

static const AsciiAsset* asset(const std::string& name) {
    const auto& assets = asciiAssets();
    for (const auto& a : assets) {
        if (a.name == name) return &a;
    }
    return nullptr;
}

static ArtFrame frameFromVariant(const AsciiVariant& v, COLORREF fallback,
                                 bool randomMask = false, bool whiteEye = false) {
    return ArtFrame(v.image, randomMask ? randomizeMask(v.mask, whiteEye) : v.mask, fallback);
}

static int randRange(int minVal, int maxExclusive) {
    if (maxExclusive <= minVal) return minVal;
    return minVal + rand() % (maxExclusive - minVal);
}

static bool solidAt(const ArtFrame& f, int x, int y) {
    return !f.isTransparent(x, y);
}

static bool entitiesCollide(const Entity& a, const Entity& b) {
    if (a.frames.empty() || b.frames.empty()) return false;
    const ArtFrame& af = a.frames[a.frame];
    const ArtFrame& bf = b.frames[b.frame];
    int ax = (int)std::floor(a.x), ay = (int)std::floor(a.y);
    int bx = (int)std::floor(b.x), by = (int)std::floor(b.y);
    int left = std::max(ax, bx);
    int top = std::max(ay, by);
    int right = std::min(ax + af.w, bx + bf.w);
    int bottom = std::min(ay + af.h, by + bf.h);
    if (left >= right || top >= bottom) return false;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (solidAt(af, x - ax, y - ay) && solidAt(bf, x - bx, y - by))
                return true;
        }
    }
    return false;
}

static int secondsToTicks(float seconds) {
    return std::max(1, (int)std::round(seconds / kTickSeconds));
}

static void setFramePeriod(Entity& e, float seconds) {
    e.framePeriod = seconds > 0.0f ? seconds / kTickSeconds : 0.0f;
    e.frameRate = 0;
}

static int minutesToTicks(int minutes) {
    return minutes * 60 * 1000 / kTimerMs;
}

static std::string stripOneLeadingNewline(const std::string& s) {
    return !s.empty() && s[0] == '\n' ? s.substr(1) : s;
}

// Water

static void makeWater() {
    // 4 tiled water rows near top of screen
    struct WR { const char* art; int yOff; int dep; };
    static const WR rows[] = {
        { "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~", 0, 8 },
        { "^^^^ ^^^  ^^^   ^^^    ^^^^      ", 1, 6 },
        { "^^^^      ^^^^     ^^^    ^^     ", 2, 4 },
        { "^^      ^^^^      ^^^    ^^^^^^  ", 3, 2 },
    };
    for (auto& r : rows) {
        Entity e;
        e.alive = true; e.tag = "water";
        e.x = 0; e.y = (float)(5 + r.yOff);
        e.depth = r.dep;
        e.frames.emplace_back(r.art, C_CYN, true);
        g_entities.push_back(e);
    }
}

// Seaweed

static ArtFrame buildSeaweedFrame(int height, bool flip) {
    std::string art;
    for (int i = 1; i <= height; ++i) {
        bool leftFrame = (i % 2) != 0;
        bool drawLeft = flip ? !leftFrame : leftFrame;
        art += drawLeft ? "(\n" : " )\n";
    }
    while (!art.empty() && art.back() == '\n') art.pop_back();
    return ArtFrame(art.c_str(), C_GRN);
}

static void makeSeaweed(int x) {
    int ht = 3 + rand() % 4;
    Entity e;
    e.alive = true; e.tag = "seaweed";
    e.x = (float)x;
    e.y = (float)(g_rows - ht);
    e.depth = 21;
    setFramePeriod(e, 0.25f + ((float)rand() / (float)RAND_MAX) * 0.05f);
    e.life = minutesToTicks(8) + rand() % minutesToTicks(4);
    e.frames.push_back(buildSeaweedFrame(ht, false));
    e.frames.push_back(buildSeaweedFrame(ht, true));
    g_entities.push_back(e);
}

// Castle

static void makeCastle() {
    const AsciiAsset* a = asset("castle");
    if (!a || a->variants.empty()) return;

    Entity e;
    e.alive = true; e.tag = "castle";
    e.x = (float)(g_cols - 32);
    e.y = (float)(g_rows - 13);
    e.depth = 22;
    for (const auto& v : a->variants)
        e.frames.push_back(frameFromVariant(v, C_WHT));
    if (!e.frames.empty())
        e.y = (float)(g_rows - e.frames[0].h);
    if (e.frames.size() >= 6) {
        for (int i = 0; i < 18; ++i) e.frameSequence.push_back(0);
        e.frameSequence.push_back(1);
        e.frameSequence.push_back(2);
        for (int i = 0; i < 18; ++i) e.frameSequence.push_back(3);
        e.frameSequence.push_back(4);
        e.frameSequence.push_back(5);
        e.frame = e.frameSequence[0];
    }
    setFramePeriod(e, 0.1f);
    g_entities.push_back(e);
}

// Fish

static void makeFish() {
    const AsciiAsset* a = (rand() % 12 > 8) ? asset("new_fish") : asset("old_fish");
    if (!a || a->variants.empty()) return;
    int fishNum = rand() % (int)a->variants.size();
    bool goRight = (fishNum % 2) == 0;
    float speed = 0.25f + ((float)rand() / (float)RAND_MAX) * 2.0f;

    const AsciiVariant& v = a->variants[fishNum];
    ArtFrame frame = frameFromVariant(v, C_WHT, true, true);
    int y = randRange(9, g_rows - frame.h);

    Entity e;
    e.alive = true; e.tag = "fish";
    e.y = (float)y;
    e.depth = randRange(3, 20);
    e.dieOff = true;
    e.countAsFish = true;

    if (goRight) {
        e.vx = speed; e.x = (float)(1 - frame.w);
    } else {
        e.vx = -speed; e.x = (float)(g_cols - 2);
    }
    e.frames.push_back(frame);
    g_entities.push_back(e);
    ++g_fishCount;
}

// Shark

static void makeMovingAsset(const std::string& name, COLORREF fallback, float speed, int depth,
                            float y, float leftX, bool randomMask = false, float frameSeconds = 0.0f,
                            bool spawnOnDeath = true, const std::string& tag = "creature") {
    const AsciiAsset* a = asset(name);
    if (!a) return;
    bool goLeft = (rand() % 2) == 1;

    Entity e;
    e.alive = true; e.tag = tag;
    e.depth = depth;
    e.dieOff = true;
    e.spawnOnDeath = spawnOnDeath;
    e.y = y;
    if (frameSeconds > 0.0f) setFramePeriod(e, frameSeconds);

    if (!a->animations.empty()) {
        int dir = goLeft && a->animations.size() > 1 ? 1 : 0;
        for (const auto& v : a->animations[dir])
            e.frames.push_back(frameFromVariant(v, fallback, randomMask));
    } else if (!a->variants.empty()) {
        int dir = goLeft && a->variants.size() > 1 ? 1 : 0;
        e.frames.push_back(frameFromVariant(a->variants[dir], fallback, randomMask));
    }

    if (e.frames.empty()) return;
    if (goLeft) {
        e.vx = -speed;
        e.x = (float)(g_cols - 2);
    } else {
        e.vx = speed;
        e.x = leftX;
    }
    g_entities.push_back(e);
}

static void makeShark() {
    int minY = 9, maxY = g_rows - 19;
    if (maxY <= minY) maxY = minY + 1;
    const AsciiAsset* a = asset("shark");
    if (!a || a->variants.size() < 2) return;
    bool goLeft = (rand() % 2) == 1;
    int dir = goLeft ? 1 : 0;
    float y = (float)(minY + rand() % (maxY - minY));
    float speed = goLeft ? -2.0f : 2.0f;
    float x = goLeft ? (float)(g_cols - 2) : -53.0f;
    int group = g_nextGroup++;

    Entity teeth;
    teeth.alive = true; teeth.tag = "teeth";
    teeth.hidden = true; teeth.group = group;
    teeth.x = goLeft ? x + 9.0f : -9.0f;
    teeth.y = y + 7.0f;
    teeth.vx = speed;
    teeth.depth = 1;
    teeth.frames.emplace_back("*", C_REB);
    g_entities.push_back(teeth);

    Entity shark;
    shark.alive = true; shark.tag = "shark";
    shark.depth = 2; shark.dieOff = true; shark.spawnOnDeath = true; shark.group = group;
    shark.x = x; shark.y = y; shark.vx = speed;
    shark.frames.push_back(frameFromVariant(a->variants[dir], C_CYN));
    g_entities.push_back(shark);
}
#if 0
    static const char* sharkR = R"art(
                              __
                             ( `\
  ,??????????????????????????)   `\
;' `.????????????????????????(     `\__
 ;   `.?????????????__..---''          `~~~~-._
  `.   `.____...--''                       (b  `--._
    >                     _.-'      .((      ._     )
  .`.-`--...__         .-'     -.___.....-(|/|/|/|/'
 ;.'?????????`. ...----`.___.',,,_______......---'
 '???????????'-')art";

    static const char* sharkL = R"art(
                     __
                    /' )
                  /'   (??????????????????????????,
              __/'     )????????????????????????.' `;
      _.-~~~~'          ``---..__?????????????.'   ;
 _.--'  b)                       ``--...____.'   .'
(     _.      )).      `-._                     <
 `\|\|\|\|)-.....___.-     `-.         __...--'-.'.
   `---......_______,,,`.___.'----... .'?????????`.;
                                     `-`???????????`)art";

    bool goRight = (rand() % 2) == 0;
    int minY = 9, maxY = g_rows - 12;
    if (maxY <= minY) maxY = minY + 1;

    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 3;
    e.dieOff = true;
    e.y = (float)(minY + rand() % (maxY - minY));

    if (goRight) {
        e.vx = 0.4f; e.x = -60.0f;
        e.frames.emplace_back(sharkR, C_CYB);
    } else {
        e.vx = -0.4f; e.x = (float)(g_cols + 2);
        e.frames.emplace_back(sharkL, C_CYB);
    }
    g_entities.push_back(e);
}

// Ship

#endif
static void makeShip() {
    makeMovingAsset("ship", C_WHT, 1.0f, 7, 0.0f, -24.0f);
    return;
}
#if 0
    static const char* shipR = R"art(
     |    |    |
    )_)  )_)  )_)
   )___))___))___)\
  )____)____)_____)\\
_____|____|____|____\\\__
\                   /)art";

    static const char* shipL = R"art(
         |    |    |
        (_(  (_(  (_(
      /(___((___((__(
    //(_____(_____(__(
__///____|____|____|_____
    \                   /)art";

    bool goRight = (rand() % 2) == 0;

    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 6; e.dieOff = true;
    e.y = 0.0f;

    if (goRight) {
        e.vx = 0.2f; e.x = -30.0f;
        e.frames.emplace_back(shipR, C_WHT);
    } else {
        e.vx = -0.2f; e.x = (float)(g_cols + 2);
        e.frames.emplace_back(shipL, C_WHT);
    }
    g_entities.push_back(e);
}

// Whale

#endif
static void makeWhale() {
    const AsciiAsset* a = asset("whale");
    if (!a || a->variants.size() < 2) return;
    bool goLeft = (rand() % 2) == 1;
    int dir = goLeft ? 1 : 0;
    int spoutAlign = goLeft ? 1 : 11;
    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 5; e.dieOff = true; e.spawnOnDeath = true; e.y = 0.0f;
    e.vx = goLeft ? -1.0f : 1.0f;
    e.x = goLeft ? (float)(g_cols - 2) : -18.0f;
    setFramePeriod(e, 0.25f);
    std::string whaleImage = stripOneLeadingNewline(a->variants[dir].image);
    std::string whaleMask = stripOneLeadingNewline(a->variants[dir].mask);
    for (int i = 0; i < 5; ++i)
        e.frames.emplace_back("\n\n\n\n" + whaleImage,
                              "\n\n\n\n" + whaleMask, C_WHT);
    for (const auto& spout : a->waterSpout) {
        std::string aligned;
        std::string maskPad;
        std::vector<std::string> lines;
        ArtFrame::parseLines(spout, lines);
        for (size_t i = 0; i < lines.size(); ++i) {
            aligned += std::string(spoutAlign, ' ') + lines[i] + "\n";
            maskPad += "\n";
        }
        e.frames.emplace_back(aligned + whaleImage,
                              maskPad + whaleMask, C_WHT);
    }
    g_entities.push_back(e);
    return;
}
#if 0
    static const char* whaleR = R"art(
        .-----:
      .'       `.
,????/       (o) \
\`._/          ,__))art";

    static const char* whaleL = R"art(
    :-----.
  .'       `.
 / (o)       \????,
(__,          \_.`/)art";

    bool goRight = (rand() % 2) == 0;

    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 6; e.dieOff = true;
    e.y = 1.0f;

    if (goRight) {
        e.vx = 0.15f; e.x = -22.0f;
        e.frames.emplace_back(whaleR, C_WHT);
    } else {
        e.vx = -0.15f; e.x = (float)(g_cols + 2);
        e.frames.emplace_back(whaleL, C_WHT);
    }
    g_entities.push_back(e);
}

// Monster

#endif
static void makeMonster() {
    makeMovingAsset("new_monster", C_GRB, 2.0f, 5, 2.0f, -54.0f, false, 0.25f);
    return;
}
#if 0
    // Sea serpent / monster swimming at the surface
    static const char* monR = R"art(
      _???_?????_???_?????_a_a
    _{.`=`.}??_{.`=`.}??{/ ''\_
_??{.'  _  '.}{.'  _  '.}{|  ._oo)
{ \?{/  .'  \}{/  .'  \}{/  |)art";

    static const char* monL = R"art(
  a_a_?????_???_?????_???_
_/'' \}??_{.`=`.}??_{.`=`.}_
(oo_.  |}{.'  _  '.}{.'  _  '.}???_
    |  \}{/  .'  \}{/  .'  \}??/ })art";

    bool goRight = (rand() % 2) == 0;

    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 6; e.dieOff = true;
    e.y = 2.0f;

    if (goRight) {
        e.vx = 0.35f; e.x = -55.0f;
        e.frames.emplace_back(monR, C_GRB);
    } else {
        e.vx = -0.35f; e.x = (float)(g_cols + 2);
        e.frames.emplace_back(monL, C_GRB);
    }
    g_entities.push_back(e);
}

// Bubble

#endif
static void makeBigFish() {
    bool second = (rand() % 3) > 1;
    int minY = 9, maxY = g_rows - (second ? 14 : 15);
    makeMovingAsset(second ? "big_fish_2" : "big_fish_1", C_YLB, second ? 2.5f : 3.0f, 2,
                    (float)randRange(minY, maxY), second ? -33.0f : -34.0f, true);
}

static void makeSwordFish() {
    makeMovingAsset("sword_fish", C_YLB, 3.5f, 2,
                    (float)randRange(9, g_rows - 14), -33.0f, true);
}

static void makeDucks() {
    makeMovingAsset("duck", C_WHT, 1.0f, 3, 5.0f, -30.0f, false, 0.25f);
}

static void makeSwan() {
    makeMovingAsset("swan", C_WHT, 1.0f, 3, 1.0f, -10.0f);
}

static void makeSubmarine() {
    const AsciiAsset* a = asset("submarine");
    if (!a || a->animations.size() < 2) return;
    bool goLeft = (rand() % 2) == 1;

    Entity e;
    e.alive = true; e.tag = "creature";
    e.depth = 3; e.dieOff = true; e.spawnOnDeath = true;
    e.x = goLeft ? (float)(g_cols - 2) : -40.0f;
    e.y = 6.0f;
    e.vx = goLeft ? -1.0f : 1.0f;
    e.submarine = true;
    for (const auto& v : a->animations[goLeft ? 1 : 0])
        e.frames.push_back(frameFromVariant(v, C_YLB));
    g_entities.push_back(e);
}

static void makeDolphins() {
    const AsciiAsset* a = asset("dolphin");
    if (!a || a->animations.size() < 2) return;
    bool goLeft = (rand() % 2) == 1;
    int dir = goLeft ? 1 : 0;
    float speed = goLeft ? -1.0f : 1.0f;
    float startX = goLeft ? (float)(g_cols - 2) : -13.0f;
    float distance = goLeft ? -15.0f : 15.0f;
    const float ys[] = {5.0f, 2.0f, 8.0f};
    const int delays[] = {24, 12, 0};
    int group = g_nextGroup++;
    for (int i = 0; i < 3; ++i) {
        Entity e;
        e.alive = true; e.tag = "dolphin";
        e.depth = 3; e.dieOff = (i == 0);
        e.spawnOnDeath = (i == 2);
        e.dolphinLead = (i == 0);
        e.group = group;
        e.x = startX - distance * i;
        e.y = ys[i];
        e.hasPath = true;
        e.pathDelay = delays[i];
        e.pathPeriod = 1.0f;
        for (int n = 0; n < 14; ++n) e.path.push_back(std::make_pair(speed, -0.5f));
        for (int n = 0; n < 2; ++n) e.path.push_back(std::make_pair(speed, 0.0f));
        for (int n = 0; n < 14; ++n) e.path.push_back(std::make_pair(speed, 0.5f));
        for (int n = 0; n < 6; ++n) e.path.push_back(std::make_pair(speed, 0.0f));
        setFramePeriod(e, 0.5f);
        for (const auto& v : a->animations[dir])
            e.frames.push_back(frameFromVariant(v, i == 0 ? C_CYB : (i == 1 ? C_BLB : C_BLU)));
        g_entities.push_back(e);
    }
}

static void makeFishhook() {
    static const char* hook = "\n       o\n      ||\n      ||\n/ \\???||\n  \\__//\n  `--'\n";
    int x = 10 + rand() % std::max(1, g_cols - 20);

    Entity line;
    int group = g_nextGroup++;
    line.alive = true; line.tag = "fishline";
    line.group = group;
    line.x = (float)(x + 7); line.y = -54.0f;
    line.vy = 1.0f; line.depth = 6;
    std::string lineArt;
    for (int i = 0; i < 50; ++i) lineArt += "|\n";
    for (int i = 0; i < 6; ++i) lineArt += " \n";
    line.frames.emplace_back(lineArt, C_GRB);
    g_entities.push_back(line);

    Entity e;
    e.alive = true; e.tag = "fishhook";
    e.group = group;
    e.x = (float)x; e.y = -4.0f;
    e.vy = 1.0f; e.depth = 6; e.dieOff = true; e.spawnOnDeath = true;
    e.frames.emplace_back(hook, C_GRB);
    g_entities.push_back(e);

    Entity point;
    point.alive = true; point.tag = "hook_point";
    point.hidden = true; point.group = group;
    point.x = (float)(x + 1); point.y = -2.0f;
    point.vy = 1.0f; point.depth = 1; point.dieOff = true;
    point.frames.emplace_back(".\n\n\\\n?", C_GRB);
    g_entities.push_back(point);
}

static void makeBubble(float bx, float by) {
    static const char* barts[] = { ".", "o", "O", "O", "O" };

    Entity e;
    e.alive = true; e.tag = "bubble";
    e.x = bx; e.y = by;
    e.vy = -1.0f;
    e.depth = 2;
    e.dieOff = true;
    setFramePeriod(e, 0.1f);
    for (auto* a : barts) e.frames.emplace_back(a, C_CYB);
    g_entities.push_back(e);
}

static void makeSplat(float x, float y, int depth) {
    static const char* frames[] = {
        "\n\n   .\n  ***\n   '\n",
        "\n \",*;`\n \"*,**\n *\"'~'\n",
        "  , ,\n \" \",\"'\n *\" *'\"\n  \" ; .\n",
        "* ' , ' `\n' ` * . '\n ' `' \",'\n* ' \" * .\n\" * ', '\n"
    };
    Entity e;
    e.alive = true; e.tag = "splat";
    e.x = x - 4.0f; e.y = y - 2.0f; e.depth = depth - 2;
    e.life = secondsToTicks(1.0f);
    setFramePeriod(e, 0.25f);
    for (auto* f : frames) e.frames.emplace_back(f, C_REB, true);
    g_entities.push_back(e);
}

// Spawn a random surface creature

static void spawnCreature() {
    int type = rand() % 11;
    switch (type) {
    case 0:  makeShip();      break;
    case 1:  makeWhale();     break;
    case 2:  makeMonster();   break;
    case 3:  makeBigFish();   break;
    case 4:  makeShark();     break;
    case 5:  makeFishhook();  break;
    case 6:  makeSwan();      break;
    case 7:  makeDucks();     break;
    case 8:  makeDolphins();  break;
    case 9:  makeSubmarine(); break;
    case 10: makeSwordFish(); break;
    }
}

// Update

static void updateEntities() {
    bool spawnNextCreature = false;
    int seaweedToRespawn = 0;
    std::vector<int> groupsToKill;
    std::vector<int> dolphinGroupsToRelease;
    for (auto& e : g_entities) {
        if (!e.alive) continue;

        // Advance frame animation
        if (e.framePeriod > 0.0f && (int)e.frames.size() > 1) {
            e.frameAcc += 1.0f;
            if (e.frameAcc >= e.framePeriod) {
                e.frameAcc -= e.framePeriod;
                if (!e.frameSequence.empty()) {
                    e.frameSeqIndex = (e.frameSeqIndex + 1) % (int)e.frameSequence.size();
                    e.frame = e.frameSequence[e.frameSeqIndex];
                } else {
                    e.frame = (e.frame + 1) % (int)e.frames.size();
                }
            }
        } else if (e.frameRate > 0 && (int)e.frames.size() > 1) {
            ++e.frameTick;
            if (e.frameTick >= e.frameRate) {
                e.frameTick = 0;
                if (!e.frameSequence.empty()) {
                    e.frameSeqIndex = (e.frameSeqIndex + 1) % (int)e.frameSequence.size();
                    e.frame = e.frameSequence[e.frameSeqIndex];
                } else {
                    e.frame = (e.frame + 1) % (int)e.frames.size();
                }
            }
        }

        // Life countdown
        if (e.life > 0 && --e.life == 0) {
            e.alive = false;
            if (e.countAsFish) --g_fishCount;
            if (e.tag == "seaweed") ++seaweedToRespawn;
            if (e.spawnOnDeath) spawnNextCreature = true;
            continue;
        }

        // Move
        if (e.tag == "water" || e.tag == "castle" || e.tag == "seaweed") {
            // static or water-managed
        } else if (e.retracting) {
            e.y -= 1.0f;
        } else if (e.tag == "fishhook" || e.tag == "fishline" || e.tag == "hook_point") {
            if ((e.y + (float)e.fh()) < (float)g_rows * 0.75f)
                e.y += 1.0f;
        } else if (e.hasPath) {
            e.pathAcc += 1.0f;
            if (e.pathAcc >= e.pathPeriod) {
                e.pathAcc -= e.pathPeriod;
                if (e.pathDelay > 0) {
                    --e.pathDelay;
                } else if (!e.path.empty()) {
                    e.x += e.path[e.pathIndex].first;
                    e.y += e.path[e.pathIndex].second;
                    e.pathIndex = (e.pathIndex + 1) % (int)e.path.size();
                }
            }
        } else if (e.submarine) {
            static const int frameDelay[] = {1, 4, 4, 9, 9, 9, 4, 4, 1};
            float centerX = (float)g_cols / 2.0f - 20.0f;
            bool wouldCrossCenter =
                (e.x < centerX && e.x + e.vx > centerX) ||
                (e.x > centerX && e.x + e.vx < centerX);
            if (!wouldCrossCenter && e.x != centerX) {
                e.x += e.vx;
            } else if (e.frame < (int)e.frames.size() - 1) {
                e.x = centerX;
                if (e.subDelay < frameDelay[e.frame]) {
                    ++e.subDelay;
                } else {
                    e.subDelay = 0;
                    ++e.frame;
                }
            } else {
                e.x += e.vx;
            }
        } else {
            if (e.life > 0 && e.vy > 0.0f && (e.y + (float)e.fh()) >= (float)g_rows * 0.75f) {
                e.vy = 0.0f;
            }
            e.x += e.vx;
            e.y += e.vy;
        }

        // Die off-screen
        if (e.dieOff) {
            if (e.x + (float)e.fw() < 0.0f || e.x > (float)g_cols ||
                e.y + (float)e.fh() < 0.0f  || e.y > (float)g_rows) {
                e.alive = false;
                if (e.countAsFish) --g_fishCount;
                if (e.group && (e.tag == "shark" || e.tag == "fishhook"))
                    groupsToKill.push_back(e.group);
                if (e.dolphinLead && e.group)
                    dolphinGroupsToRelease.push_back(e.group);
                if (e.spawnOnDeath) spawnNextCreature = true;
            }
        }
    }

    for (int group : dolphinGroupsToRelease) {
        for (auto& e : g_entities)
            if (e.alive && e.group == group && e.tag == "dolphin")
                e.dieOff = true;
    }

    for (int group : groupsToKill) {
        for (auto& e : g_entities)
            if (e.alive && e.group == group && (e.tag == "teeth" || e.tag == "hook_point" || e.tag == "fishline"))
                e.alive = false;
    }

    // Physical collisions used by bubbles, shark teeth, and fishhooks.
    std::vector<std::pair<float, float>> splats;
    float surfaceY = -1.0f;
    for (const auto& w : g_entities)
        if (w.alive && w.tag == "water")
            surfaceY = std::max(surfaceY, w.y);
    for (auto& b : g_entities) {
        if (!b.alive || b.tag != "bubble") continue;
        if (surfaceY >= 0.0f && b.y <= surfaceY) {
            b.alive = false;
            continue;
        }
        for (const auto& w : g_entities) {
            if (w.alive && w.tag == "water" && entitiesCollide(b, w)) {
                b.alive = false;
                break;
            }
        }
    }

    for (auto& fish : g_entities) {
        if (!fish.alive || fish.tag != "fish" || fish.retracting) continue;
        for (const auto& obj : g_entities) {
            if (!obj.alive) continue;
            if (obj.tag == "teeth" && entitiesCollide(fish, obj)) {
                splats.push_back(std::make_pair(obj.x, obj.y));
                fish.alive = false;
                --g_fishCount;
                break;
            }
            if (obj.tag == "hook_point" && !obj.retracting && entitiesCollide(fish, obj)) {
                int group = obj.group;
                fish.retracting = true;
                fish.vx = 0.0f;
                fish.vy = -1.0f;
                fish.depth = 5;
                for (auto& hooked : g_entities) {
                    if (hooked.group == group &&
                        (hooked.tag == "fishhook" || hooked.tag == "fishline" || hooked.tag == "hook_point")) {
                        hooked.retracting = true;
                        hooked.vx = 0.0f;
                        hooked.vy = -1.0f;
                    }
                }
                break;
            }
        }
    }

    for (const auto& s : splats)
        makeSplat(s.first, s.second, 3);

    // Remove dead entities
    g_entities.erase(
        std::remove_if(g_entities.begin(), g_entities.end(),
                       [](const Entity& e){ return !e.alive; }),
        g_entities.end());

    for (int i = 0; i < seaweedToRespawn; ++i)
        makeSeaweed(1 + rand() % std::max(1, g_cols - 2));

    // Each fish gets its own Perl-style chance to emit a bubble.
    std::vector<std::pair<float, float>> bubblePos;
    for (const auto& e : g_entities) {
        if (e.alive && e.tag == "fish" && !e.retracting && rand() % 100 > 97) {
            float bx = e.x + (e.vx > 0 ? (float)e.fw() : 0.0f);
            float by = e.y + (float)(e.fh() / 2);
            bubblePos.push_back(std::make_pair(bx, by));
        }
    }
    for (const auto& p : bubblePos)
        makeBubble(p.first, p.second);

    // Maintain fish population
    int targetFish = ((std::max(1, g_rows - 9) * g_cols) / 350);
    if (g_fishCount < targetFish)
        makeFish();

    if (spawnNextCreature && !g_spawningCreature) {
        g_spawningCreature = true;
        spawnCreature();
        g_spawningCreature = false;
    }

    ++g_tick;
}

// Build buffer
static void buildBuffer() {
    clearBuf();

    // Collect pointers sorted backâ†’front (highest depth first)
    std::vector<Entity*> order;
    order.reserve(g_entities.size());
    for (auto& e : g_entities) if (e.alive) order.push_back(&e);

    std::stable_sort(order.begin(), order.end(),
        [](const Entity* a, const Entity* b){ return a->depth > b->depth; });

    for (Entity* ep : order) {
        const Entity& e = *ep;
        if (e.hidden) continue;
        const ArtFrame& f = e.frames[e.frame];

        if (e.tag == "water") {
            // Tile horizontally without scroll; Perl's waterline is stationary.
            int tileW = std::max(1, f.w);
            int bx = 0;
            int by = (int)e.y;
            for (int t = 0; bx + t * tileW < g_cols + tileW; ++t)
                drawArt(f, bx + t * tileW, by, true);
        } else {
            drawArt(f, (int)e.x, (int)e.y);
        }
    }
}

// Paint
static void paint(HDC hdc) {
    buildBuffer();

    HDC     memDC  = CreateCompatibleDC(hdc);
    HBITMAP bmp    = CreateCompatibleBitmap(hdc, g_sW, g_sH);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, bmp);

    // Black background
    RECT r = {0, 0, g_sW, g_sH};
    HBRUSH br = CreateSolidBrush(C_BLK);
    FillRect(memDC, &r, br);
    DeleteObject(br);

    HFONT oldFont = (HFONT)SelectObject(memDC, g_font);
    SetBkMode(memDC, OPAQUE);
    SetBkColor(memDC, C_BLK);

    // Render cell buffer â€“ group consecutive same-color chars per row
    for (int row = 0; row < g_rows; ++row) {
        int y = row * g_cH;
        int col = 0;
        while (col < g_cols) {
            if (g_buf[row][col].ch == ' ') { ++col; continue; }

            COLORREF curCol = g_buf[row][col].col;
            int      start  = col;
            std::string run;
            while (col < g_cols &&
                   g_buf[row][col].col == curCol &&
                   g_buf[row][col].ch  != ' ') {
                run += g_buf[row][col].ch ? g_buf[row][col].ch : ' ';
                ++col;
            }
            SetTextColor(memDC, curCol);
            TextOutA(memDC, start * g_cW, y, run.c_str(), (int)run.size());
        }
    }

    SelectObject(memDC, oldFont);
    BitBlt(hdc, 0, 0, g_sW, g_sH, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDC);
}

// Window procedure

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwnd = hwnd;

        // Tune kTargetScreenRows near the top of this file to make the
        // aquarium denser or larger on screen.
        int fontH = std::max(8, g_sH / kTargetScreenRows);
        g_font = CreateFontA(
            -fontH, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            ANTIALIASED_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

        // Measure actual cell size
        HDC hdc = GetDC(hwnd);
        HFONT old = (HFONT)SelectObject(hdc, g_font);
        SIZE sz;
        GetTextExtentPoint32A(hdc, "W", 1, &sz);
        g_cW = sz.cx; g_cH = sz.cy;
        SelectObject(hdc, old);
        ReleaseDC(hwnd, hdc);

        g_cols = g_sW / std::max(1, g_cW);
        g_rows = g_sH / std::max(1, g_cH);

        // Initialise buffer
        g_buf.assign(g_rows, std::vector<Cell>(g_cols));

        srand((unsigned)time(NULL));

        makeWater();
        makeCastle();

        int nWeeds = g_cols / 15;
        for (int i = 0; i < nWeeds; ++i)
            makeSeaweed(1 + rand() % (g_cols - 2));

        int initFish = (std::max(1, g_rows - 9) * g_cols) / 350;
        for (int i = 0; i < initFish; ++i)
            makeFish();

        spawnCreature();

        SetTimer(hwnd, 1, kTimerMs, NULL);
        g_startTick = GetTickCount();
        return 0;
    }

    case WM_TIMER:
        if (g_exitEvt && WaitForSingleObject(g_exitEvt, 0) == WAIT_OBJECT_0) {
            DestroyWindow(hwnd);
            return 0;
        }
        updateEntities();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        paint(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_KEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
        if (g_previewMode) return 0;
        if (g_exitEvt) SetEvent(g_exitEvt);
        if (g_ownerHwnd && IsWindow(g_ownerHwnd)) PostMessageA(g_ownerHwnd, WM_CLOSE, 0, 0);
        DestroyWindow(hwnd);
        return 0;

    case WM_MOUSEMOVE: {
        if (g_previewMode) return 0;
        static POINT last = {-1, -1};
        POINT cur = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (last.x == -1) { last = cur; return 0; }
        if ((GetTickCount() - g_startTick) < 2000) return 0;
        if (cur.x != last.x || cur.y != last.y) {
            if (g_exitEvt) SetEvent(g_exitEvt);
            if (g_ownerHwnd && IsWindow(g_ownerHwnd)) PostMessageA(g_ownerHwnd, WM_CLOSE, 0, 0);
            DestroyWindow(hwnd);
        }
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        if (g_font) { DeleteObject(g_font); g_font = NULL; }
        if (!g_previewMode) ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// WinMain

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR cmdLine, int) {
    // Parse --exitEvent <name>
    std::string cmd(cmdLine ? cmdLine : "");
    size_t pos = cmd.find("--exitEvent ");
    if (pos != std::string::npos) {
        std::string evName = cmd.substr(pos + 12);
        size_t sp = evName.find(' ');
        if (sp != std::string::npos) evName = evName.substr(0, sp);
        int len = MultiByteToWideChar(CP_UTF8, 0, evName.c_str(), -1, NULL, 0);
        std::wstring wev(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, evName.c_str(), -1, &wev[0], len);
        g_exitEvt = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, wev.c_str());
    }

    pos = cmd.find("--owner ");
    if (pos != std::string::npos) {
        std::string owner = cmd.substr(pos + 8);
        size_t sp = owner.find(' ');
        if (sp != std::string::npos) owner = owner.substr(0, sp);
        g_ownerHwnd = reinterpret_cast<HWND>(
            static_cast<UINT_PTR>(_strtoui64(owner.c_str(), NULL, 10)));
    }

    g_previewMode = cmd.find("--preview") != std::string::npos &&
                    g_ownerHwnd && IsWindow(g_ownerHwnd);

    int sX = 0;
    int sY = 0;
    if (g_previewMode && g_ownerHwnd && IsWindow(g_ownerHwnd)) {
        RECT rc = {};
        GetClientRect(g_ownerHwnd, &rc);
        g_sW = std::max(1L, rc.right - rc.left);
        g_sH = std::max(1L, rc.bottom - rc.top);
    } else {
        // Cover the entire virtual screen (all monitors)
        g_sW = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        g_sH = GetSystemMetrics(SM_CYVIRTUALSCREEN);
        sX = GetSystemMetrics(SM_XVIRTUALSCREEN);
        sY = GetSystemMetrics(SM_YVIRTUALSCREEN);
        if (g_sW < 1) g_sW = GetSystemMetrics(SM_CXSCREEN);
        if (g_sH < 1) g_sH = GetSystemMetrics(SM_CYSCREEN);
    }

    WNDCLASSA wc = {};
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "AsciiquariumApp";
    wc.hCursor       = NULL;
    RegisterClassA(&wc);

    DWORD exStyle = g_previewMode ? 0 : WS_EX_TOPMOST;
    DWORD style = g_previewMode ? (WS_CHILD | WS_VISIBLE) : (WS_POPUP | WS_VISIBLE);

    HWND hwnd = CreateWindowExA(
        exStyle,
        "AsciiquariumApp", "Asciiquarium",
        style,
        sX, sY, g_sW, g_sH,
        g_ownerHwnd, NULL, hInst, NULL);

    if (!hwnd) return 1;

    if (!g_previewMode)
        ShowCursor(FALSE);

    MSG msg = {};
    while (GetMessageA(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (g_exitEvt) CloseHandle(g_exitEvt);
    return (int)msg.wParam;
}
