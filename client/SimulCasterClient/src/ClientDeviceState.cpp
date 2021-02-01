#include "ClientDeviceState.h"
#include "basic_linear_algebra.h"

ClientDeviceState::ClientDeviceState():
localOriginPos(0,0,0)
,relativeHeadPos(0,0,0)
, transformToLocalOrigin(scr::mat4::Translation(-localOriginPos))
{
}