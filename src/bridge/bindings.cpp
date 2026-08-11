#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <stdexcept>
#include "engine.hpp"

namespace py = pybind11;

PYBIND11_MODULE(corevr_bridge, m) {
    m.doc() = "CoreVR native bridge (pybind11)";

    m.def("initialize", []() {
        std::string err;
        bool ok = corevr::initialize_openvr(err);
        if (!ok) throw std::runtime_error(err);
        return true;
    }, "Initialize OpenVR runtime (throws on fatal error)");

    m.def("shutdown", []() {
        corevr::shutdown_openvr();
    }, "Shutdown OpenVR runtime");

    py::class_<corevr::OverlayManager>(m, "OverlayManager")
        .def(py::init<>())
        .def("create_overlay", [](corevr::OverlayManager &self, const std::string &key, const std::string &name) {
            std::string err;
            if (!self.create_overlay(key, name, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_overlay_position", [](corevr::OverlayManager &self, float x, float y, float z) {
            std::string err;
            if (!self.set_overlay_position(x, y, z, err)) throw std::runtime_error(err);
            return true;
        })
        .def("show_overlay", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.show_overlay(err)) throw std::runtime_error(err);
            return true;
        })
        .def("hide_overlay", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.hide_overlay(err)) throw std::runtime_error(err);
            return true;
        })
        .def("render_test_texture", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.render_test_texture(err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_target_window_by_title", [](corevr::OverlayManager &self, const std::string &title) {
            std::string err;
            if (!self.set_target_window_by_title(title, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_target_window_by_hwnd", [](corevr::OverlayManager &self, uint64_t hwnd_val) {
            std::string err;
            if (!self.set_target_window_by_hwnd(hwnd_val, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_overlay_alpha", [](corevr::OverlayManager &self, float alpha) {
            std::string err;
            if (!self.set_overlay_alpha(alpha, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_overlay_curvature", [](corevr::OverlayManager &self, float curvature) {
            std::string err;
            if (!self.set_overlay_curvature(curvature, err)) throw std::runtime_error(err);
            return true;
        })
        .def("process_controller_uv", [](corevr::OverlayManager &self, float u, float v, int mouse_event) {
            std::string err;
            if (!self.process_controller_uv(u, v, mouse_event, err)) throw std::runtime_error(err);
            return true;
        })
        .def("poll_controller_intersection", [](corevr::OverlayManager &self, int role) {
            int x=0,y=0; bool isDown=false; std::string err;
            if (!self.poll_controller_intersection(static_cast<vr::ETrackedControllerRole>(role), x, y, isDown, err)) throw std::runtime_error(err);
            return py::make_tuple(x,y,isDown);
        })
        .def("attach_to_wrist", [](corevr::OverlayManager &self, int role, float ox, float oy, float oz) {
            std::string err;
            if (!self.attach_to_wrist(static_cast<vr::ETrackedControllerRole>(role), ox, oy, oz, err)) throw std::runtime_error(err);
            return true;
        })
        .def("start_wgc_capture_by_title", [](corevr::OverlayManager &self, const std::string &title) {
            std::string err;
            if (!self.start_wgc_capture_by_title(title, err)) throw std::runtime_error(err);
            return true;
        })
        .def("stop_wgc_capture", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.stop_wgc_capture(err)) throw std::runtime_error(err);
            return true;
        })
        .def("trigger_haptic_feedback", [](corevr::OverlayManager &self, int role, float duration, float freq, float amp) {
            std::string err;
            if (!self.trigger_haptic_feedback(static_cast<vr::ETrackedControllerRole>(role), duration, freq, amp, err)) throw std::runtime_error(err);
            return true;
        })
        .def("is_wrist_facing_user", [](corevr::OverlayManager &self, int role, float threshold) {
            bool result=false; std::string err;
            if (!self.is_wrist_facing_user(static_cast<vr::ETrackedControllerRole>(role), threshold, result, err)) throw std::runtime_error(err);
            return result;
        })
        .def("grab_overlay", [](corevr::OverlayManager &self, int role) {
            std::string err;
            if (!self.grab_overlay(static_cast<vr::ETrackedControllerRole>(role), err)) throw std::runtime_error(err);
            return true;
        })
        .def("release_overlay", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.release_overlay(err)) throw std::runtime_error(err);
            return true;
        })
        .def("attach_to_hmd", [](corevr::OverlayManager &self, float ox, float oy, float oz) {
            std::string err;
            if (!self.attach_to_hmd(ox, oy, oz, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_laser_enabled", [](corevr::OverlayManager &self, bool enabled) {
            std::string err;
            if (!self.set_laser_enabled(enabled, err)) throw std::runtime_error(err);
            return true;
        })
        .def("set_locked", [](corevr::OverlayManager &self, bool locked) {
            std::string err;
            if (!self.set_locked(locked, err)) throw std::runtime_error(err);
            return true;
        })
        .def("is_double_tap_detected", [](corevr::OverlayManager &self, int role) {
            bool result=false; std::string err;
            if (!self.is_double_tap_detected(static_cast<vr::ETrackedControllerRole>(role), result, err)) throw std::runtime_error(err);
            return result;
        })
        .def("poll_physical_buttons", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.poll_physical_buttons(err)) throw std::runtime_error(err);
            return true;
        })
        .def("render_test_texture", [](corevr::OverlayManager &self) {
            std::string err;
            if (!self.render_test_texture(err)) throw std::runtime_error(err);
            return true;
        });
    m.def("is_hmd_looking_down", [](float threshold) {
        bool result=false; std::string err;
        corevr::OverlayManager om;
        if (!om.is_hmd_looking_down(threshold, result, err)) throw std::runtime_error(err);
        return result;
    });
}
