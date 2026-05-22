/**
 * @file rmg.hpp
 * @brief RV1106 vision runtime - minimal capture base
 *
 * 当前 reset 只保留 Milestone 1 所需接口。VENC、RTSP、FileSaver 和
 * Pipeline 会在 runtime 抽象稳定后以 node/edge 形式重新引入。
 */

#pragma once

#include "MediaFrame.hpp"
#include "MediaModule.hpp"
#include "SystemManager.hpp"
#include "VideoCapture.hpp"
