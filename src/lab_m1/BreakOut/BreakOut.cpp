#include "lab_m1/BreakOut/BreakOut.h"

#include <vector>
#include <iostream>
#include <queue>

#include <random>

using namespace std;
using namespace m1;

static std::mt19937 g_rng{ std::random_device{}() };
static inline float RandRange(float a, float b) {
    std::uniform_real_distribution<float> d(a, b);
    return d(g_rng);
}

static inline glm::vec3 BrickColor(int hp, int maxHp)
{
    hp = std::max(0, std::min(hp, maxHp));
    if (maxHp <= 1) return glm::vec3(0.20f, 0.80f, 0.30f);

    float t = (float)(hp - 1) / (float)std::max(1, maxHp - 1);

    glm::vec3 c0(0.92f, 0.20f, 0.20f);
    glm::vec3 c1(0.98f, 0.85f, 0.20f);
    glm::vec3 c2(0.20f, 0.80f, 0.30f);

    if (t < 0.5f) {
        float u = t / 0.5f;
        return c0 * (1.0f - u) + c1 * u;
    }
    else {
        float u = (t - 0.5f) / 0.5f;
        return c1 * (1.0f - u) + c2 * u;
    }
}

static inline float Clamp(float x, float a, float b) { return std::max(a, std::min(b, x)); }

static inline glm::vec2 MouseToScene(BreakOut* self, int mx, int my) {
    return { (float)mx, (float)self->resolution.y - (float)my };
}

static inline glm::mat3 Translate(float translateX, float translateY)
{
    return glm::mat3(
        1, 0, 0,
        0, 1, 0,
        translateX, translateY, 1
    );
}

static inline glm::mat3 Scale(float scaleX, float scaleY)
{
    return glm::mat3(
        scaleX, 0, 0,
        0, scaleY, 0,
        0, 0, 1
    );
}

static inline glm::mat3 Rotate(float radians)
{
    float c = cos(radians);
    float s = sin(radians);

    return glm::mat3(
        c, s, 0,
        -s, c, 0,
        0, 0, 1
    );
}

static Mesh* CreateCircle(
    const std::string& name,
    const glm::vec3& color,
    int segments = 48)
{
    segments = std::max(16, segments);

    const float cx = 0.5f, cy = 0.5f, r = 0.5f;

    std::vector<VertexFormat> vtx;
    std::vector<unsigned int> idx;

    vtx.emplace_back(glm::vec3(cx, cy, 0.0f), color); 
    for (int i = 0; i <= segments; ++i) {
        float t = (i / (float)segments) * 2.0f * (float)M_PI;
        float x = cx + r * cosf(t);
        float y = cy + r * sinf(t);
        vtx.emplace_back(glm::vec3(x, y, 0.0f), color);
        idx.push_back(i + 1);
    }

    std::vector<unsigned int> fan;
    for (int i = 1; i < (int)idx.size(); ++i) {
        fan.push_back(0);
        fan.push_back(idx[i - 1]);
        fan.push_back(idx[i]);
    }

    Mesh* m = new Mesh(name);
    m->InitFromData(vtx, fan);
    return m;
}

static Mesh* CreateHeartParam(const std::string& name,
    const glm::vec3& color,
    int samples = 128)
{

    if (samples < 16) samples = 16;

    std::vector<glm::vec2> pts;
    pts.reserve(samples);

    const float TWO_PI = 6.283185307179586f;
    for (int i = 0; i < samples; ++i) {
        float t = (i / (float)samples) * TWO_PI;

        float s = sinf(t);
        float c = cosf(t);

        float x = 16.0f * s * s * s;
        float y = 13.0f * c - 5.0f * cosf(2.0f * t) - 2.0f * cosf(3.0f * t) - cosf(4.0f * t);

        pts.emplace_back(x, y);
    }

    glm::vec2 mn(1e9f), mx(-1e9f);
    for (auto& p : pts) { mn = glm::min(mn, p); mx = glm::max(mx, p); }
    glm::vec2 size = mx - mn;

    for (auto& p : pts) p = (p - mn) / size;

    glm::vec2 center(0.0f);
    for (auto& p : pts) center += p;
    center /= (float)pts.size();

    std::vector<VertexFormat> vtx;
    vtx.reserve(pts.size() + 2);
    vtx.emplace_back(glm::vec3(center, 0.0f), color);
    for (auto& p : pts) vtx.emplace_back(glm::vec3(p, 0.0f), color);
    vtx.emplace_back(glm::vec3(pts[0], 0.0f), color);

    std::vector<unsigned int> idx(vtx.size());
    for (unsigned i = 0; i < vtx.size(); ++i) idx[i] = i;

    Mesh* m = new Mesh(name);
    m->SetDrawMode(GL_TRIANGLE_FAN);
    m->InitFromData(vtx, idx);
    return m;
}

