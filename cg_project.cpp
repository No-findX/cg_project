#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "view/game_view.hpp"
#include "viewmodel/game_view_model.hpp"

// GameApplication orchestrates window management, rendering, and gameplay state.
// Mirrors the lifecycle of a typical game loop: init -> run -> shutdown.
class GameApplication {
public:
    GameApplication(int width, int height);
    ~GameApplication();

    bool init();
    void run();

private:
    // Create the GLFW window, configure GL context, and register callbacks.
    bool initWindow();
    // Handle keyboard input (WASD/arrow/escape) and feed commands to the ViewModel.
    void processInput();
    // Poll the ViewModel for win state changes and keep announcements in sync.
    void update();
    // Clear buffers and ask the view to render either UI or gameplay scene.
    void render();

    // Raw mouse events forwarded to the view (camera orbit or UI hover).
    void onMouseMove(double xpos, double ypos);
    void onMouseButton(int button, int action, double xpos, double ypos);

    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

private:
    GLFWwindow* window_ = nullptr;    // GLFW window/context handle.
    GameView view_;                    // Presentation layer (UI + gameplay rendering).
    GameViewModel viewModel_;          // ViewModel powering the gameplay state.
    int windowWidth_ = 0;
    int windowHeight_ = 0;

    double lastFrameTime_ = 0.0;       // Timestamp of previous frame (for delta time).
    double lastInputTime_ = 0.0;       // Timestamp of last processed input (throttling).
    const double inputCooldown_ = 0.2; // Minimum time between accepted inputs.
    bool winAnnounced_ = false;        // Tracks whether win message was printed.
    bool gameStarted_ = false;         // Gated until the Start button is clicked.

    GameState cachedState_{};          // Local copy of game state used for rendering.
    GameState cachedNextState_{};
    bool hasCachedState_ = false;      // Whether cachedState_ currently holds a valid snapshot.

    // 位移动画时序
    bool animatingMove_ = false;
    double moveAnimStart_ = 0.0;
    double moveAnimDuration_ = 0.2; // 缩短动画时长，提高轻快感

    // 动画末段输入缓存
    bool hasPendingInput_ = false;
    Input pendingInput_ = UP;
};

GameApplication::GameApplication(int width, int height)
    : windowWidth_(width), windowHeight_(height) {}

GameApplication::~GameApplication() {
    view_.shutdown();
    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

bool GameApplication::init() {
    // Initialization order mirrors dependencies: create window/context → init view/UI → load level data.
    if (!initWindow()) {
        return false;
    }

    view_.init(windowWidth_, windowHeight_);
    view_.setGameSceneVisible(false);
    view_.setStartCallback([this]() {
        if (!gameStarted_) {
            gameStarted_ = true;
            view_.setGameSceneVisible(true);
        }
    });

    if (!viewModel_.loadDefaultLevel()) {
        std::cerr << "Failed to load default level. Game will run without logic." << std::endl;
    }

    lastFrameTime_ = glfwGetTime();
    return true;
}

bool GameApplication::initWindow() {
    // Encapsulate GLFW/GLAD setup so init() stays focused on higher-level configuration.
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(windowWidth_, windowHeight_, "Portal Parabox", nullptr, nullptr);
    if (!window_) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    glfwSetWindowUserPointer(window_, this);
    glfwSetCursorPosCallback(window_, CursorPosCallback);
    glfwSetMouseButtonCallback(window_, MouseButtonCallback);
    glfwSwapInterval(1);

    glViewport(0, 0, windowWidth_, windowHeight_);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    return true;
}

void GameApplication::run() {
    // Canonical game loop: process input → update model → render view until window closes.
    while (!glfwWindowShouldClose(window_)) {
        double currentTime = glfwGetTime();
        double deltaTime = currentTime - lastFrameTime_;
        lastFrameTime_ = currentTime;
        (void)deltaTime;

        processInput();
        update();
        render();

        glfwSwapBuffers(window_);
        glfwPollEvents();
    }
}

void GameApplication::processInput() {
    // Gather discrete input once per frame (with cooldown) to keep movement grid-aligned like Sokoban.
    if (!window_) {
        return;
    }

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window_, true);
        return;
    }

    if (!viewModel_.hasGame()) {
        return;
    }

    if (!gameStarted_) {
        return;
    }

    double now = glfwGetTime();

    // 动画期间：允许相机旋转；移动操作在末段进行缓存
    auto isNearMoveEnd = [&]() -> bool {
        if (!animatingMove_) return false;
        double u = (now - moveAnimStart_) / moveAnimDuration_;
        return u >= 0.5; // 末段判定阈值：50% 以后
    };

    Input input = UP;
    int viewRotate = GLFW_KEY_U;
    bool hasInput = false;
    bool operateAction = false;

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_UP) == GLFW_PRESS) {
        input = UP;
        hasInput = true;
        operateAction = true;
    } else if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_DOWN) == GLFW_PRESS) {
        input = DOWN;
        hasInput = true;
        operateAction = true;
    } else if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_LEFT) == GLFW_PRESS) {
        input = LEFT;
        hasInput = true;
        operateAction = true;
    } else if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window_, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        input = RIGHT;
        hasInput = true;
        operateAction = true;
    }

    if (glfwGetKey(window_, GLFW_KEY_U) == GLFW_PRESS) {
        viewRotate = GLFW_KEY_U;
        hasInput = true;
    }
    else if (glfwGetKey(window_, GLFW_KEY_I) == GLFW_PRESS) {
        viewRotate = GLFW_KEY_I;
        hasInput = true;
    }

    if (!hasInput) {
        return;
    }

    // 新增：相机旋转期间锁定所有游戏输入（包括继续旋转与移动），防止角度漂移与输入抖动
    if (view_.isCameraRotating()) {
        return;
    }

    if (operateAction) {
        // 若处于动画末段，缓存该输入，等待动画结束后执行
        if (animatingMove_ && isNearMoveEnd()) {
            pendingInput_ = view_.remapInputForCamera(input);
            hasPendingInput_ = true;
            return;
        }

        // 非动画或非末段：按冷却判断是否可执行
        if (animatingMove_) {
            // 动画进行中，且非末段：忽略该移动输入
            return;
        }

        if (now - lastInputTime_ < inputCooldown_) {
            return;
        }

        // 以相机为参考系重映射输入
        input = view_.remapInputForCamera(input);

        // 应用输入前缓存“前状态”
        GameState prev = viewModel_.getState();

        // 执行一次操作
        viewModel_.handleInput(input);

        // 输入后读取“目标状态”
        GameState next = viewModel_.getState();

        // 缓存状态用于渲染插值
        cachedState_ = prev;
        cachedNextState_ = next;
        hasCachedState_ = true;

        // 启动动画
        animatingMove_ = true;
        moveAnimStart_ = now;
        view_.beginMoveAnimation(static_cast<float>(moveAnimDuration_));

        lastInputTime_ = now;
    } else {
        bool wasRotating = view_.isCameraRotating();
        view_.handleKey(viewRotate);
        bool nowRotating = view_.isCameraRotating();
        if (!wasRotating && nowRotating) {
            lastInputTime_ = now;
        }
    }
}

