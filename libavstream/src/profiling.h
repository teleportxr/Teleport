#if TRACY_ENABLE
#include <tracy/Tracy.hpp>
#endif

#if TRACY_ENABLE
#define TELEPORT_FRAME_END FrameMark
#define TELEPORT_PROFILE_ZONE(a) ZoneNamed(a, true)
#define TELEPORT_PROFILE_ZONE_NUM(a) ZoneValue(a)
#define TELEPORT_PROFILE_AUTOZONE ZoneScoped
#else
#define TELEPORT_FRAME_END
#define TELEPORT_PROFILE_ZONE(a)
#define TELEPORT_PROFILE_AUTOZONE
#endif