static Mesh* CreateNotchTriangle(const std::string& name, const glm::vec3& color)
{
    std::vector<VertexFormat> v = {
        VertexFormat(glm::vec3(1.0f, 0.0f, 0.0f), color),
        VertexFormat(glm::vec3(1.0f, 1.0f, 0.0f), color),
        VertexFormat(glm::vec3(0.5f, 0.5f, 0.0f), color),
    };

    Mesh* m = new Mesh(name);
    m->InitFromData(v, { 0, 1, 2 });
    return m;
}

static Mesh* CreateSquare(
    const std::string& name,
    glm::vec3 leftBottomCorner,
    float length,
    glm::vec3 color,
    bool fill)
{
    glm::vec3 corner = leftBottomCorner;

    std::vector<VertexFormat> vertices =
    {
        VertexFormat(corner, color),
        VertexFormat(corner + glm::vec3(length, 0, 0), color),
        VertexFormat(corner + glm::vec3(length, length, 0), color),
        VertexFormat(corner + glm::vec3(0, length, 0), color)
    };

    Mesh* square = new Mesh(name);
    std::vector<unsigned int> indices;

    if (!fill) {
        square->SetDrawMode(GL_LINE_LOOP);
        indices = { 0, 1, 2, 3 };
    }
    else
    {
        indices = { 0, 1, 2, 0, 2, 3 };
    }

    square->InitFromData(vertices, indices);
    return square;
}

BreakOut::BreakOut()
{
}


BreakOut::~BreakOut()
{
}


void BreakOut::Init()
{
    resolution = window->GetResolution();
    auto camera = GetSceneCamera();
    camera->SetOrthographic(0, (float)resolution.x, 0, (float)resolution.y, 0.01f, 400);
    camera->SetPosition(glm::vec3(0, 0, 50));
    camera->SetRotation(glm::vec3(0, 0, 0));
    camera->Update();
    GetCameraInput()->SetActive(false);

    {
        Mesh* square = CreateSquare("filled-square", glm::vec3(0, 0, 0), 1, glm::vec3(0, 1, 0), true);
        AddMeshToList(square);
    }
    {
        Mesh* square = CreateSquare("wireframe-square", glm::vec3(0, 0, 0), 1, glm::vec3(0, 1, 0), false);
        AddMeshToList(square);
    }
    {
        Mesh* notch = CreateNotchTriangle("filled-notch-tri", glm::vec3(0, 0, 0));
        AddMeshToList(notch);
    }
    {
        Mesh* heart = CreateHeartParam("filled-heart", glm::vec3(0.93f, 0.17f, 0.27f));
        AddMeshToList(heart);
    }
    {
        Mesh* circle = CreateCircle("filled-circle", glm::vec3(0.95f, 0.95f, 0.95f), 64);
        AddMeshToList(circle);
    }

    gridOcc.assign(gridRows, std::vector<BlockType>(gridCols, BT_None));
    remaining = kMaxBlocks;
    isEnabled = ValidateShip();

    {
        glm::ivec2 res = window->GetResolution();
        textRenderer = new gfxc::TextRenderer(window->props.selfDir, res.x, res.y);
        textRenderer->Load(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::FONTS, "Hack-Bold.ttf"), 24);
    }

    shakeTime = shakeDur = 0.0f;
    shakeAmp = 0.0f;
}


void BreakOut::FrameStart()
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glViewport(0, 0, resolution.x, resolution.y);
}

void BreakOut::TriggerShake(float duration, float amplitude)
{
    shakeDur = std::max(shakeDur, duration);
    shakeTime = std::max(shakeTime, duration);
    shakeAmp = std::max(shakeAmp, amplitude);
}

