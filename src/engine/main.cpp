#include "engine.hpp"
#include <openvr.h>
#include <string>
#include <mutex>
#include <cstring>
#include <thread>
#include <chrono>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")
#endif
#ifdef _WIN32
#include <vector>
#endif
#if defined(_WIN32) && !defined(__MINGW32__)
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.Interop.h>
#include <winrt/base.h>
#endif

static vr::IVRSystem* g_vr_system = nullptr;
static std::mutex g_vr_mutex;
static std::atomic<bool> g_laser_enabled(true);

namespace corevr {

bool initialize_openvr(std::string &out_error) {
    std::lock_guard<std::mutex> lock(g_vr_mutex);
    if (g_vr_system) {
        return true; // ya inicializado
    }

    vr::EVRInitError eError = vr::VRInitError_None;
    g_vr_system = vr::VR_Init(&eError, vr::VRApplication_Overlay);

    if (eError != vr::VRInitError_None) {
        g_vr_system = nullptr;
        out_error = vr::VR_GetVRInitErrorAsEnglishDescription(eError);
        return false;
    }

    return true;
}

void shutdown_openvr() {
    std::lock_guard<std::mutex> lock(g_vr_mutex);
    if (g_vr_system) {
        vr::VR_Shutdown();
        g_vr_system = nullptr;
    }
}

OverlayManager::OverlayManager()
    : overlay_handle_(vr::k_ulOverlayHandleInvalid), laser_handle_(vr::k_ulOverlayHandleInvalid), created_(false) {
#ifdef _WIN32
    d3d_device_ = nullptr;
    d3d_context_ = nullptr;
    d3d_texture_ = nullptr;
    tex_width_ = 512;
    tex_height_ = 512;
    target_hwnd_ = nullptr;
#endif
    grabbed_ = false;
    grab_device_index_ = vr::k_unTrackedDeviceIndexInvalid;
    prev_grip_left_ = false;
    prev_grip_right_ = false;
    last_app_time_left_ = std::chrono::steady_clock::time_point();
    last_app_time_right_ = std::chrono::steady_clock::time_point();
    double_tap_left_ = false;
    double_tap_right_ = false;
    prev_app_left_ = false;
    prev_app_right_ = false;
    // identity last_abs_transform_
    std::memset(&last_abs_transform_, 0, sizeof(last_abs_transform_));
    last_abs_transform_.m[0][0] = 1.0f; last_abs_transform_.m[1][1] = 1.0f; last_abs_transform_.m[2][2] = 1.0f;
    locked_ = false;
}

OverlayManager::~OverlayManager() {
    if (created_) {
        vr::VROverlay()->HideOverlay(overlay_handle_);
        vr::VROverlay()->DestroyOverlay(overlay_handle_);
    }
    if (laser_handle_ != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(laser_handle_);
        vr::VROverlay()->DestroyOverlay(laser_handle_);
    }
#ifdef _WIN32
    if (d3d_texture_) {
        reinterpret_cast<ID3D11Texture2D*>(d3d_texture_)->Release();
        d3d_texture_ = nullptr;
    }
    if (d3d_context_) {
        reinterpret_cast<ID3D11DeviceContext*>(d3d_context_)->Release();
        d3d_context_ = nullptr;
    }
    if (d3d_device_) {
        reinterpret_cast<ID3D11Device*>(d3d_device_)->Release();
        d3d_device_ = nullptr;
    }
#endif
}

bool OverlayManager::create_overlay(const std::string &key, const std::string &name, std::string &out_error) {
    std::lock_guard<std::mutex> lock(g_vr_mutex);
    if (!g_vr_system) {
        if (!initialize_openvr(out_error)) return false;
    }

    vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    vr::EVROverlayError err = vr::VROverlay()->CreateOverlay(key.c_str(), name.c_str(), &handle);
    if (err != vr::VROverlayError_None) {
        out_error = "CreateOverlay failed (error code: " + std::to_string((int)err) + ")";
        return false;
    }

    overlay_handle_ = handle;
    created_ = true;

    // Default size in meters
    vr::VROverlay()->SetOverlayWidthInMeters(overlay_handle_, 1.0f);

    // Intentamos inicializar D3D si estamos en Windows
#ifdef _WIN32
    std::string derr;
    init_d3d(derr); // no fatal si falla, pero intentamos
#endif

    return true;
}

bool OverlayManager::set_overlay_position(float x, float y, float z, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }

