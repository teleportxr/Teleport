#pragma once
#include "TeleportServer/Export.h"
#include "TeleportServer/InteropStructures.h"
#include <libavstream/common_exports.h>
#include <libavstream/node.h>

TELEPORT_EXPORT void Server_ClearGeometryStore();

TELEPORT_EXPORT avs::uid Server_GenerateUid();
TELEPORT_EXPORT avs::uid Server_GetOrGenerateUid(const char *path);

TELEPORT_EXPORT bool Server_SetCachePath(const char* path);
TELEPORT_EXPORT bool Server_StoreNode(avs::uid id,const InteropNode &node);

TELEPORT_EXPORT bool Server_UpdateNodeTransform(avs::uid id,const avs::Transform &tr);
TELEPORT_EXPORT bool Server_StoreTexture(avs::uid id,const char *path, std::time_t lastModified, InteropTexture texture, bool genMips, bool highQualityUASTC, bool forceOverwrite);
TELEPORT_EXPORT bool Server_IsTextureStored(avs::uid id);
TELEPORT_EXPORT bool Server_StoreMesh(avs::uid id,const char *path, std::time_t lastModified, const InteropMesh *mesh, avs::AxesStandard extractToStandard, bool verify);
TELEPORT_EXPORT bool Server_StoreMaterial(avs::uid id, const char *path, std::time_t lastModified, InteropMaterial material);
TELEPORT_EXPORT bool Server_GetNode(avs::uid id, InteropNode *node);
TELEPORT_EXPORT avs::Node *Server_GetModifiableNode(avs::uid id);

TELEPORT_EXPORT void Server_Tick(float deltaTimeSeconds);

TELEPORT_EXPORT void Server_Teleport_Shutdown();

TELEPORT_EXPORT avs::uid Server_GetUnlinkedClientID();

TELEPORT_EXPORT uint64_t Server_GetNumberOfTexturesWaitingForCompression();
TELEPORT_EXPORT bool Server_GetMessageForNextCompressedTexture(char *str, size_t len);
TELEPORT_EXPORT void Server_CompressNextTexture();