void BreakOut::SpawnBrickParticles(const Brick& b, int count)
{
    glm::vec2 center = b.pos + b.size * 0.5f;

    for (int i = 0; i < count; ++i) {
        float ang = RandRange(0.0f, 6.2831853f);
        float spd = RandRange(120.0f, 380.0f);
        glm::vec2 vel = { cosf(ang) * spd, sinf(ang) * spd };

        Particle p;
        p.pos = center + glm::vec2(RandRange(-b.size.x * 0.15f, b.size.x * 0.15f),
            RandRange(-b.size.y * 0.15f, b.size.y * 0.15f));
        p.vel = vel;
        p.life = RandRange(0.35f, 0.8f);
        p.size = RandRange(10.0f, 20.0f);
        p.color = BrickColor(std::max(1, b.hp), std::max(1, b.maxHp));

        particles.push_back(p);
    }
}

void BreakOut::UpdateGame(float dt)
{
    for (auto& p : particles) {
        if (p.life <= 0.0f) continue;
        p.life -= dt;
        p.pos += p.vel * dt;
        p.vel.y -= 800.0f * dt;
        p.size = std::max(0.0f, p.size - 60.0f * dt);
    }

    if (ballStuck) return;

    for (auto& b : bricks) {
        if (b.dying) {
            b.breakT -= dt * 5.0f;
            if (b.breakT <= 0.0f) {
                b.breakT = 0.0f;
                b.alive = false;
            }
        }
        if (b.hitCooldown > 0.0f) {
            b.hitCooldown = std::max(0.0f, b.hitCooldown - dt);
        }
    }

    float radius = ballSize * 0.5f;

    ballPos += ballVel * dt;

    if (ballPos.x - radius <= 0.0f) { ballPos.x = radius; ballVel.x = std::abs(ballVel.x); }
    if (ballPos.x + radius >= resolution.x) { ballPos.x = resolution.x - radius; ballVel.x = -std::abs(ballVel.x); }

    if (ballPos.y + radius >= resolution.y) { ballPos.y = resolution.y - radius; ballVel.y = -std::abs(ballVel.y); }

    if (ballPos.y + radius < 0.0f) {
        lives--;
        if (lives <= 0) {
            editor = true;
            inGame = false;
            return;
        }
        else {
            ResetBall();
            return;
        }
    }

    glm::vec2 aMin = paddlePos;
    glm::vec2 aMax = paddlePos + paddleSize;
    glm::vec2 closest{ Clamp(ballPos.x, aMin.x, aMax.x), Clamp(ballPos.y, aMin.y, aMax.y) };
    glm::vec2 diff = ballPos - closest;
    float d2 = glm::dot(diff, diff);

    if (d2 <= radius * radius) {
        glm::vec2 n;
        float left = std::abs(ballPos.x - aMin.x);
        float right = std::abs(ballPos.x - aMax.x);
        float bottom = std::abs(ballPos.y - aMin.y);
        float top = std::abs(ballPos.y - aMax.y);

        if (bottom < top && bottom < left && bottom < right)       n = { 0.0f, -1.0f };
        else if (top < bottom && top < left && top < right)        n = { 0.0f,  1.0f };
        else if (left < right)                                     n = { -1.0f, 0.0f };
        else                                                       n = { 1.0f,  0.0f };

        ballVel = ballVel - 2.0f * glm::dot(ballVel, n) * n;

        float hitT = ((ballPos.x - paddlePos.x) / paddleSize.x) * 2.0f - 1.0f;
        hitT = Clamp(hitT, -1.0f, 1.0f);
        float ang = glm::radians(35.0f) * hitT;
        float spd = glm::length(ballVel);
        glm::vec2 dir = glm::normalize(ballVel);
        float cs = cos(ang), sn = sin(ang);
        glm::vec2 dirRot = { dir.x * cs - dir.y * sn, dir.x * sn + dir.y * cs };
        if (dirRot.y < 0.2f) dirRot.y = 0.2f;
        dirRot = glm::normalize(dirRot);
        ballVel = dirRot * spd;

        ballPos.y = aMax.y + radius + 0.1f;
    }

    for (Brick& b : bricks) {
        if (!b.alive) continue;
        if (b.dying) continue;

        glm::vec2 bMin = b.pos;
        glm::vec2 bMax = b.pos + b.size;

        glm::vec2 closestB{ Clamp(ballPos.x, bMin.x, bMax.x), Clamp(ballPos.y, bMin.y, bMax.y) };
        glm::vec2 d = ballPos - closestB;
        if (glm::dot(d, d) <= radius * radius) {

            float dxLeft = std::abs((ballPos.x - bMin.x));
            float dxRight = std::abs((ballPos.x - bMax.x));
            float dyBottom = std::abs((ballPos.y - bMin.y));
            float dyTop = std::abs((ballPos.y - bMax.y));

            if (std::min(dxLeft, dxRight) < std::min(dyBottom, dyTop)) {
                ballVel.x = -ballVel.x;
                ballPos.x += (ballVel.x > 0 ? 1.0f : -1.0f);
            }
            else {
                ballVel.y = -ballVel.y;
                ballPos.y += (ballVel.y > 0 ? 1.0f : -1.0f);
            }

            if (b.hitCooldown <= 0.0f) {
                b.hp -= 1;
                b.hitCooldown = 0.5f;

                if (b.hp <= 0) {
                    b.hp = 0;
                    b.dying = true;
                    b.breakT = 1.0f;
                    score += 1;
                    TriggerShake(0.25f, 8.0f);
                    SpawnBrickParticles(b, 24 + b.maxHp * 6);
                }
            }

            break;
        }
    }

    bool any = false;
    for (auto& b : bricks) if (b.alive) { any = true; break; }
    if (!any) {
        editor = true;
        inGame = false;
        return;
    }
}