    vr::HmdMatrix34_t mat;
    // Identity rotation
    std::memset(&mat, 0, sizeof(mat));
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[0][3] = x;
    mat.m[1][3] = y;
    mat.m[2][3] = z;

    vr::EVROverlayError err = vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle_, vr::TrackingUniverseStanding, &mat);
    if (err != vr::VROverlayError_None) {
        out_error = "SetOverlayTransformAbsolute failed (code: " + std::to_string((int)err) + ")";
        return false;
    }

    // cache last absolute transform
    last_abs_transform_ = mat;

    return true;
}

bool OverlayManager::show_overlay(std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    vr::EVROverlayError err = vr::VROverlay()->ShowOverlay(overlay_handle_);
    if (err != vr::VROverlayError_None) {
        out_error = "ShowOverlay failed (code: " + std::to_string((int)err) + ")";
        return false;
    }
    return true;
}

bool OverlayManager::hide_overlay(std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    vr::EVROverlayError err = vr::VROverlay()->HideOverlay(overlay_handle_);
    if (err != vr::VROverlayError_None) {
        out_error = "HideOverlay failed (code: " + std::to_string((int)err) + ")";
        return false;
    }
    return true;
}

bool OverlayManager::is_valid() const { return created_; }

#ifdef _WIN32
bool OverlayManager::init_d3d(std::string &out_error) {
    if (d3d_device_ && d3d_context_) return true;

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                   D3D11_SDK_VERSION, &device, &featureLevel, &context);
    if (FAILED(hr)) {
        out_error = "D3D11CreateDevice failed";
        return false;
    }

    d3d_device_ = device;
    d3d_context_ = context;

    // Crear textura inicial
    D3D11_TEXTURE2D_DESC desc;
    ZeroMemory(&desc, sizeof(desc));
    desc.Width = tex_width_;
    desc.Height = tex_height_;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ID3D11Texture2D* tex = nullptr;
    hr = device->CreateTexture2D(&desc, nullptr, &tex);
    if (FAILED(hr) || !tex) {
        out_error = "CreateTexture2D failed";
        // release device/context
        context->Release();
        device->Release();
        d3d_device_ = nullptr; d3d_context_ = nullptr;
        return false;
    }

    d3d_texture_ = tex;
    return true;
}

bool OverlayManager::render_test_texture(std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    if (!d3d_device_ || !d3d_context_) {
        if (!init_d3d(out_error)) return false;
    }

    ID3D11Device* device = reinterpret_cast<ID3D11Device*>(d3d_device_);
    ID3D11DeviceContext* context = reinterpret_cast<ID3D11DeviceContext*>(d3d_context_);
    ID3D11Texture2D* tex = reinterpret_cast<ID3D11Texture2D*>(d3d_texture_);

    // If a target window is set, capture it to the texture; otherwise generate gradient
    const int w = tex_width_;
    const int h = tex_height_;

    if (target_hwnd_) {
        HWND hwnd = reinterpret_cast<HWND>(target_hwnd_);
        HDC hdcWindow = GetDC(hwnd);
        if (!hdcWindow) {
            out_error = "GetDC failed for target window";
            return false;
        }
        HDC hdcMem = CreateCompatibleDC(hdcWindow);
        HBITMAP hbm = CreateCompatibleBitmap(hdcWindow, w, h);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hbm);
        // PrintWindow is better for capturing window content; fallback to BitBlt
        if (!PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT)) {
            BitBlt(hdcMem, 0, 0, w, h, hdcWindow, 0, 0, SRCCOPY);
        }

        BITMAPINFO bmi;
        ZeroMemory(&bmi, sizeof(bmi));
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> data(w * h * 4);
        if (!GetDIBits(hdcMem, hbm, 0, h, data.data(), &bmi, DIB_RGB_COLORS)) {
            out_error = "GetDIBits failed";
            SelectObject(hdcMem, oldBmp);
            DeleteObject(hbm);
            DeleteDC(hdcMem);
            ReleaseDC(hwnd, hdcWindow);
            return false;
        }

        SelectObject(hdcMem, oldBmp);
        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWindow);

        D3D11_BOX box;
        box.left = 0; box.top = 0; box.front = 0;
        box.right = w; box.bottom = h; box.back = 1;
        context->UpdateSubresource(tex, 0, &box, data.data(), w * 4, 0);
    } else {
        // Generate simple gradient RGBA image
        std::vector<uint8_t> data(w * h * 4);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int i = (y * w + x) * 4;
                data[i + 0] = (uint8_t)((x * 255) / w); // R gradient
                data[i + 1] = (uint8_t)((y * 255) / h); // G gradient
                data[i + 2] = (uint8_t)(128); // B constant
                data[i + 3] = 255; // A
            }
        }
        D3D11_BOX box;
        box.left = 0; box.top = 0; box.front = 0;
        box.right = w; box.bottom = h; box.back = 1;
        context->UpdateSubresource(tex, 0, &box, data.data(), w * 4, 0);
    }

    // Set overlay texture
    vr::Texture_t texture = { (void*)tex, vr::TextureType_DirectX, vr::ColorSpace_Auto };
    vr::EVROverlayError err = vr::VROverlay()->SetOverlayTexture(overlay_handle_, &texture);
    if (err != vr::VROverlayError_None) {
        out_error = "SetOverlayTexture failed (code: " + std::to_string((int)err) + ")";
        return false;
    }

    return true;
}
#else
bool OverlayManager::init_d3d(std::string &out_error) {
    out_error = "DirectX is only supported on Windows";
    return false;
}

