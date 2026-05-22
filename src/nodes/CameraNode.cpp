#include "nodes/CameraNode.hpp"

#include <utility>

namespace rmg::nodes {

    CameraNode::CameraNode(CameraConfig config) :
        config_(std::move(config)), isp_(isp_config()), vi_(vi_config()) {}

    CameraNode::~CameraNode() { (void) close(); }

    Result<void> CameraNode::open() {
        auto current = state_.load();
        if (current == runtime::NodeState::kOpen || current == runtime::NodeState::kRunning) {
            return Result<void>::success();
        }

        if (current == runtime::NodeState::kError) {
            (void) close();
        }

        auto mpi_result = sys::MpiSystem::acquire();
        if (!mpi_result) {
            mark_error();
            return Result<void>::failure(std::move(mpi_result).error());
        }
        mpi_ = std::move(mpi_result).value();

        auto isp_result = isp_.open();
        if (!isp_result) {
            (void) close();
            mark_error();
            return isp_result;
        }

        auto vi_result = vi_.open();
        if (!vi_result) {
            (void) close();
            mark_error();
            return vi_result;
        }

        state_.store(runtime::NodeState::kOpen);
        return Result<void>::success();
    }

    Result<void> CameraNode::start() {
        auto current = state_.load();
        if (current == runtime::NodeState::kRunning) {
            return Result<void>::success();
        }

        if (current != runtime::NodeState::kOpen) {
            return Result<void>::failure(
                Error::invalid_state("CameraNode", "start", "camera must be open before start"));
        }

        state_.store(runtime::NodeState::kRunning);
        return Result<void>::success();
    }

    Result<void> CameraNode::stop() {
        auto current = state_.load();
        if (current == runtime::NodeState::kRunning) {
            state_.store(runtime::NodeState::kOpen);
        }
        return Result<void>::success();
    }

    Result<void> CameraNode::close() {
        if (state_.load() == runtime::NodeState::kRunning) {
            (void) stop();
        }

        vi_.close();
        isp_.close();
        mpi_ = sys::MpiSystem::Handle();
        state_.store(runtime::NodeState::kClosed);

        return Result<void>::success();
    }

    Result<YuvFrame> CameraNode::read_frame(int timeout_ms) {
        if (state_.load() != runtime::NodeState::kRunning) {
            return Result<YuvFrame>::failure(
                Error::invalid_state("CameraNode", "read_frame", "camera must be running before reading frames"));
        }

        auto frame_result = vi_.read_frame(timeout_ms);
        if (!frame_result) {
            if (frame_result.error().code == ErrorCode::kTimeout) {
                ++stats_.timeouts;
            } else {
                ++stats_.errors;
            }
            return frame_result;
        }

        ++stats_.frames_read;
        return frame_result;
    }

    Result<uint32_t> CameraNode::current_fps() const { return vi_.current_fps(); }

    Result<void> CameraNode::set_frame_rate(uint32_t fps) { return isp_.set_frame_rate(fps); }

    Result<void> CameraNode::set_mirror_flip(bool mirror, bool flip) { return isp_.set_mirror_flip(mirror, flip); }

    sys::IspConfig CameraNode::isp_config() const {
        sys::IspConfig config;
        config.cam_id = config_.cam_id;
        config.iq_path = config_.iq_path;
        config.hdr_mode = config_.hdr_mode;
        config.multi_cam = config_.multi_cam;
        return config;
    }

    sys::ViChannelConfig CameraNode::vi_config() const {
        sys::ViChannelConfig config;
        config.cam_id = config_.cam_id;
        config.pipe_id = config_.pipe_id;
        config.chn_id = config_.chn_id;
        config.width = config_.width;
        config.height = config_.height;
        config.dev_name = config_.dev_name;
        config.pixel_format = config_.pixel_format;
        config.buf_count = config_.buf_count;
        config.depth = config_.depth;
        return config;
    }

    void CameraNode::mark_error() {
        ++stats_.errors;
        state_.store(runtime::NodeState::kError);
    }

} // namespace rmg::nodes
