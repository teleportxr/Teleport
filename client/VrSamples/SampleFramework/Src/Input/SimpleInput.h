/************************************************************************************

Filename    :   SimpleInput.h
Content     :   Helper around VRAPI input calls
Created     :   July 2020
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

************************************************************************************/
#pragma once

#include "VrApi.h"
#include "VrApi_Input.h"
#include "OVR_Math.h"

namespace OVRFW {

class SimpleInput {
   public:
    SimpleInput() {
        Reset();
    }
    ~SimpleInput() = default;

    static const ovrDeviceID kInvalidDeviceID = 0;

    void Reset() {
        controllerL_ = kInvalidDeviceID;
        controllerR_ = kInvalidDeviceID;
        handL_ = kInvalidDeviceID;
        handR_ = kInvalidDeviceID;
    }

    void Update(ovrMobile* ovr, double displayTimeInSeconds) {
        Reset();
        /// Enumerate
        for (uint32_t deviceIndex = 0;; deviceIndex++) {
            ovrInputCapabilityHeader capsHeader;
            if (vrapi_EnumerateInputDevices(ovr, deviceIndex, &capsHeader) < 0) {
                break; // no more devices
            }

            if (capsHeader.Type == ovrControllerType_TrackedRemote) {
                ovrInputTrackedRemoteCapabilities remoteCaps;
                remoteCaps.Header = capsHeader;
                ovrResult result = vrapi_GetInputDeviceCapabilities(ovr, &remoteCaps.Header);
                if (result == ovrSuccess) {
                    if ((remoteCaps.ControllerCapabilities & ovrControllerCaps_LeftHand) != 0) {
                        controllerL_ = capsHeader.DeviceID;
                    } else {
                        controllerR_ = capsHeader.DeviceID;
                    }
                }
            }
            if (capsHeader.Type == ovrControllerType_Hand) {
                ovrInputHandCapabilities handCaps;
                handCaps.Header = capsHeader;
                ovrResult result = vrapi_GetInputDeviceCapabilities(ovr, &handCaps.Header);
                if (result == ovrSuccess) {
                    if ((handCaps.HandCapabilities & ovrHandCaps_LeftHand) != 0) {
                        handL_ = capsHeader.DeviceID;
                    } else {
                        handR_ = capsHeader.DeviceID;
                    }
                }
            }
        }
        /// update states
        if (IsLeftHandTracked()) {
            handInputStatePrevL_ = handInputStateL_;
            handInputStateL_.Header.ControllerType = ovrControllerType_Hand;
            (void)vrapi_GetCurrentInputState(ovr, handL_, &handInputStateL_.Header);
            handPoseL_.Header.Version = ovrHandVersion_1;
            (void)vrapi_GetHandPose(ovr, handL_, displayTimeInSeconds, &(handPoseL_.Header));
        } else {
            handInputStatePrevL_.InputStateStatus = 0u;
        }
        if (IsRightHandTracked()) {
            handInputStatePrevR_ = handInputStateR_;
            handInputStateR_.Header.ControllerType = ovrControllerType_Hand;
            (void)vrapi_GetCurrentInputState(ovr, handR_, &handInputStateR_.Header);
            handPoseR_.Header.Version = ovrHandVersion_1;
            (void)vrapi_GetHandPose(ovr, handR_, displayTimeInSeconds, &(handPoseR_.Header));
        } else {
            handInputStatePrevR_.InputStateStatus = 0u;
        }

        if (IsLeftControllerTracked()) {
            remoteInputStateL_.Header.ControllerType = ovrControllerType_TrackedRemote;
            (void)vrapi_GetCurrentInputState(ovr, controllerL_, &remoteInputStateL_.Header);
            (void)vrapi_GetInputTrackingState(
                ovr, controllerL_, displayTimeInSeconds, &controllerPoseL_);
        }
        if (IsRightControllerTracked()) {
            remoteInputStateR_.Header.ControllerType = ovrControllerType_TrackedRemote;
            (void)vrapi_GetCurrentInputState(ovr, controllerR_, &remoteInputStateR_.Header);
            (void)vrapi_GetInputTrackingState(
                ovr, controllerR_, displayTimeInSeconds, &controllerPoseR_);
        }
    }

    bool IsLeftHandTracked() const {
        return handL_ != kInvalidDeviceID;
    }
    bool IsRightHandTracked() const {
        return handR_ != kInvalidDeviceID;
    }
    bool IsLeftControllerTracked() const {
        return controllerL_ != kInvalidDeviceID;
    }
    bool IsRightControllerTracked() const {
        return controllerR_ != kInvalidDeviceID;
    }

    bool IsLeftHandPinching() const {
        return (handInputStateL_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }
    bool IsRightHandPinching() const {
        return (handInputStateR_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }
    bool WasLeftHandPinching() const {
        return (handInputStatePrevL_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }
    bool WasRightHandPinching() const {
        return (handInputStatePrevR_.InputStateStatus & ovrInputStateHandStatus_IndexPinching) != 0;
    }

    const ovrInputStateTrackedRemote& LeftControllerInputState() const {
        return remoteInputStateL_;
    }
    const ovrInputStateTrackedRemote& RightControllerInputState() const {
        return remoteInputStateR_;
    }

    const ovrInputStateHand& LeftHandInputState() const {
        return handInputStateL_;
    }
    const ovrInputStateHand& RightHandInputState() const {
        return handInputStateR_;
    }

    const ovrInputStateHand& PreviousLeftHandInputState() const {
        return handInputStatePrevL_;
    }
    const ovrInputStateHand& PreviousRightHandInputState() const {
        return handInputStatePrevR_;
    }

    const ovrHandPose& LeftHandPose() const {
        return handPoseL_;
    }
    const ovrHandPose& RightHandPose() const {
        return handPoseR_;
    }

    OVR::Posef LeftControllerPose() const {
        // Note: this does a cast conversion
        return controllerPoseL_.HeadPose.Pose;
    }
    OVR::Posef RightControllerPose() const {
        // Note: this does a cast conversion
        return controllerPoseR_.HeadPose.Pose;
    }

   private:
    ovrDeviceID controllerL_;
    ovrDeviceID controllerR_;
    ovrDeviceID handL_;
    ovrDeviceID handR_;
    ovrInputStateTrackedRemote remoteInputStateL_;
    ovrInputStateTrackedRemote remoteInputStateR_;
    ovrInputStateHand handInputStateL_;
    ovrInputStateHand handInputStateR_;
    ovrInputStateHand handInputStatePrevL_;
    ovrInputStateHand handInputStatePrevR_;
    ovrHandPose handPoseL_;
    ovrHandPose handPoseR_;
    ovrTracking controllerPoseL_;
    ovrTracking controllerPoseR_;
};

} // namespace OVRFW