bool OverlayManager::render_test_texture(std::string &out_error) {
    out_error = "DirectX rendering not supported on this platform";
    return false;
}
#endif

#ifdef _WIN32
struct OverlayManagerEnumData { const std::string* target; HWND found; };
static BOOL CALLBACK OverlayManager_EnumWindowsCallback(HWND h, LPARAM lParam) {
    OverlayManagerEnumData* data = reinterpret_cast<OverlayManagerEnumData*>(lParam);
    char buf[512];
    GetWindowTextA(h, buf, sizeof(buf));
    if (strstr(buf, data->target->c_str())) {
        data->found = h;
        return FALSE;
    }
    return TRUE;
}

bool OverlayManager::set_target_window_by_title(const std::string &title, std::string &out_error) {
    HWND hwnd = FindWindowA(nullptr, title.c_str());
    if (!hwnd) {
        OverlayManagerEnumData ed{&title, nullptr};
        EnumWindows(OverlayManager_EnumWindowsCallback, (LPARAM)&ed);
        if (!ed.found) { out_error = "Window not found by title"; return false; }
        hwnd = ed.found;
    }
    target_hwnd_ = reinterpret_cast<void*>(hwnd);
    return true;
}

bool OverlayManager::set_target_window_by_hwnd(uint64_t hwnd_value, std::string &out_error) {
    HWND hwnd = reinterpret_cast<HWND>(uintptr_t(hwnd_value));
    if (!IsWindow(hwnd)) { out_error = "Invalid HWND"; return false; }
    target_hwnd_ = reinterpret_cast<void*>(hwnd);
    return true;
}

bool OverlayManager::set_overlay_alpha(float alpha, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    vr::VROverlay()->SetOverlayAlpha(overlay_handle_, alpha);
    return true;
}

bool OverlayManager::set_overlay_curvature(float curvature, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    vr::VROverlay()->SetOverlayCurvature(overlay_handle_, curvature);
    return true;
}

bool OverlayManager::process_controller_uv(float u, float v, int mouse_event, std::string &out_error) {
    if (!target_hwnd_) { out_error = "No target HWND set"; return false; }
    HWND hwnd = reinterpret_cast<HWND>(target_hwnd_);
    RECT rect;
    if (!GetClientRect(hwnd, &rect)) { out_error = "GetClientRect failed"; return false; }
    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    int x = int(u * w);
    int y = int(v * h);

    LPARAM lparam = MAKELPARAM(x, y);
    UINT msg = WM_MOUSEMOVE;
    if (mouse_event == 1) msg = WM_LBUTTONDOWN;
    else if (mouse_event == 2) msg = WM_LBUTTONUP;

    if (!PostMessage(hwnd, msg, (WPARAM)MK_LBUTTON, lparam)) {
        out_error = "PostMessage failed";
        return false;
    }
    // trigger short haptic on mouse down
    if (mouse_event == 1) {
        // default to left hand
        std::string herr;
        trigger_haptic_feedback(vr::TrackedControllerRole_LeftHand, 0.02f, 150.0f, 0.5f, herr);
    }
    return true;
}
#endif