void BreakOut::Update(float deltaTimeSeconds)
{
    resolution = window->GetResolution();

    glViewport(0, 0, resolution.x, resolution.y);

    auto camera = GetSceneCamera();
    camera->SetOrthographic(0, (float)resolution.x, 0, (float)resolution.y, 0.01f, 400);

    glm::vec2 shakeOffset(0.0f);
    if (shakeTime > 0.0f) {
        shakeTime = std::max(0.0f, shakeTime - deltaTimeSeconds);
        float t = (shakeDur > 0.0f) ? (shakeTime / shakeDur) : 0.0f;      // 1..0
        float ampNow = shakeAmp * t * t;  // easing out (quadratic)
        shakeOffset.x = RandRange(-ampNow, ampNow);
        shakeOffset.y = RandRange(-ampNow, ampNow);
    }
    camera->SetPosition(glm::vec3(shakeOffset.x, shakeOffset.y, 50.0f));  // aplica shake
    camera->SetRotation(glm::vec3(0, 0, 0));
    camera->Update();

    glDisable(GL_DEPTH_TEST);

    if (editor) {
        RenderEditor();
    }
    else {
        if (inGame) UpdateGame(deltaTimeSeconds);
        RenderGame();
    }
}


void BreakOut::FrameEnd()
{
}

void BreakOut::OnInputUpdate(float deltaTime, int mods)
{
    if (editor || !inGame) return;

    paddleVX = 0.0f;
    if (window->KeyHold(GLFW_KEY_LEFT) || window->KeyHold(GLFW_KEY_A))  paddleVX -= paddleSpeed;
    if (window->KeyHold(GLFW_KEY_RIGHT) || window->KeyHold(GLFW_KEY_D)) paddleVX += paddleSpeed;

    paddlePos.x += paddleVX * deltaTime;
    paddlePos.x = std::max(0.0f, std::min(paddlePos.x, (float)resolution.x - paddleSize.x));

    if (ballStuck) {
        ballPos.x = paddlePos.x + paddleSize.x * 0.5f;
        ballPos.y = paddlePos.y + paddleSize.y + ballSize * 0.5f + 4.0f;
    }
}


void BreakOut::OnKeyPress(int key, int mods)
{
    if (!editor && inGame) {
        if (key == GLFW_KEY_SPACE && ballStuck) {
            ballStuck = false;
            float dirX = (paddleVX > 30.0f ? 1.0f : (paddleVX < -30.0f ? -1.0f : 1.0f));
            glm::vec2 dir = glm::normalize(glm::vec2(dirX, 1.0f));
            ballVel = dir * ballSpeed;
        }
    }
}


void BreakOut::OnKeyRelease(int key, int mods)
{
    // Add key release event
}


void BreakOut::OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY)
{
    cursor = MouseToScene(this, mouseX, mouseY);
}


bool BreakOut::IsStartButtonHot(float sx, float sy) const
{
    const float pad = 36.0f;
    const float size = 42.0f;
    const float startButtonWidth = 56.0f;

    const float boxX = resolution.x - pad - startButtonWidth;
    const float boxY = resolution.y - pad - size;
    return (sx >= boxX && sx <= boxX + startButtonWidth &&
        sy >= boxY && sy <= boxY + size);
}

