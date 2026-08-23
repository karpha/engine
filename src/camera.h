#pragma once
// 相机有LookAt， up， position，
// 光线与球体的交集，连接光源点与球心，判断delta判别式，。。。。
// 光线与三角形是否相交：Möller–Trumbore algorithm
// 此处计算光线投射（光线与其他物体的交集）可以用于计算camera ray
// perspective orthographic 的计算过程使用glm函数，只需要调用、传入参数即可
// perspective更贴近现实，所以相关参数会有fov，近平面n， 和远平面f
// orthographic类似三视图的侧视图（平面设计图纸），所以会有上，下，左，右和n，f，

// 诸如代码    glm::mat4 model = glm::mat4(1.0f)，这里的model指的是将 物体的位置（包含所有顶点坐标） 进行MVP变换时使用的 M 变换矩阵，（1.0f）表示物体的位置不变
// view：就是指相机中看到的位置，相机空间，对应矩阵V，可以使用 glm::lookAt(cameraPosition, cameraTarget, upVector);
// 透视投影：矩阵P， 可以使用函数glm::perspective(glm::radians(fovY), aspectRatio, nearPlane, farPlane)， 实际使用时只需要传入参数即可
// 正交投影与透视投影同理
/**
 * Ref: https://docs.vulkan.org/tutorial/latest/Building_a_Simple_Engine/Camera_Transformations/04_camera_implementation.html
 */

#include <glm/gtc/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <algorithm>

enum class CameraMovement{
    FORWARD,
    BACKWARD,
    UP,
    DOWN,
    LEFT,
    RIGHT
};

class Camera{
    void updateCameraVectors();
    public:
    Camera(glm::vec3 position = glm::vec3(0.0f,0.0f,0.0f),  // world origin
            glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),     // Y-axis as world up
            float yaw = -90.f,
            float pitch = 0.0 );
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f) const;

    // human interaction
    void processKeyboard(CameraMovement direction, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true);
    void processMouseScroll(float yOffset);

    glm::vec3 getPosition() const {
        return position;
    }
    glm::vec3 getFront() const {
        return front;
    }
    float getZoom() const {
        return zoom;
    }

    protected:  // 需要在继承类中使用的变量,需要在基类中声明为protected
// these form camera's local coordinate system in world space
    glm::vec3 position;     // camera's location in world space
    glm::vec3 front;        // camera's local front
    glm::vec3 up;       //camera's local up
    glm::vec3 right;        //camera's local right
    glm::vec3 worldUp;      // global up vector(typically Y-axis)

    float yaw, pitch;
    private:
    // user interaction
    float movementSpeed;
    float mouseSensitivity;
    float zoom;     // field-of-view of perspective projection
};

class ThirdPersonCamera : public Camera{
    private:
    glm::vec3 targetPosition;   // current world position of target character
    glm::vec3 targetForward;    // target forward direction

    float followDistance;
    float followHeight;
    float followSmoothness;

    float minDistance;
    float raycastDiatance;

    glm::vec3 desiredPosition;
    glm::vec3 smoothDampVelocity;
    public:
    ThirdPersonCamera(float followDistance = 5.0f,
                    float followHeight = 2.0f,
                    float followSmoothness = 0.1f,
                    float minDistance = 1.0f
    );
    void updataPosition(const glm::vec3& targetPos, const glm::vec3& targetFwd, float deltaTime );
    void handleOcclusion(const Scene& scene);   // 碰撞
    void orbit(float horizontalAngle, float verticalAngle);     // 环绕相机

    void setFollowDistance(float distance){
        followDistance = distance;
    }
    void setFollowHeight(float height){
        followHeight = height;
    }
    void setFollowSmoothness(float smoothness){
        followSmoothness = smoothness;
    }
};