static vr::HmdMatrix34_t multiply34(const vr::HmdMatrix34_t &a, const vr::HmdMatrix34_t &b);

#ifdef _WIN32
bool OverlayManager::poll_controller_intersection(vr::ETrackedControllerRole controller_role, int &out_x, int &out_y, bool &out_is_trigger_down, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }

    uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(controller_role);
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) { out_error = "Controller role not tracked"; return false; }

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &pose = poses[deviceIndex];
    if (!pose.bPoseIsValid) { out_error = "Controller pose not valid"; return false; }

    vr::HmdVector3_t source;
    source.v[0] = pose.mDeviceToAbsoluteTracking.m[0][3];
    source.v[1] = pose.mDeviceToAbsoluteTracking.m[1][3];
    source.v[2] = pose.mDeviceToAbsoluteTracking.m[2][3];

    vr::HmdVector3_t direction;
    direction.v[0] = pose.mDeviceToAbsoluteTracking.m[0][2];
    direction.v[1] = pose.mDeviceToAbsoluteTracking.m[1][2];
    direction.v[2] = pose.mDeviceToAbsoluteTracking.m[2][2];

    vr::VROverlayIntersectionParams_t params;
    params.eOrigin = vr::TrackingUniverseStanding;
    params.vSource = source;
    params.vDirection = direction;

    vr::VROverlayIntersectionResults_t results;
    bool hit = vr::VROverlay()->ComputeOverlayIntersection(overlay_handle_, &params, &results);
    if (!hit) { return false; }

    float u = results.vUVs.v[0];
    float v = results.vUVs.v[1];
    out_x = int(u * tex_width_);
    out_y = int(v * tex_height_);

    // Controller button state (trigger)
    vr::VRControllerState_t state;
    if (g_vr_system->GetControllerState(deviceIndex, &state, sizeof(state))) {
        uint64_t mask = (1ULL << vr::k_EButton_SteamVR_Trigger);
        bool isDown = (state.ulButtonPressed & mask) != 0;
        out_is_trigger_down = isDown;

        // generate mouse events on edges
        bool *prev = (controller_role == vr::TrackedControllerRole_LeftHand) ? &prev_trigger_left_ : &prev_trigger_right_;
        if (isDown && !(*prev)) {
            // down
            process_controller_uv(u, v, 1, out_error);
        } else if (!isDown && *prev) {
            process_controller_uv(u, v, 2, out_error);
        } else {
            process_controller_uv(u, v, 0, out_error);
        }
        *prev = isDown;
    }

    // Application/Menu button (B/Y) double-tap detection
    if (g_vr_system->GetControllerState(deviceIndex, &state, sizeof(state))) {
        uint64_t appMask = (1ULL << vr::k_EButton_ApplicationMenu);
        bool appDown = (state.ulButtonPressed & appMask) != 0;
        bool isLeft = (controller_role == vr::TrackedControllerRole_LeftHand);
        auto now = std::chrono::steady_clock::now();
        if (isLeft) {
            if (appDown && !prev_app_left_) {
                if (last_app_time_left_ != std::chrono::steady_clock::time_point()) {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_app_time_left_).count();
                    if (diff <= 400) {
                        double_tap_left_ = true;
                        last_app_time_left_ = std::chrono::steady_clock::time_point();
                    } else {
                        last_app_time_left_ = now;
                    }
                } else {
                    last_app_time_left_ = now;
                }
            }
            prev_app_left_ = appDown;
        } else {
            if (appDown && !prev_app_right_) {
                if (last_app_time_right_ != std::chrono::steady_clock::time_point()) {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_app_time_right_).count();
                    if (diff <= 400) {
                        double_tap_right_ = true;
                        last_app_time_right_ = std::chrono::steady_clock::time_point();
                    } else {
                        last_app_time_right_ = now;
                    }
                } else {
                    last_app_time_right_ = now;
                }
            }
            prev_app_right_ = appDown;
        }
    }

    // Grip handling for grab/drag
    if (g_vr_system->GetControllerState(deviceIndex, &state, sizeof(state))) {
        uint64_t gripMask = (1ULL << vr::k_EButton_Grip);
        bool gripDown = (state.ulButtonPressed & gripMask) != 0;
        bool *prevGrip = (controller_role == vr::TrackedControllerRole_LeftHand) ? &prev_grip_left_ : &prev_grip_right_;
        if (gripDown && !(*prevGrip)) {
            std::string gerr;
            grab_overlay(controller_role, gerr);
        } else if (!gripDown && *prevGrip) {
            std::string rerr;
            release_overlay(rerr);
        }
        *prevGrip = gripDown;

        // If currently grabbed by this overlay, update position and allow thumbstick zoom
        if (grabbed_ && grab_device_index_ == deviceIndex) {
            // update overlay absolute = controller_pose * relative
            vr::TrackedDevicePose_t poses2[vr::k_unMaxTrackedDeviceCount];
            g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses2, vr::k_unMaxTrackedDeviceCount);
            const vr::TrackedDevicePose_t &cp = poses2[deviceIndex];
            if (cp.bPoseIsValid) {
                vr::HmdMatrix34_t ctrl = cp.mDeviceToAbsoluteTracking;
                // allow thumbstick Y to modify distance
                float axisY = 0.0f;
                if (state.unPacketNum && state.rAxis && state.rAxis[0].y) axisY = state.rAxis[0].y;
                if (fabs(axisY) > 0.05f) {
                    // move relative transform along local Z
                    grab_relative_transform_.m[2][3] += axisY * 0.01f; // small step
                }
                vr::HmdMatrix34_t newabs = multiply34(ctrl, grab_relative_transform_);
                vr::EVROverlayError seterr = vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle_, vr::TrackingUniverseStanding, &newabs);
                if (seterr == vr::VROverlayError_None) {
                    last_abs_transform_ = newabs;
                }
            }
        }
    }

    // Laser pointer overlay: create/update a thin quad attached to controller
    float laser_length = 2.0f; // meters
    if (laser_handle_ == vr::k_ulOverlayHandleInvalid) {
        vr::VROverlayHandle_t lh = vr::k_ulOverlayHandleInvalid;
        vr::EVROverlayError lerr = vr::VROverlay()->CreateOverlay("corevr.laser", "CoreVR Laser", &lh);
        if (lerr == vr::VROverlayError_None) {
            laser_handle_ = lh;
            vr::VROverlay()->SetOverlayWidthInMeters(laser_handle_, laser_length);
            vr::VROverlay()->ShowOverlay(laser_handle_);
        }
    }
    if (!g_laser_enabled.load()) {
        if (laser_handle_ != vr::k_ulOverlayHandleInvalid) {
            vr::VROverlay()->HideOverlay(laser_handle_);
        }
    }
    if (laser_handle_ != vr::k_ulOverlayHandleInvalid) {
        // place overlay relative to controller: centered forward by half length
        vr::HmdMatrix34_t mat;
        std::memset(&mat, 0, sizeof(mat));
        mat.m[0][0] = 1.0f; mat.m[1][1] = 1.0f; mat.m[2][2] = 1.0f;
        mat.m[0][3] = 0.0f; mat.m[1][3] = 0.0f; mat.m[2][3] = -laser_length / 2.0f;
        vr::EVROverlayError terr = vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(laser_handle_, deviceIndex, &mat);
        if (terr == vr::VROverlayError_None) {
            // color: neon cyan when trigger is down, otherwise soft blue
            if (out_is_trigger_down) {
                vr::VROverlay()->SetOverlayColor(laser_handle_, 0.0f, 1.0f, 1.0f);
            } else {
                vr::VROverlay()->SetOverlayColor(laser_handle_, 0.2f, 0.8f, 1.0f);
            }
            vr::VROverlay()->SetOverlayWidthInMeters(laser_handle_, laser_length);
            vr::VROverlay()->ShowOverlay(laser_handle_);
        }
    }

    return true;
}