void BreakOut::BuildBricks()
{
    bricks.clear();

    int rows = 5, cols = 10;
    float gap = 8.0f;
    float marginX = 80.0f;
    float topY = resolution.y * 0.78f;

    float totalW = resolution.x - 2.0f * marginX - (cols - 1) * gap;
    float bw = totalW / cols;
    float bh = 24.0f;

    for (int r = 0; r < rows; ++r) {
        int rowHp = 1 + (r % 3);
        for (int c = 0; c < cols; ++c) {
            Brick b;
            b.size = { bw, bh };
            b.pos = { marginX + c * (bw + gap), topY - r * (bh + gap) };
            b.hp = b.maxHp = rowHp;
            b.alive = true;
            b.dying = false;
            b.breakT = 1.0f;
            b.hitCooldown = 0.0f;

            bricks.push_back(b);
        }
    }
}

void BreakOut::ResetBall()
{
    ballStuck = true;
    ballSpeed = 390.0f;
    ballPos = { paddlePos.x + paddleSize.x * 0.5f, paddlePos.y + paddleSize.y + ballSize * 0.5f + 4.0f };

    ballVel = { 0.0f, 0.0f };
}

void BreakOut::EnterGameFromEditor()
{
    inGame = true;
    score = 0;
    lives = 3;

    int rmin = gridRows, rmax = -1, cmin = gridCols, cmax = -1;
    for (int r = 0; r < gridRows; ++r)
        for (int c = 0; c < gridCols; ++c)
            if (gridOcc[r][c] != BT_None) {
                rmin = std::min(rmin, r);
                rmax = std::max(rmax, r);
                cmin = std::min(cmin, c);
                cmax = std::max(cmax, c);
            }

    int Wcells = (cmax >= cmin) ? (cmax - cmin + 1) : 3;
    int Hcells = (rmax >= rmin) ? (rmax - rmin + 1) : 1;

    paddleSize.x = std::max(80.0f, Wcells * (gridSquare + gridSpacing) * 0.9f);
    paddleSize.y = std::max(18.0f, Hcells * (gridSquare) * 0.55f);

    paddlePos.x = (resolution.x - paddleSize.x) * 0.5f;
    paddlePos.y = 64.0f;

    ballSize = 16.0f;
    ResetBall();

    BuildBricks();
}

void BreakOut::OnMouseBtnPress(int mouseX, int mouseY, int button, int mods)
{
    glm::vec2 p = MouseToScene(this, mouseX, mouseY);

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        const float panelWidth = resolution.x * 0.18f;
        const int rows = 4;
        const float cellH = resolution.y / (float)rows;

        if (p.x >= 0.f && p.x <= panelWidth) {
            int r = (int)floor(p.y / cellH);
            if (r >= 0 && r < rows) {
                switch (r) {
                case 0: dragType = BT_Solid; break;
                default: dragType = BT_None; break;
                }
                if (dragType != BT_None) {
                    dragging = true;
                    cursor = p;
                }
            }
        }
    }

    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        glm::vec2 p = MouseToScene(this, mouseX, mouseY);
        if (IsStartButtonHot(p.x, p.y) && isEnabled) {
            editor = false;
            EnterGameFromEditor();
            return;
        }
    }

    if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
        int row, col;
        if (ScreenToGridCell(p.x, p.y, row, col)) {
            if (gridOcc[row][col] != BT_None) {
                gridOcc[row][col] = BT_None;
                remaining = std::min(kMaxBlocks, remaining + 1);
                ValidateShip();
            }
        }
    }
}


void BreakOut::OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods)
{
    glm::vec2 p = MouseToScene(this, mouseX, mouseY);

    if (button == GLFW_MOUSE_BUTTON_RIGHT && dragging) {
        int row, col;
        if (ScreenToGridCell(p.x, p.y, row, col)) {
            bool wasEmpty = (gridOcc[row][col] == BT_None);

            gridOcc[row][col] = dragType;

            if (wasEmpty && remaining > 0) {
                remaining = std::max(0, remaining - 1);
            }
            ValidateShip();
        }

        dragging = false;
        dragType = BT_None;
    }
}


void BreakOut::OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY)
{
}


void BreakOut::OnWindowResize(int width, int height)
{
}

void BreakOut::RenderEditor()
{
    RenderGrid();
    RenderComponents();
    RenderCurrency();
    RenderStartButton();

    RenderPlacedBlocks();
    RenderGhost();
}

