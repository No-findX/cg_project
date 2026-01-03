#version 330 core

// 基础 PBR 顶点着色器：
// 1) 接收网格的顶点属性（位置/法线/颜色/纹理坐标）
// 2) 将顶点变换到世界空间与裁剪空间
// 3) 计算用于阴影映射的光源裁剪坐标（FragPosLightSpace）

// 顶点属性输入：与 GameRenderer 设置的 layout(location) 保持一致
layout(location = 0) in vec3 aPos;       // 模型空间顶点位置
layout(location = 1) in vec3 aNormal;    // 模型空间法线
layout(location = 2) in vec3 aColor;     // 顶点颜色（可用于基础色调或调试）
layout(location = 3) in vec2 aTexCoord;  // 纹理坐标

// 传递给片元着色器的插值数据
out vec3 FragPos;            // 世界空间位置
out vec3 Normal;             // 世界空间法线
out vec3 VertexColor;        // 顶点颜色
out vec2 TexCoord;           // 纹理坐标
out vec4 FragPosLightSpace;  // 光源裁剪空间坐标，用于阴影贴图采样

// 变换矩阵 Uniforms
uniform mat4 model;             // 模型矩阵：模型空间 -> 世界空间
uniform mat4 view;              // 视图矩阵：世界空间 -> 观察空间
uniform mat4 projection;        // 投影矩阵：观察空间 -> 裁剪空间
uniform mat4 lightSpaceMatrix;  // 光源的投影-视图矩阵：世界空间 -> 光源裁剪空间（阴影）

void main() {
    // 1) 将顶点位置从模型空间变换到世界空间
    FragPos = vec3(model * vec4(aPos, 1.0));
    
    // 2) 计算世界空间法线
    //    使用法线矩阵 mat3(transpose(inverse(model))) 以正确处理非均匀缩放
    Normal = mat3(transpose(inverse(model))) * aNormal;
    
    // 3) 传递颜色与纹理坐标供片元着色器使用
    VertexColor = aColor;
    TexCoord = aTexCoord;
    
    // 4) 计算光源裁剪空间坐标，用于阴影贴图采样
    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);
    
    // 5) 计算最终裁剪空间位置，供光栅化阶段使用
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