bool OverlayManager::trigger_haptic_feedback(vr::ETrackedControllerRole role, float duration_seconds, float frequency, float amplitude, std::string &out_error) {
    if (!g_vr_system) { out_error = "OpenVR not initialized"; return false; }
    uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(role);
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) { out_error = "Controller role not tracked"; return false; }

    // Map amplitude to number of pulses and duration
    int total_ms = int(duration_seconds * 1000.0f);
    if (total_ms <= 0) total_ms = 20;
    int pulse_ms = 15; // microduration per pulse
    int pulses = total_ms / pulse_ms;
    for (int i = 0; i < pulses; ++i) {
        // TriggerHapticPulse takes microseconds
        unsigned short us = (unsigned short)(pulse_ms * 1000);
        // axis 0
        g_vr_system->TriggerHapticPulse(deviceIndex, 0, us);
        std::this_thread::sleep_for(std::chrono::milliseconds(pulse_ms));
    }
    return true;
}

bool OverlayManager::is_wrist_facing_user(vr::ETrackedControllerRole role, float threshold_degrees, bool &out_result, std::string &out_error) {
    out_result = false;
    if (!g_vr_system) { out_error = "OpenVR not initialized"; return false; }
    uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(role);
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) { out_error = "Controller role not tracked"; return false; }

    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &hmdPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    const vr::TrackedDevicePose_t &ctrlPose = poses[deviceIndex];
    if (!hmdPose.bPoseIsValid || !ctrlPose.bPoseIsValid) { out_error = "Pose invalid"; return false; }

    // compute HMD forward vector
    vr::HmdMatrix34_t hm = hmdPose.mDeviceToAbsoluteTracking;
    // forward is -Z axis
    float fx = -hm.m[2][0];
    float fy = -hm.m[2][1];
    float fz = -hm.m[2][2];

    // controller position
    vr::HmdMatrix34_t cm = ctrlPose.mDeviceToAbsoluteTracking;
    float cx = cm.m[0][3];
    float cy = cm.m[1][3];
    float cz = cm.m[2][3];

    float hx = hm.m[0][3];
    float hy = hm.m[1][3];
    float hz = hm.m[2][3];

    float vx = hx - cx;
    float vy = hy - cy;
    float vz = hz - cz;

    float dot = vx*fx + vy*fy + vz*fz;
    float vlen = sqrtf(vx*vx + vy*vy + vz*vz);
    float flen = sqrtf(fx*fx + fy*fy + fz*fz);
    if (vlen == 0 || flen == 0) { out_result = false; return true; }
    float cosang = dot / (vlen * flen);
    if (cosang > 1.0f) cosang = 1.0f;
    if (cosang < -1.0f) cosang = -1.0f;
    float ang = acosf(cosang) * 180.0f / 3.14159265f;
    out_result = (ang <= threshold_degrees);
    return true;
}

