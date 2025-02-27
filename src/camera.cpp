#include "camera.hpp"
#include <imgui/imgui.h>

#define RELATIVE_MOUSE_MODE false

void Camera::displayUI() {

    ImGui::Begin("Camera Control");
    if (ImGui::SliderFloat("FOV", &_fovy, 10.f, 140.f, "%01.f"))
        _updateProjectionMatrix();

    if (ImGui::SliderFloat2("Near/Far", &_zNear, 0.01f, 100.f, "%.3f", ImGuiSliderFlags_Logarithmic))
        _updateProjectionMatrix();

    ImGui::SliderFloat("Speed", &_speed, 0.05f, 10.f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Sensitivity", &_sensitivity, 0.01f, 1.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Smoothness", &_smoothness, 0.01f, 1.0f, "%.2f");

    ImGui::Separator();
    vec3 position = getPosition();
    if (ImGui::InputFloat3("Position: ", &position[0], "%.1f"))
        _setAnimationGoal(position);
    if (ImGui::InputFloat2("Yaw/Pitch: ", &_yaw, "%.1f"))
        _updateVectors();

    // ImGui::Separator();
    // ImGui::Text("Front: (%.1f, %.1f, %.1f)", _invDir.x, _invDir.y, _invDir.z);
    // ImGui::Text("Right: (%.1f, %.1f, %.1f)", _right.x, _right.y, _right.z);
    // ImGui::Text("Up: (%.1f, %.1f, %.1f)", _up.x, _up.y, _up.z);

    ImGui::End();
}

void Camera::handleEvents() {
    // Don't handle events if ImGui is capturing the mouse
    if (ImGui::GetIO().WantCaptureMouse)
        return;

    /*if (RELATIVE_MOUSE_MODE) {

        // First relative mouse mode
        bool isButtonLeft = p_event.button.button == SDL_BUTTON_LEFT;
        if (p_event.type == SDL_MOUSEBUTTONDOWN && isButtonLeft)
            SDL_SetRelativeMouseMode(SDL_TRUE); // Capture the mouse and hide the cursor
        else if (p_event.type == SDL_MOUSEBUTTONUP && isButtonLeft)
            SDL_SetRelativeMouseMode(SDL_FALSE); // Release the mouse and show the cursor
    }*/

    // Then handle events
    ImGuiIO &io = ImGui::GetIO();

    if (ImGui::IsKeyDown(ImGuiKey_Z)) moveFront(1);
    if (ImGui::IsKeyDown(ImGuiKey_S)) moveFront(-1);
    if (ImGui::IsKeyDown(ImGuiKey_Q)) moveRight(-1);
    if (ImGui::IsKeyDown(ImGuiKey_D)) moveRight(1);
    if (ImGui::IsKeyDown(ImGuiKey_R)) moveUp(1);
    if (ImGui::IsKeyDown(ImGuiKey_F)) moveUp(-1);

    if (io.MouseWheel != 0.f)
        processMouseScroll(io.MouseWheel);

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        auto delta = io.MouseDelta;
        rotateRelative(delta.x, delta.y);
    }
}

void Camera::processMouseScroll(float yoffset) {
    _fovy = glm::clamp(_fovy - yoffset * 4, 1.f, 140.f);
    _updateProjectionMatrix();
}