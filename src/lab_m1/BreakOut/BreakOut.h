#pragma once

#include "components/simple_scene.h"
#include "lab_extra/basic_text/basic_text.h"


namespace m1
{
    class BreakOut : public gfxc::SimpleScene
    {
     public:
         BreakOut();
        ~BreakOut();

        void Init() override;

     private:
         enum BlockType {
             BT_None = -1, BT_Solid = 0
         };

        void FrameStart() override;
        void Update(float deltaTimeSeconds) override;
        void FrameEnd() override;

        void OnInputUpdate(float deltaTime, int mods) override;
        void OnKeyPress(int key, int mods) override;
        void OnKeyRelease(int key, int mods) override;
        void OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY) override;
        void OnMouseBtnPress(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY) override;
        void OnWindowResize(int width, int height) override;

        void RenderEditor();

        void RenderGrid();
        void RenderComponents();
        void RenderCurrency();
        void RenderStartButton();
        void RenderPlacedBlocks();
        void RenderGhost();

        void RenderGame();

        bool ScreenToGridCell(float sx, float sy, int& outRow, int& outCol) const;
        bool PointInGrid(float sx, float sy) const;
        glm::vec2 CellTopLeft(int row, int col) const;

        bool ValidateShipWith(const std::vector<std::vector<BlockType>>& occ) const;
        int CountBlocks(const std::vector<std::vector<BlockType>>& occ) const;
        bool CanPlace(BlockType t, int row, int col) const;
        bool IsConnected(const std::vector<std::vector<BlockType>>& occ) const;

        bool IsStartButtonHot(float sx, float sy) const;
        bool ValidateShip();
        bool IsRectangularShip(const std::vector<std::vector<BlockType>>& occ) const;

        struct Brick {
            glm::vec2 pos;
            glm::vec2 size;
            int  hp = 1;
            int  maxHp = 1;
            bool alive = true;
            bool dying = false;
            float breakT = 1.0f;

            float hitCooldown = 0.0f;
        };

        struct Particle {
            glm::vec2 pos;
            glm::vec2 vel;
            float life;
            float size;
            glm::vec3 color;
        };

        bool inGame = false;
        glm::vec2 paddlePos{ 0,0 };
        glm::vec2 paddleSize{ 160, 24 };
        float paddleSpeed = 520.0f;
        float paddleVX = 0.0f;

        glm::vec2 ballPos{ 0,0 };
        glm::vec2 ballVel{ 0,0 };
        float     ballSpeed = 800.0f;
        float     ballSize = 16.0f;
        bool      ballStuck = true;

        int lives = 3;
        int score = 0;

        std::vector<Brick> bricks;

        void EnterGameFromEditor();
        void ResetBall();
        void BuildBricks();
        void UpdateGame(float dt);
        void RenderHUD();

     public:
         glm::vec2 resolution;

         bool dragging = false;
         BlockType dragType = BT_None;
         glm::vec2 cursor{ 0.f, 0.f };

         int gridRows = 9, gridCols = 17;
         float gridSquare = 33.0f, gridSpacing = 15.0f;
         std::vector<std::vector<BlockType>> gridOcc;
         glm::vec2 gridOrigin{ 0.f, 0.f };

         bool editor = true;
         bool isEnabled = true;

         const int kMaxBlocks = 10;
         int remaining = kMaxBlocks;

         float shakeTime = 0.0f;
         float shakeDur = 0.0f;
         float shakeAmp = 0.0f;

         std::vector<Particle> particles;

         gfxc::TextRenderer* textRenderer = nullptr;

         void TriggerShake(float duration, float amplitude);
         void SpawnBrickParticles(const Brick& b, int count);
    };
} 