bool OverlayManager::attach_to_wrist(vr::ETrackedControllerRole hand_role, float offset_x, float offset_y, float offset_z, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }

    uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(hand_role);
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) { out_error = "Controller role not tracked"; return false; }

    vr::HmdMatrix34_t mat;
    std::memset(&mat, 0, sizeof(mat));
    mat.m[0][0] = 1.0f; mat.m[1][1] = 1.0f; mat.m[2][2] = 1.0f;
    mat.m[0][3] = offset_x; mat.m[1][3] = offset_y; mat.m[2][3] = offset_z;

    vr::EVROverlayError err = vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay_handle_, deviceIndex, &mat);
    if (err != vr::VROverlayError_None) { out_error = "SetOverlayTransformTrackedDeviceRelative failed"; return false; }
    // compute approximate absolute transform now and cache it
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &pose = poses[deviceIndex];
    if (pose.bPoseIsValid) {
        vr::HmdMatrix34_t dev = pose.mDeviceToAbsoluteTracking;
        // multiply dev * mat
        vr::HmdMatrix34_t absm;
        // rotation part
        for (int r=0;r<3;++r) for (int c=0;c<3;++c) {
            absm.m[r][c] = 0.0f;
            for (int k=0;k<3;++k) absm.m[r][c] += dev.m[r][k] * mat.m[k][c];
        }
        // translation
        for (int r=0;r<3;++r) {
            absm.m[r][3] = dev.m[r][0]*mat.m[0][3] + dev.m[r][1]*mat.m[1][3] + dev.m[r][2]*mat.m[2][3] + dev.m[r][3];
        }
        last_abs_transform_ = absm;
    }
    return true;
}

