#ifndef GAMELIGHT_HPP
#define GAMELIGHT_HPP

#include <glm/glm.hpp>
#include <vector>
#include <string>

/// @brief 点光源结构体
struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
    float intensity;
    float radius;  // 光照半径
    
    PointLight(const glm::vec3& pos, const glm::vec3& col, float inten, float rad = 10.0f)
        : position(pos), color(col), intensity(inten), radius(rad) {}
};

/// @brief 方向光结构体（用于主光源）
struct DirectionalLight {
    glm::vec3 direction;
    glm::vec3 color;
    float intensity;
    
    DirectionalLight() = default;
    
    DirectionalLight(const glm::vec3& dir, const glm::vec3& col, float inten)
        : direction(glm::normalize(dir)), color(col), intensity(inten) {}
};

/// @brief PBR材质参数
struct PBRMaterial {
    glm::vec3 albedo;      // 基础颜色
    float metallic;        // 金属度 [0,1]
    float roughness;       // 粗糙度 [0,1]
    float ao;              // 环境光遮蔽 [0,1]
    
    PBRMaterial(const glm::vec3& alb = glm::vec3(1.0f), 
                float metal = 0.0f, 
                float rough = 0.5f, 
                float ambOcc = 1.0f)
        : albedo(alb), metallic(metal), roughness(rough), ao(ambOcc) {}
};

/// @brief 实验室光照管理器
class LabLightingSystem {
public:
    LabLightingSystem() {
        setupDefaultLighting();
    }
    
    /// @brief 设置默认实验室光照（天花板点光源+环境光）
    void setupDefaultLighting() {
        // 顶部暖白日光（模拟强烈阳光，色温约5500K）
        mainLight_ = DirectionalLight(
            glm::vec3(-0.5f, -1.0f, -0.4f),
            glm::vec3(1.0f, 0.95f, 0.88f),  // 暖白光
            2.0f                            // 降低强度，避免过曝
        );
        
        // 环境光改为深冷蓝（模拟天空散射），与主光形成冷暖对比
        ambientColor_ = glm::vec3(0.2f, 0.25f, 0.4f);
        ambientIntensity_ = 0.1f;          // 极低环境光，接近纯黑阴影
        
        pointLights_.clear();
        invalidateCache();
    }
    
    /// @brief 设置天花板灯光阵列（网格布局）
    /// @param roomSize 房间尺寸（格子数）
    /// @param ceilingHeight 天花板高度
    /// @param tileSize 单个格子世界尺寸
    void setupCeilingLights(int roomSize, float ceilingHeight, float tileSize = 1.0f) {
        if (roomSize == cachedRoomSize_ &&
            ceilingHeight == cachedCeilingHeight_ &&
            tileSize == cachedTileSize_) {
            return; // 参数未变化，沿用已有灯光
        }

        cachedRoomSize_ = roomSize;
        cachedCeilingHeight_ = ceilingHeight;
        cachedTileSize_ = tileSize;

        pointLights_.clear();
        
        // 实验室高亮冷白灯（色温约6500K）
        const glm::vec3 labLightColor(0.92f, 0.96f, 1.0f);
        const float lightIntensity = 1.0f;    // 关闭点光源干扰，确保阴影纯净
        const float lightRadius = 12.0f;      // 增加覆盖范围
        
        // 计算房间中心和半径
        float boardHalf = roomSize * tileSize * 0.5f;
        
        // 网格布局：每2x2格子放置一盏灯（可调整密度）
        int lightSpacing = 2;  // 灯间距（格子数）
        int lightsPerRow = (roomSize / lightSpacing);
        if (lightsPerRow < 2) lightsPerRow = 2;  // 至少2x2阵列
        
        float actualSpacing = (roomSize * tileSize) / static_cast<float>(lightsPerRow);
        float startOffset = -boardHalf + actualSpacing * 0.5f;
        
        // 生成网格点光源（限制在4个内）
        int maxLights = 4;
        int lightCount = 0;
        
        for (int i = 0; i < lightsPerRow && lightCount < maxLights; ++i) {
            for (int j = 0; j < lightsPerRow && lightCount < maxLights; ++j) {
                float x = startOffset + i * actualSpacing;
                float z = startOffset + j * actualSpacing;
                
                // 灯光位置：略低于天花板（模拟嵌入式灯）
                glm::vec3 lightPos(x, ceilingHeight - 0.3f, z);
                
                pointLights_.emplace_back(lightPos, labLightColor, lightIntensity, lightRadius);
                lightCount++;
            }
        }
    }
    
    /// @brief 添加点光源（可用于特殊效果）
    void addPointLight(const PointLight& light) {
        if (pointLights_.size() < 4) {  // 限制最多4个点光源
            pointLights_.push_back(light);
        }
    }
    
    /// @brief 清空所有点光源
    void clearPointLights() {
        pointLights_.clear();
        invalidateCache();
    }
    
    /// @brief 获取主光源
    const DirectionalLight& getMainLight() const { return mainLight_; }
    
    /// @brief 获取环境光
    glm::vec3 getAmbientLight() const { return ambientColor_ * ambientIntensity_; }
    
    /// @brief 获取所有点光源
    const std::vector<PointLight>& getPointLights() const { return pointLights_; }
    
    /// @brief 创建预设材质
    static PBRMaterial createMaterial(const std::string& type, const glm::vec3& baseColor) {
        if (type == "plastic") {
            // 塑料：低金属度，中等粗糙度
            return PBRMaterial(baseColor, 0.0f, 0.5f, 1.0f);
        } else if (type == "metal") {
            // 金属：高金属度，低粗糙度
            return PBRMaterial(baseColor, 0.9f, 0.2f, 1.0f);
        } else if (type == "ceramic") {
            // 陶瓷：低金属度，低粗糙度
            return PBRMaterial(baseColor, 0.0f, 0.3f, 1.0f);
        } else if (type == "matte") {
            // 哑光：低金属度，高粗糙度
            return PBRMaterial(baseColor, 0.0f, 0.9f, 1.0f);
        }
        // 默认塑料材质
        return PBRMaterial(baseColor, 0.0f, 0.5f, 1.0f);
    }
    
private:
    DirectionalLight mainLight_;
    glm::vec3 ambientColor_;
    float ambientIntensity_;
    std::vector<PointLight> pointLights_;

    int cachedRoomSize_ = -1;
    float cachedCeilingHeight_ = -1.0f;
    float cachedTileSize_ = -1.0f;

    void invalidateCache() {
        cachedRoomSize_ = -1;
        cachedCeilingHeight_ = -1.0f;
        cachedTileSize_ = -1.0f;
    }
};

#endif // GAMELIGHT_HPP