void BreakOut::RenderGrid()
{
    float margin = 4 * gridSpacing;

    float startX = resolution.x / 2 - 300.f;
    float startY = resolution.y / 7;

    gridOrigin = { startX, startY };

    for (int i = 0; i < gridRows; i++) {
        for (int j = 0; j < gridCols; j++) {
            float x = startX + j * (gridSquare + gridSpacing);
            float y = startY + i * (gridSquare + gridSpacing);

            glm::mat3 modelMatrix = glm::mat3(1);
            modelMatrix *= Translate(x, y);
            modelMatrix *= Scale(gridSquare, gridSquare);
            RenderMesh2D(meshes["filled-square"], modelMatrix, glm::vec3(65 / 255.f, 117 / 255.f, 230 / 255.f));
        }
    }

    float totalWidth = gridCols * gridSquare + (gridCols - 1) * gridSpacing;
    float totalHeight = gridRows * gridSquare + (gridRows - 1) * gridSpacing;

    glm::mat3 frameMatrix = glm::mat3(1);
    frameMatrix *= Translate(startX - margin / 2, startY - margin / 2);
    frameMatrix *= Scale(totalWidth + margin, totalHeight + margin);

    RenderMesh2D(meshes["wireframe-square"], frameMatrix, glm::vec3(0.0f, 0.0f, 1.0f));
}

void BreakOut::RenderComponents()
{
    const float panelWidth = resolution.x * 0.18f;
    const int rows = 4;
    const float cellH = resolution.y / (float)rows;

    for (int r = 0; r < rows; ++r) {
        const float x = 0.0f;
        const float y = r * cellH;

        glm::mat3 model = glm::mat3(1);
        model *= Translate(x, y);
        model *= Scale(panelWidth, cellH);

        RenderMesh2D(meshes["wireframe-square"], model, glm::vec3(1.0f, 0.0f, 0.0f));
    }

    {
        const glm::ivec2 res = window->GetResolution();
        const float inset = 4.0f;

        glm::mat3 m(1);
        m *= Translate(inset * 0.5f, inset * 0.5f);
        m *= Scale(res.x - inset, res.y - inset);

        RenderMesh2D(meshes["wireframe-square"], m, glm::vec3(1.0f, 0.0f, 0.0f));
    }
    {
        const int r = 0;
        const float cellX = 0.0f;
        const float cellY = r * cellH;

        const float desiredSide = gridSquare + 0.5f * gridSpacing;

        const float maxSideInCell = std::min(panelWidth, cellH) * 0.96f;
        const float side = std::min(desiredSide, maxSideInCell);

        const float x = cellX + (panelWidth - side) * 0.5f;
        const float y = cellY + (cellH - side) * 0.5f;

        glm::mat3 m(1);
        m *= Translate(x, y);
        m *= Scale(side, side);

        RenderMesh2D(meshes["filled-square"], m, glm::vec3(0.72f, 0.72f, 0.72f));
    }
}

void BreakOut::RenderCurrency()
{
    const int total = kMaxBlocks;
    const float pad = 36.0f, size = 42.0f, gap = 28.0f;
    const float startButtonWidth = 56.0f;

    const float rowWidth = total * size + (total - 1) * gap;
    const float startX = resolution.x - pad - startButtonWidth - rowWidth - 100.0f;
    const float y = resolution.y - pad - size;

    for (int i = 0; i < total; ++i) {
        const glm::vec3 col = (i < remaining) ? glm::vec3(0.0f, 0.75f, 0.0f)
            : glm::vec3(0.25f, 0.25f, 0.25f);
        glm::mat3 m(1);
        m *= Translate(startX + i * (size + gap), y);
        m *= Scale(size, size);
        RenderMesh2D(meshes["filled-square"], m, col);
    }
}

void BreakOut::RenderStartButton()
{
    const float pad = 36.0f;
    const float size = 42.0f;
    const float startButtonWidth = 56.0f;

    const float boxX = resolution.x - pad - startButtonWidth;
    const float boxY = resolution.y - pad - size;

    const glm::vec3 colFill = isEnabled ? glm::vec3(48 / 255.f, 179 / 255.f, 7 / 255.f) : glm::vec3(0.75f, 0.0f, 0.0f);

    const float shapeW = size;
    const float shapeX = boxX + (startButtonWidth - shapeW) * 0.5f;
    const float shapeY = boxY;

    {
        glm::mat3 m(1);
        m *= Translate(shapeX, shapeY);
        m *= Scale(shapeW, shapeW);
        RenderMesh2D(meshes["filled-square"], m, colFill);
    }

    {
        glm::mat3 m(1);
        m *= Translate(shapeX, shapeY);
        m *= Scale(shapeW, shapeW);
        RenderMesh2D(meshes["filled-notch-tri"], m, glm::vec3(0, 0, 0));
    }
}