bool OverlayManager::start_wgc_capture_by_title(const std::string &title, std::string &out_error) {
    // Try to find window and initialize WGC (skeleton). If WinRT is not available, fallback to GDI capture.
    HWND hwnd = FindWindowA(nullptr, title.c_str());
    if (!hwnd) {
        // enumerate substring
        HWND found = nullptr;
        struct EnumData { const std::string* title; HWND *found; } ed{&title, &found};
        EnumWindows([](HWND h, LPARAM l)->BOOL {
            char buf[512]; GetWindowTextA(h, buf, sizeof(buf));
            EnumData* d = reinterpret_cast<EnumData*>(l);
            if (strstr(buf, d->title->c_str())) { *(d->found) = h; return FALSE; }
            return TRUE;
        }, (LPARAM)&ed);
        if (!found) { out_error = "Window not found"; return false; }
        hwnd = found;
    }
    target_hwnd_ = reinterpret_cast<void*>(hwnd);

#if defined(_WIN32) && !defined(__MINGW32__)
    // Try initialize WinRT / WGC
    try {
        winrt::init_apartment();
        auto item = winrt::Windows::Graphics::Capture::GraphicsCaptureItem::CreateForWindow(winrt::Windows::Foundation::IInspectable(winrt::attach_abi(hwnd)));
        // Note: full frame-pool/session wiring to D3D11 is non-trivial; for now we mark WGC enabled and keep target hwnd.
        wgc_enabled_ = true;
    } catch (...) {
        // fallback to GDI
        wgc_enabled_ = false;
    }
#else
    // MinGW does not support WinRT headers; use GDI capture fallback only.
    wgc_enabled_ = false;
#endif

    return true;
}

bool OverlayManager::stop_wgc_capture(std::string &out_error) {
    wgc_enabled_ = false;
    return true;
}
#pragma region math_helpers
static vr::HmdMatrix34_t multiply34(const vr::HmdMatrix34_t &a, const vr::HmdMatrix34_t &b) {
    vr::HmdMatrix34_t r; std::memset(&r,0,sizeof(r));
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) {
        float v = 0.0f;
        for (int k=0;k<3;++k) v += a.m[i][k]*b.m[k][j];
        r.m[i][j] = v;
    }
    for (int i=0;i<3;++i) {
        r.m[i][3] = a.m[i][0]*b.m[0][3] + a.m[i][1]*b.m[1][3] + a.m[i][2]*b.m[2][3] + a.m[i][3];
    }
    return r;
}

static vr::HmdMatrix34_t invert34(const vr::HmdMatrix34_t &m) {
    vr::HmdMatrix34_t r; std::memset(&r,0,sizeof(r));
    // transpose rotation
    for (int i=0;i<3;++i) for (int j=0;j<3;++j) r.m[i][j] = m.m[j][i];
    // translation = -R^T * t
    float tx = m.m[0][3], ty = m.m[1][3], tz = m.m[2][3];
    for (int i=0;i<3;++i) r.m[i][3] = -(r.m[i][0]*tx + r.m[i][1]*ty + r.m[i][2]*tz);
    return r;
}
#pragma endregion

bool OverlayManager::grab_overlay(vr::ETrackedControllerRole role, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    if (locked_) { out_error = "Overlay is locked"; return false; }
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }
    uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(role);
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) { out_error = "Controller role not tracked"; return false; }
    // compute relative = inverse(controller_pose) * overlay_abs
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &pose = poses[deviceIndex];
    if (!pose.bPoseIsValid) { out_error = "Controller pose invalid"; return false; }
    vr::HmdMatrix34_t ctrl = pose.mDeviceToAbsoluteTracking;
    vr::HmdMatrix34_t inv = invert34(ctrl);
    grab_relative_transform_ = multiply34(inv, last_abs_transform_);
    grabbed_ = true;
    grab_device_index_ = deviceIndex;
    return true;
}

bool OverlayManager::release_overlay(std::string &out_error) {
    if (!grabbed_) { out_error = "Not grabbed"; return false; }
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &pose = poses[grab_device_index_];
    if (!pose.bPoseIsValid) { out_error = "Controller pose invalid"; return false; }
    vr::HmdMatrix34_t ctrl = pose.mDeviceToAbsoluteTracking;
    vr::HmdMatrix34_t newabs = multiply34(ctrl, grab_relative_transform_);
    // set absolute transform and cache
    vr::EVROverlayError err = vr::VROverlay()->SetOverlayTransformAbsolute(overlay_handle_, vr::TrackingUniverseStanding, &newabs);
    if (err != vr::VROverlayError_None) { out_error = "SetOverlayTransformAbsolute failed on release"; return false; }
    last_abs_transform_ = newabs;
    grabbed_ = false;
    grab_device_index_ = vr::k_unTrackedDeviceIndexInvalid;
    return true;
}

