#include "camera.h"

void Camera::processKeyboard(CameraMovement direction, float deltaTime){
    float velocity = movementSpeed * deltaTime;
    switch (direction){
        case CameraMovement::FORWARD:
            position += front * velocity;
            break;
        case CameraMovement::BACKWARD:
            position -= front * velocity;
            break;
        case CameraMovement::RIGHT:
            position += right * velocity;
            break;
        case CameraMovement::LEFT:
            position -= right * velocity;
            break;
        case CameraMovement::UP:
            position += up * velocity;
            break;
        case CameraMovement::DOWN:
            position -= up * velocity;
            break;
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset, bool constrainPitch = true){
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;
    yaw += xOffset;
    pitch += yOffset;
    // constrain pitch
    if (constrainPitch){
        pitch = std::clamp(pitch, -89.0f,89.0f);
    }
    updateCameraVectors();
}

void Camera::updateCameraVectors(){
    // caculate new front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
    // caculate new up & right
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(front, right));
}

glm::mat4 Camera::getViewMatrix() const{
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::getProjectionMatrix(float aspectRatio, float nearPlane = 0.1f, float farPlane = 100.0f) const{
    return glm::perspective(glm::radians(zoom), aspectRatio, nearPlane, farPlane);
}



void processInput(GLFWwindow* window, Camera& camera, float deltaTime){
    // wasd movement following standard fps scheme conventions
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::LEFT,deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
    // vertical movement
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::DOWN, deltaTime);
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos){
    static bool firstMouse = true;      // initial mouse position  flag
    static float lastX = 0.0f, lastY = 0.0f;
    if (firstMouse){
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;       //屏幕空间（左上角为原点）中y增大是向下，相机的y增大是向上

    lastX = xpos;
    lastY = ypos;
    camera.processMouseMovement(xoffset, yoffset);
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset){
    camera.processMouseScroll(yoffset);
}

void setupInputCallbacks(GLFWwindow* window){
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);    // mouse capture mode
}


// ThirdPersonCamera::
void ThirdPersonCamera::updataPosition(const glm::vec3& targetPos, const glm::vec3& targetFwd, float deltaTime ){
    targetPosition = targetPos;
    targetForward = glm::normalize(targetFwd);
    
    // caculate
    glm::vec3 offset = -targetForward * followDistance;
    offset.y = followHeight;

    desiredPosition = offset + targetPosition;

    position = glm::mix(position, desiredPosition, 1.0f - pow(followSmoothness, deltaTime * 60.0f));
    front = glm::normalize (targetPosition - position);     // public继承自 camera, 需要通过继承类的对象访问
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));

}

void ThirdPersonCamera::handleOcclusion(const Scene& scene){
    Ray ray;
    ray.origin = targetPosition;
    ray.direction = glm::normalize(desiredPosition - targetPosition);
    // check for intersection
    RaycastHit hit;
    if (scene.raycast(ray, hit, glm::length(desiredPosition - targetPosition))){
        // if intersection, move the camera to the hit point
        float offsetDistance = 0.2f;    // small offset to avoid clipping
        position = hit.point - (ray.direction * offsetDistance);

        float currentDistance = glm::length(position - targetPosition);
        if (currentDistance < minDistance){
            position = targetPosition + ray.direction * minDistance;
        }

        front = glm::normalize(targetPosition - position);
        right = glm::normalize(glm::cross( front, worldUp));
        up = glm::normalize(glm::cross(front, right));
    }
}

void ThirdPersonCamera::orbit(float horizontalAngle, float verticalAngle){
    yaw += horizontalAngle;
    pitch += verticalAngle;
    
    pitch = std::clamp(pitch, -89.0f, 89.0f);   // constrain pitch to avoid flipping

    // calculate new position
    float radius = followDistance;
    float yawRad = glm::radians(yaw);
    float pitchRad = glm::radians(pitch);
    
    // Convert spherical coordinates to Cartesian
    glm::vec3 offset;
    offset.x = radius * cos(yawRad) * cos(pitchRad);
    offset.y = radius *  sin(pitchRad);
    offset.z = radius * sin(yawRad) * cos(pitchRad);
    disiredPosition = targetPosition + offset;  // set disire position

    front = glm::normalize(targetPosition - disiredPosition);
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(front, right));
}
/**
 * How to USE thirdPersonCamera；
 * 👇
 */
// void gameLoop(float deltaTime){
//     // update charater position
//     charater.update(deltaTime);
//     // update camera position TO follow charater
//     thirdPersonCamera.updataPosition( charater.getPosition(), charater.getForward(), deltaTime );
//     thirdPersonCamera.handleOcclusion(scene);

//     if (mouseInoutDetected){
//         thirdPersonCamera.orbit(mouseDeltaX, mouseDeltaY);
//     }
//     // get view & projection matrix for rendering
//     glm::mat4 viewMatrix = thirdPersonCamera.getViewMatrix();
//     glm::mat4 projMatrix = thirdPersonCamera.getProjectionMatrix(aspectRatio);

//     // use these matrix for rendering
//     renderer.render(scene, viewMatrix, projMatrix);
// }