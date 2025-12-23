#ifndef GAME_VIEW_MODEL_HPP
#define GAME_VIEW_MODEL_HPP

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

#include "model/include/gameplay.hpp"
#include "model/include/level_loader.hpp"

// GameViewModel coordinates model state (GamePlay) for the view layer.
/// Bridge between the view layer and the core gameplay logic. Responsible for
/// loading levels, forwarding inputs, and exposing lightweight read-only state.
class GameViewModel {
public:
    /// Attempt to locate and load the default level file by probing several folders.
    bool loadDefaultLevel() {
        static const std::array<std::string, 2> relativeCandidates = {
            "levels/l1.json",
            "model/levels/l1.json"
        };

        auto tryLoadFromBase = [&](const std::filesystem::path& base) {
            for (const auto& rel : relativeCandidates) {
                std::filesystem::path candidate = base / rel;
                if (std::filesystem::exists(candidate)) {
                    return loadLevel(candidate.string());
                }
            }
            return false;
        };

        std::filesystem::path current = std::filesystem::current_path();
        while (true) {
            if (tryLoadFromBase(current)) {
                return true;
            }
            if (!current.has_parent_path()) {
                break;
            }
            auto parent = current.parent_path();
            if (parent == current) {
                break;
            }
            current = parent;
        }

        std::filesystem::path headerDir = std::filesystem::path(__FILE__).parent_path();
        std::filesystem::path projectRoot = headerDir.parent_path();
        if (tryLoadFromBase(projectRoot)) {
            return true;
        }

        return false;
    }

    /// Load the level located at 'path' and construct the underlying GamePlay instance.
    bool loadLevel(const std::string& path) {
        try {
            level_ = LevelLoader::loadLevel(path);
            gameplay_ = std::make_unique<GamePlay>(level_);
            winState_ = gameplay_->getCurrState().is_win;
            return true;
        } catch (const std::exception& ex) {
            std::cerr << "Error loading level " << path << ": " << ex.what() << std::endl;
            gameplay_.reset();
            return false;
        }
    }

    bool hasGame() const {
        return gameplay_ != nullptr;
    }

    void handleInput(Input input) {
        std::cout << "Handling input: " << input << std::endl;
        if (!gameplay_) {
            return;
        }
        gameplay_->operate(input);
        // When operate starts a continuous move, do not force commit here.
        // Caller should rely on tick() to advance and commit.
        // For tools/tests that expect immediate commit, updateState can be called explicitly.
        winState_ = gameplay_->getCurrState().is_win;
    }

    void update() {
        if (!gameplay_) {
            winState_ = false;
            return;
        }
        winState_ = gameplay_->getCurrState().is_win;
    }

    // Advance model simulation by dt seconds (for continuous moves)
    void tick(double dt) {
        if (gameplay_) gameplay_->tick(dt);
    }

    // Query model move duration so view/renderer can sync visuals
    double getMoveDuration() const {
        return gameplay_ ? gameplay_->getMoveDuration() : 0.0;
    }

    GameState getState() const {
        if (!gameplay_) {
            return GameState{};
        }
        // If the gameplay is currently performing a continuous move, expose
        // the nextState so the renderer can detect the target tile and start
        // visual interpolation. Otherwise return the committed current state.
        if (gameplay_->isMoving()) {
            return gameplay_->getNextState();
        }
        return gameplay_->getCurrState();
    }

    const Level* getLevel() const {
        return gameplay_ ? &level_ : nullptr;
    }

    bool isWin() const {
        return winState_;
    }

private:
    Level level_{};
    std::unique_ptr<GamePlay> gameplay_;
    bool winState_ = false;
};

#endif // GAME_VIEW_MODEL_HPP