bool OverlayManager::attach_to_hmd(float offset_x, float offset_y, float offset_z, std::string &out_error) {
    if (!created_) { out_error = "Overlay not created"; return false; }
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }
    uint32_t hmdIndex = vr::k_unTrackedDeviceIndex_Hmd;
    vr::HmdMatrix34_t mat; std::memset(&mat,0,sizeof(mat));
    mat.m[0][0]=1.0f; mat.m[1][1]=1.0f; mat.m[2][2]=1.0f;
    mat.m[0][3]=offset_x; mat.m[1][3]=offset_y; mat.m[2][3]=offset_z;
    vr::EVROverlayError err = vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay_handle_, hmdIndex, &mat);
    if (err != vr::VROverlayError_None) { out_error = "SetOverlayTransformTrackedDeviceRelative failed for HMD"; return false; }
    // cache absolute by multiplying
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &pose = poses[hmdIndex];
    if (pose.bPoseIsValid) {
        last_abs_transform_ = multiply34(pose.mDeviceToAbsoluteTracking, mat);
    }
    return true;
}

bool OverlayManager::is_hmd_looking_down(float threshold_degrees, bool &out_result, std::string &out_error) {
    out_result = false;
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }
    vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
    g_vr_system->GetDeviceToAbsoluteTrackingPose(vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
    const vr::TrackedDevicePose_t &hmdPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
    if (!hmdPose.bPoseIsValid) { out_error = "HMD pose invalid"; return false; }
    vr::HmdMatrix34_t hm = hmdPose.mDeviceToAbsoluteTracking;
    float fx = -hm.m[2][0];
    float fy = -hm.m[2][1];
    float fz = -hm.m[2][2];
    float len = sqrtf(fx*fx + fy*fy + fz*fz);
    if (len == 0) { out_result = false; return true; }
    // pitch (positive when looking down) = asin(-fy)
    float pitch = asinf(-fy / len) * 180.0f / 3.14159265f;
    out_result = (pitch >= threshold_degrees);
    return true;
}

bool OverlayManager::set_laser_enabled(bool enabled, std::string &out_error) {
    g_laser_enabled.store(enabled);
    if (!enabled && laser_handle_ != vr::k_ulOverlayHandleInvalid) {
        vr::VROverlay()->HideOverlay(laser_handle_);
    }
    return true;
}

bool OverlayManager::set_locked(bool locked, std::string &out_error) {
    locked_ = locked;
    return true;
}

bool OverlayManager::is_double_tap_detected(vr::ETrackedControllerRole role, bool &out_result, std::string &out_error) {
    out_result = false;
    if (role == vr::TrackedControllerRole_LeftHand) {
        if (double_tap_left_) { out_result = true; double_tap_left_ = false; return true; }
    } else if (role == vr::TrackedControllerRole_RightHand) {
        if (double_tap_right_) { out_result = true; double_tap_right_ = false; return true; }
    }
    return true;
}

bool OverlayManager::poll_physical_buttons(std::string &out_error) {
    if (!g_vr_system) { if (!initialize_openvr(out_error)) return false; }
    vr::VRControllerState_t state;
    // iterate left and right roles
    for (int r = 1; r <= 2; ++r) {
        uint32_t deviceIndex = g_vr_system->GetTrackedDeviceIndexForControllerRole(static_cast<vr::ETrackedControllerRole>(r));
        if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) continue;
        if (!g_vr_system->GetControllerState(deviceIndex, &state, sizeof(state))) continue;
        uint64_t appMask = (1ULL << vr::k_EButton_ApplicationMenu);
        bool appDown = (state.ulButtonPressed & appMask) != 0;
        auto now = std::chrono::steady_clock::now();
        if (r == 1) {
            if (appDown && !prev_app_left_) {
                if (last_app_time_left_ != std::chrono::steady_clock::time_point()) {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_app_time_left_).count();
                    if (diff <= 400) { double_tap_left_ = true; last_app_time_left_ = std::chrono::steady_clock::time_point(); }
                    else { last_app_time_left_ = now; }
                } else { last_app_time_left_ = now; }
            }
            prev_app_left_ = appDown;
        } else {
            if (appDown && !prev_app_right_) {
                if (last_app_time_right_ != std::chrono::steady_clock::time_point()) {
                    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_app_time_right_).count();
                    if (diff <= 400) { double_tap_right_ = true; last_app_time_right_ = std::chrono::steady_clock::time_point(); }
                    else { last_app_time_right_ = now; }
                } else { last_app_time_right_ = now; }
            }
            prev_app_right_ = appDown;
        }
    }
    return true;
}
#endif

} // namespace corevr