void GameApplication::update() {
    // Maintain bookkeeping flags (win announcements) separate from rendering to keep run loop tidy.
    if (!gameStarted_) {
        winAnnounced_ = false;
        return;
    }

    // 仅维护胜利状态标志
    viewModel_.update();

    if (viewModel_.hasGame()) {
        bool isWin = viewModel_.isWin();
        if (isWin && !winAnnounced_) {
            std::cout << "You won the level!" << std::endl;
            winAnnounced_ = true;
        } else if (!isWin && winAnnounced_) {
            winAnnounced_ = false;
        }
    } else {
        winAnnounced_ = false;
    }
}

void GameApplication::render() {
    // Render path toggles automatically based on gameStarted_/hasGame flags, keeping UI fallback intact.
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!gameStarted_) {
        view_.render(nullptr, nullptr);
        return;
    }

    const Level* level = viewModel_.getLevel();
    if (viewModel_.hasGame() && level) {
        if (!animatingMove_) {
            // 动画未进行：正常刷新
            cachedState_ = viewModel_.getState();
            cachedNextState_ = viewModel_.getNextState();
            hasCachedState_ = true;
        } else {
            // 动画期间：检查结束
            double now = glfwGetTime();
            if (now - moveAnimStart_ >= moveAnimDuration_) {
                animatingMove_ = false;

                // 若存在缓存输入，则立即执行并触发下一段动画
                if (hasPendingInput_) {
                    hasPendingInput_ = false;

                    GameState prev = viewModel_.getState();
                    // 执行缓存输入
                    viewModel_.handleInput(pendingInput_);
                    GameState next = viewModel_.getState();

                    cachedState_ = prev;
                    cachedNextState_ = next;
                    hasCachedState_ = true;

                    // 立即启动下一段动画
                    animatingMove_ = true;
                    moveAnimStart_ = now;
                    view_.beginMoveAnimation(static_cast<float>(moveAnimDuration_));
                    lastInputTime_ = now;
                } else {
                    // 无缓存输入：刷新到最新状态
                    cachedState_ = viewModel_.getState();
                    cachedNextState_ = viewModel_.getNextState();
                    hasCachedState_ = true;
                }
            }
        }

        if (hasCachedState_) {
            view_.render(&cachedState_, level, &cachedNextState_);
        } else {
            view_.render(nullptr, nullptr);
        }
    } else {
        hasCachedState_ = false;
        view_.render(nullptr, nullptr);
    }
}

void GameApplication::onMouseMove(double xpos, double ypos) {
    view_.handleMouseMove(static_cast<float>(xpos), static_cast<float>(ypos));
}

void GameApplication::onMouseButton(int button, int action, double xpos, double ypos) {
    view_.handleMouseButton(button, action, static_cast<float>(xpos), static_cast<float>(ypos));
}

void GameApplication::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    if (auto* app = static_cast<GameApplication*>(glfwGetWindowUserPointer(window))) {
        app->onMouseMove(xpos, ypos);
    }
}

void GameApplication::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    double xpos = 0.0;
    double ypos = 0.0;
    glfwGetCursorPos(window, &xpos, &ypos);
    if (auto* app = static_cast<GameApplication*>(glfwGetWindowUserPointer(window))) {
        app->onMouseButton(button, action, xpos, ypos);
    }
}

int main() {
    GameApplication app(800, 600);
    if (!app.init()) {
        return -1;
    }

    app.run();
    return 0;
}
