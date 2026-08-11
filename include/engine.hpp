#pragma once

#include <cstdint>
#include <string>
#include <atomic>
#include <chrono>
#include <openvr.h>

namespace corevr {

// Inicializa OpenVR (devuelve true si OK, false si error y pone el mensaje en out_error)
bool initialize_openvr(std::string &out_error);

// Cierra la sesión de OpenVR si estaba inicializada
void shutdown_openvr();

class OverlayManager {
public:
	OverlayManager();
	~OverlayManager();

	// Crea un overlay con una key única y un nombre legible
	bool create_overlay(const std::string &key, const std::string &name, std::string &out_error);

	// Establece posición en metros (tracking space: Standing)
	bool set_overlay_position(float x, float y, float z, std::string &out_error);

	// Mostrar / ocultar
	bool show_overlay(std::string &out_error);
	bool hide_overlay(std::string &out_error);

	// Consultas
	bool is_valid() const;

	// Inicializar DirectX (Windows). Devuelve true si la inicialización fue correcta.
	bool init_d3d(std::string &out_error);

	// Renderizar/actualizar la textura de prueba y subirla al overlay
	bool render_test_texture(std::string &out_error);

	// Window capture (target window)
	bool set_target_window_by_title(const std::string &title, std::string &out_error);
	bool set_target_window_by_hwnd(uint64_t hwnd_value, std::string &out_error);

	// Alpha / Curvature (glass mode)
	bool set_overlay_alpha(float alpha, std::string &out_error);
	bool set_overlay_curvature(float curvature, std::string &out_error);

	// Process controller intersection given UV coords (0..1) - maps to window pixels and posts mouse events
	bool process_controller_uv(float u, float v, int mouse_event /*0=move,1=down,2=up*/, std::string &out_error);

	// Poll controller intersection using ComputeOverlayIntersection and optionally post mouse events.
	bool poll_controller_intersection(vr::ETrackedControllerRole controller_role, int &out_x, int &out_y, bool &out_is_trigger_down, std::string &out_error);

	// Attach overlay to wrist/controller
	bool attach_to_wrist(vr::ETrackedControllerRole hand_role, float offset_x, float offset_y, float offset_z, std::string &out_error);

	// Start/stop Windows Graphics Capture (WGC) for a target window title
	bool start_wgc_capture_by_title(const std::string &title, std::string &out_error);
	bool stop_wgc_capture(std::string &out_error);

	// Haptic feedback
	bool trigger_haptic_feedback(vr::ETrackedControllerRole role, float duration_seconds, float frequency, float amplitude, std::string &out_error);

	// Look-at-wrist detection
	bool is_wrist_facing_user(vr::ETrackedControllerRole role, float threshold_degrees, bool &out_result, std::string &out_error);

	// Grab/drag overlays using controller grip
	bool grab_overlay(vr::ETrackedControllerRole role, std::string &out_error);
	bool release_overlay(std::string &out_error);

	// Attach to HMD
	bool attach_to_hmd(float offset_x, float offset_y, float offset_z, std::string &out_error);

	// Check if HMD is looking down (pitch > threshold_degrees)
	bool is_hmd_looking_down(float threshold_degrees, bool &out_result, std::string &out_error);

	// Global laser toggle
	bool set_laser_enabled(bool enabled, std::string &out_error);

	// Lock overlay to prevent grabbing
	bool set_locked(bool locked, std::string &out_error);

	// Double-tap detection (Application/Menu button B/Y)
	bool is_double_tap_detected(vr::ETrackedControllerRole role, bool &out_result, std::string &out_error);

	// Poll physical buttons (updates internal button state like double-tap)
	bool poll_physical_buttons(std::string &out_error);

private:
	unsigned long long overlay_handle_;
	unsigned long long laser_handle_;
	std::atomic<bool> created_;
	// Grab/drag state
	bool grabbed_;
	uint32_t grab_device_index_;
	vr::HmdMatrix34_t grab_relative_transform_;
	// previous grip per hand
	bool prev_grip_left_;
	bool prev_grip_right_;
	// cached last absolute transform of this overlay
	vr::HmdMatrix34_t last_abs_transform_;
	bool locked_;

	// Application button double-tap state
	std::chrono::steady_clock::time_point last_app_time_left_;
	std::chrono::steady_clock::time_point last_app_time_right_;
	bool double_tap_left_;
	bool double_tap_right_;
	// previous app button state
	bool prev_app_left_;
	bool prev_app_right_;
	// DirectX resources (solo en Windows)
#ifdef _WIN32
	void* d3d_device_; // ID3D11Device*
	void* d3d_context_; // ID3D11DeviceContext*
	void* d3d_texture_; // ID3D11Texture2D*
	int tex_width_;
	int tex_height_;
#ifdef _WIN32
	// Window capturing
	void* target_hwnd_; // HWND stored as void*
	bool prev_trigger_left_;
	bool prev_trigger_right_;
#ifdef _WIN32
	bool wgc_enabled_;
#endif
#endif
#endif
};

} // namespace corevr
