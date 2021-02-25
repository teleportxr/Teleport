/************************************************************************************

Filename    :   TinyUI.h
Content     :   Componentized wrappers for GuiSys
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include "OVR_Math.h"
#include "OVR_FileSys.h"

#include "GUI/GuiSys.h"
#include "Locale/OVR_Locale.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace OVRFW {

/// NOTE: this requires the app to have panel.ktx as a resource
class TinyUI {
   public:
    TinyUI() : GuiSys(nullptr), Locale(nullptr) {}
    ~TinyUI() {}

    struct HitTestDevice {
        OVR::Vector3f pointerStart = {0.0f, 0.0f, 0.0f};
        OVR::Vector3f pointerEnd = {0.0f, 0.0f, 1.0f};
        OVRFW::VRMenuObject* hitObject = nullptr;
        bool clicked = false;
    };

    bool Init(const xrJava* context, OVRFW::ovrFileSys* FileSys);
    void Shutdown();
    void Update(const OVRFW::ovrApplFrameIn& in);
    void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out);

    OVRFW::VRMenuObject* AddLabel(
        const std::string& labelText,
        const OVR::Vector3f& position,
        const OVR::Vector2f& size = {100.0f, 50.0f},
        const std::string& postfix = "");
    OVRFW::VRMenuObject* AddButton(
        const std::string& label,
        const OVR::Vector3f& position,
        const OVR::Vector2f& size = {100.0f, 50.0f},
        const std::function<void(void)>& handler = {},
        const std::string& postfix = "");
    OVRFW::VRMenuObject* AddSlider(
        const std::string& label,
        const OVR::Vector3f& position,
        float* value,
        const float defaultValue,
        const float delta = 0.02f);

    OVRFW::OvrGuiSys& GetGuiSys() {
        return *GuiSys;
    }
    OVRFW::ovrLocale& GetLocale() {
        return *Locale;
    }
    std::vector<OVRFW::TinyUI::HitTestDevice>& HitTestDevices() {
        return Devices;
    }

    void AddHitTestRay(const OVR::Posef& ray, bool isClicking);

   protected:
    OVRFW::VRMenuObject* CreateMenu(
        const std::string& labelText,
        const OVR::Vector3f& position,
        const OVR::Vector2f& size,
        const std::string& postfix);

   private:
    OVRFW::OvrGuiSys* GuiSys;
    OVRFW::ovrLocale* Locale;
    std::unordered_map<VRMenuObject*, std::function<void(void)>> ButtonHandlers;
    std::vector<OVRFW::TinyUI::HitTestDevice> Devices;
    std::vector<OVRFW::TinyUI::HitTestDevice> PreviousFrameDevices;
};

} // namespace OVRFW
