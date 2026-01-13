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
    bool loadLevelByIndex(int index) {
        if (index < 0 || index >= levelList_.size()) return false;

        // 使用 probeAndLoad 确保能像 loadDefaultLevel 一样找到文件
        if (probeAndLoad(levelList_[index])) {
            currentLevelIndex_ = index;
            // 打印调试信息，确认当前加载的关卡
            std::cout << "Successfully loaded level index " << index << ": " << levelList_[index] << std::endl;
            return true;
        }
        std::cerr << "Failed to load level index " << index << ": " << levelList_[index] << std::endl;
        return false;
    }

    bool loadNextLevel() {
        // 尝试加载下一个索引的关卡
        return loadLevelByIndex(currentLevelIndex_ + 1);
    }

    bool isLastLevel() const {
        return currentLevelIndex_ >= levelList_.size() - 1;
    }
    /// Attempt to locate and load the default level file by probing several folders.
    bool loadDefaultLevel() {
        // 为了方便调试，我们默认从列表的第一个关卡开始加载
        // 这样 loadNextLevel 逻辑就是连贯的 (0 -> 1 -> 2 ...)
        // 如果想把 l2 作为起始关，可以调整 levelList_ 的顺序或者这里传 1
        if (loadLevelByIndex(0)) {
            return true;
        }
        
        // 如果列表加载失败，保留原有的 fallback 逻辑作为保险
        static const std::array<std::string, 2> relativeCandidates = {
            "levels/l2.json",
            "model/levels/l2.json"
        };
        for (const auto& rel : relativeCandidates) {
            if (probeAndLoad(rel)) {
                return true;
            }
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
        gameplay_->updateState();
        winState_ = gameplay_->getCurrState().is_win;
    }

    void update() {
        if (!gameplay_) {
            winState_ = false;
            return;
        }
        winState_ = gameplay_->getCurrState().is_win;
    }

    GameState getState() const {
        if (!gameplay_) {
            return GameState{};
        }
        return gameplay_->getCurrState();
    }

    GameState getNextState() const {
        if (!gameplay_) {
            return GameState{};
        }
        return gameplay_->getNextState();
    }

    const Level* getLevel() const {
        return gameplay_ ? &level_ : nullptr;
    }

    bool isWin() const {
        return winState_;
    }

private:
    // 将路径探测逻辑提取为通用函数
    bool probeAndLoad(const std::string& relativePath) {
        // 定义一个 lambda 来尝试从 base 加载
        auto tryLoadFromBase = [&](const std::filesystem::path& base) {
             // 1. 直接拼接
            std::filesystem::path candidate = base / relativePath;
            if (std::filesystem::exists(candidate)) {
                return loadLevel(candidate.string());
            }
            // 2. 尝试去掉前缀的 model/ 再次拼接 (应对 levelList 中写死路径的情况)
            // 例如 relativePath 是 "model/levels/l1.json"，但 base 已经是 project/model/ 了
            if (relativePath.rfind("model/", 0) == 0) { // starts with "model/"
                std::filesystem::path stripModel = base / relativePath.substr(6);
                if (std::filesystem::exists(stripModel)) {
                    return loadLevel(stripModel.string());
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
        std::filesystem::path projectRoot = headerDir.parent_path(); // viewmodel/.. -> root
        if (tryLoadFromBase(projectRoot)) {
            return true;
        }

        return false;
    }

    Level level_{};
    std::unique_ptr<GamePlay> gameplay_;
    bool winState_ = false;
    std::vector<std::string> levelList_ = {
        "model/levels/l1.json",
        "model/levels/l2.json",
        "model/levels/l3.json",
        "model/levels/l4.json",
        "model/levels/l5.json"
    };
    int currentLevelIndex_ = 0;
};

#endif // GAME_VIEW_MODEL_HPP
