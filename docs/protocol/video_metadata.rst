.. _video-metadata-reference:

##############
Video Metadata
##############

The Teleport protocol specifies that **video metadata** be sent in a separate stream to the video frame.
This allows more modularity as changes in the structure or content of the metadata will not impact the video stream.


Structure of Metadata
^^^^^^^^^^^^^^^^^^^^^
Teleport defines a structure suitable for hybrid rendering between the client and the server.
Teleport requires that the metadata contain a header consisting of useful information for the client application.
The remaining content of the metadata may vary depending on the needs of the application.
The header consists of a **timestamp** in milliseconds intended to inform the client of when the metadata was created by the server application.
The metadata header must also have a **Tag ID** which is an unsigned integer in the range of 0 - 31 that uniquely identifies the video frame the metadata belongs to.
A **Tag ID* is written as 5 bits to the **video texture** by the server and this should be used by the client to match the video frame with its associated metadata.


The metadata is of the form:

+--------------------------+-------------------+---------------------------+
|          bytes           |        type       |    description            |
|                          |                   |                           |
+==========================+===================+===========================+
|      8                   |    uint64         |    timestamp_unix_ms      |
+--------------------------+-------------------+---------------------------+
|      4                   |    uint32         |       tag id              |
+--------------------------+-------------------+---------------------------+
|      40                  |   Transform       |    cameraTransform        |
+--------------------------+-------------------+---------------------------+
|      4                   |    uint32         |       lightCount          |
+--------------------------+-------------------+---------------------------+
|      4                   |    float          |    diffuseAmbientScale    |
+--------------------------+-------------------+---------------------------+
|LightTagData * lightCount |    LightTagData   |         lights            |
+--------------------------+-------------------+---------------------------+




where

.. list-table:: Transform
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 12
     - avs::vec3
     - position
   * - 16
     - avs::vec4
     - rotation
   * - 12
     - avs::vec3
     - scale
  
and where

.. list-table:: LightTagData
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 40
     - avs::Transform
     - worldTransform
   * - 16
     - avs::vec4
     - color
   * - 4
     - float
     - range
   * - 4
     - float
     - spotAngle
   * - 1
     - avs::LightType
     - lightType
   * - 12
     - avs::vec3
     - position
   * - 16
     - avs::vec4
     - orientation
   * - 64
     - float[4][4]
     - shadowProjectionMatrix
   * - 64
     - float[4][4]
     - worldToShadowMatrix
   * - 8
     - int[2]
     - texturePosition
   * - 4
     - int
     - textureSize
   * - 8
     - uint64_t
     - uid



Sending Metadata to the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Each instance of metadata should be sent by the server as an individual **payload**.
Every video frame payload needs to have one matching payload of metadata.
The metadata must be received on the client before the associated video frame as it will be needed to render the contents of the video frame correctly.
For each video frame, the server must write the same **Tag ID** to the video frame and associated metadata.
The current time should be written to the unix timestamp of the metadata on its creation.
The camera position and rotation at the time the **cubemap** was rendered on the server should be added to the **cameraTransform** member of the metadata.
For more details on the structure of the **cubemap**, see :ref:`_video_reference`.
The diffuse to ambient scale in the scene of the server applicatiion's renderer should also be added to the metadata.
For each light in the scene, an instance of **avs::LightTagData** should be created and populated with the light's properties.
Each payload of metadata should be sent to the client using the protocols and processes described in :ref:`_data_transfer_reference`.



Processing Metadata on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The client should receive the payload of metadata before the related video frame.
The metadata should be cached by the client's application for use when the video frame is received.
Metadata needed to render the content of the video frame should be uploaded to the GPU for shader accessibility. 
This includes the **Tag ID*, **cameraTransform**, **diffuseAmbientScale** and **lights**.
The **Tag ID* must used by the shader to use the correct the metadata for a video frame.
The position and rotation of the camera will be needed for the reprojection algorithm used to render the **cubemap** contained in video frame correctly.
The **diffuseAmbientScale** and **lights** should be used by the client application's renderer to render local geometry.