glm::vec2 BreakOut::CellTopLeft(int row, int col) const {
    float x = gridOrigin.x + col * (gridSquare + gridSpacing);
    float y = gridOrigin.y + row * (gridSquare + gridSpacing);
    return { x, y };
}

bool BreakOut::PointInGrid(float sx, float sy) const {
    float w = gridCols * gridSquare + (gridCols - 1) * gridSpacing;
    float h = gridRows * gridSquare + (gridRows - 1) * gridSpacing;
    return (sx >= gridOrigin.x && sx <= gridOrigin.x + w &&
        sy >= gridOrigin.y && sy <= gridOrigin.y + h);
}

bool BreakOut::ScreenToGridCell(float sx, float sy, int& outRow, int& outCol) const {
    if (!PointInGrid(sx, sy)) return false;

    float lx = sx - gridOrigin.x;
    float ly = sy - gridOrigin.y;

    float pitchX = gridSquare + gridSpacing;
    float pitchY = gridSquare + gridSpacing;

    int col = (int)floor(lx / pitchX);
    int row = (int)floor(ly / pitchY);

    if (col < 0 || col >= gridCols || row < 0 || row >= gridRows) return false;

    float cx = lx - col * pitchX;
    float cy = ly - row * pitchY;
    if (cx > gridSquare || cy > gridSquare) return false;

    outCol = col;
    outRow = row;
    return true;
}

void BreakOut::RenderPlacedBlocks()
{
    const float side = gridSquare + gridSpacing;
    const float offset = gridSpacing * 0.5f;

    for (int r = 0; r < gridRows; ++r) {
        for (int c = 0; c < gridCols; ++c) {
            BlockType t = gridOcc[r][c];
            if (t == BT_None) continue;

            glm::vec2 tl = CellTopLeft(r, c);

            glm::mat3 m(1);
            m *= Translate(tl.x - offset, tl.y - offset);
            m *= Scale(side, side);

            RenderMesh2D(meshes["filled-square"], m, glm::vec3(0.72f, 0.72f, 0.72f));
        }
    }
}

void BreakOut::RenderGhost()
{
    if (!dragging || dragType == BT_None) return;

    const float side = gridSquare + gridSpacing;
    const float offset = gridSpacing * 0.5f;

    int row, col;
    bool overCell = ScreenToGridCell(cursor.x, cursor.y, row, col);

    glm::mat3 m(1);
    if (overCell) {
        glm::vec2 tl = CellTopLeft(row, col);
        m *= Translate(tl.x - offset, tl.y - offset);
    }
    else {
        m *= Translate(cursor.x - side * 0.5f, cursor.y - side * 0.5f);
    }
    m *= Scale(side, side);

    RenderMesh2D(meshes["filled-square"], m, glm::vec3(0.45f, 0.45f, 0.45f));
}

int BreakOut::CountBlocks(const std::vector<std::vector<BlockType>>& occ) const {
    int cnt = 0;
    for (int r = 0; r < gridRows; ++r)
        for (int c = 0; c < gridCols; ++c)
            if (occ[r][c] != BT_None) ++cnt;
    return cnt;
}

bool BreakOut::IsRectangularShip(const std::vector<std::vector<BlockType>>& occ) const {
    int used = CountBlocks(occ);
    if (used == 0) return false;

    int rmin = gridRows, rmax = -1, cmin = gridCols, cmax = -1;
    for (int r = 0; r < gridRows; ++r)
        for (int c = 0; c < gridCols; ++c)
            if (occ[r][c] != BT_None) {
                rmin = std::min(rmin, r);
                rmax = std::max(rmax, r);
                cmin = std::min(cmin, c);
                cmax = std::max(cmax, c);
            }

    int H = rmax - rmin + 1;
    int W = cmax - cmin + 1;

    if (used != H * W) return false;
    for (int r = rmin; r <= rmax; ++r)
        for (int c = cmin; c <= cmax; ++c)
            if (occ[r][c] == BT_None) return false;

    if (W < H) return false;

    return true;
}

bool BreakOut::IsConnected(const std::vector<std::vector<BlockType>>& occ) const {
    int sr = -1, sc = -1;
    for (int r = 0; r < gridRows && sr < 0; ++r)
        for (int c = 0; c < gridCols; ++c)
            if (occ[r][c] != BT_None) { sr = r; sc = c; break; }

    if (sr < 0) return false;

    std::vector<std::vector<char>> vis(gridRows, std::vector<char>(gridCols, 0));
    std::queue<std::pair<int, int>> q;
    q.push({ sr, sc });
    vis[sr][sc] = 1;
    int seen = 0;

    auto push = [&](int r, int c) {
        if (r < 0 || r >= gridRows || c < 0 || c >= gridCols) return;
        if (vis[r][c]) return;
        if (occ[r][c] == BT_None) return;
        vis[r][c] = 1;
        q.push({ r,c });
        };

    while (!q.empty()) {
        auto [r, c] = q.front(); q.pop();
        ++seen;
        push(r + 1, c); push(r - 1, c); push(r, c + 1); push(r, c - 1);
    }

    return seen == CountBlocks(occ);
}

bool BreakOut::ValidateShipWith(const std::vector<std::vector<BlockType>>& occ) const
{
    const int used = CountBlocks(occ);
    if (used < 1 || used > 10) return false;
    if (!IsConnected(occ)) return false;
    if (!IsRectangularShip(occ)) return false;

    return true;
}

bool BreakOut::ValidateShip()
{
    bool ok = ValidateShipWith(gridOcc);
    isEnabled = ok;
    return ok;
}

bool BreakOut::CanPlace(BlockType t, int row, int col) const
{
    if (t == BT_None) return false;

    int used = CountBlocks(gridOcc);
    if (gridOcc[row][col] == BT_None && remaining <= 0) return false;

    auto tmp = gridOcc;
    tmp[row][col] = t;
    return ValidateShipWith(tmp);
}

void BreakOut::RenderGame()
{
    {
        glm::mat3 m(1);
        m *= Translate(paddlePos.x, paddlePos.y);
        m *= Scale(paddleSize.x, paddleSize.y);
        RenderMesh2D(meshes["filled-square"], m, glm::vec3(0.85f, 0.85f, 0.85f));
    }

    {
        glm::mat3 m(1);
        m *= Translate(ballPos.x - ballSize * 0.5f, ballPos.y - ballSize * 0.5f);
        m *= Scale(ballSize, ballSize);
        RenderMesh2D(meshes["filled-circle"], m, glm::vec3(0.95f, 0.95f, 0.95f));
    }

    for (const auto& p : particles) {
        if (p.life <= 0.0f || p.size <= 0.0f) continue;

        float s = p.size;
        glm::vec3 col = p.color * (0.3f + 0.7f * std::max(0.0f, p.life)); // fade out

        glm::mat3 m(1);
        m *= Translate(p.pos.x - s * 0.5f, p.pos.y - s * 0.5f);
        m *= Scale(s, s);

        RenderMesh2D(meshes["filled-square"], m, col);
    }

    for (auto& b : bricks) {
        if (!b.alive) continue;

        float s = b.dying ? glm::max(0.0f, b.breakT) : 1.0f;
        glm::vec3 col = BrickColor(b.hp, b.maxHp);
        glm::vec2 c = b.pos + b.size * 0.5f;

        glm::mat3 m(1);
        m *= Translate(c.x - b.size.x * 0.5f * s, c.y - b.size.y * 0.5f * s);
        m *= Scale(b.size.x * s, b.size.y * s);

        RenderMesh2D(meshes["filled-square"], m, col);
    }

    RenderHUD();
}

void BreakOut::RenderHUD()
{
    if (!inGame) return;

    const float pad = 16.0f;
    const float size = 28.0f;
    const float gap = 10.0f;

    float x = resolution.x - pad - size;
    float y = resolution.y - pad - size;

    for (int i = 0; i < 3; ++i) {
        glm::vec3 col = (i < lives) ? glm::vec3(0.93f, 0.17f, 0.27f) : glm::vec3(0.35f, 0.35f, 0.35f);

        glm::mat3 m(1);
        m *= Translate(x - i * (size + gap), y);
        m *= Scale(size, size);
        RenderMesh2D(meshes["filled-heart"], m, col);
    }

    if (textRenderer) {
        std::string s = "Score: " + std::to_string(score);
        glm::vec3 txtCol = glm::vec3(0.95f);
        textRenderer->RenderText(s, 16.0f, 16.0f, 1.0f, txtCol);
    }
}
