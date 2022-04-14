.. _video:

#####
Video
#####


The server must stream video to the client wth minimal latency. 
This section will describe the structure and content of the video frames, how low latency can be achieved and what the client must do to process the data.


Video Texture Layout
^^^^^^^^^^^^^^^^^^^^
The server should use a video encoder to compress the video stream. 
The rendering output of the server application should be written to a video texture and this should be provided as an input surface to the video encoder.
The protocol is flexible regarding the layout of the video texture. It is up to the client and server to support the same layout.
The resolution of the video texture is only limited to what the application needs and the encoder/decoder can support. 
It may contain one or more sub-textures as required by the client. 

Currently the Teleport protocol specifies a video texture layout for VR applications wanting to implement hybrid rendering.
The server application should render an axis-aligned cubemap each frame and send the cubemap as part of the video texture to the client.
**YUV** formats are commonly used with video encoders and therefore the alpha channel must be stored in an extra pixel.
The alpha channel will store the depth of the rendered scene on the server.
The RGB and alpha channels of the cubemap should therefore be rendered separately to the video texture as a color and depth cubemap.
The depth cubemap should be rendered at half the resolution of the color cubemap.
The resolution of the video texture is configurable but must be communicated to the client in the **VideoConfig** structure of the **SetupCommand**.
A resolution of 1536x1536 pixels is recommended for the video texture with 512x512 for each face of the color cubemap and 256x256 for each face of the depth cubemap. 

A **Tag ID** should also be included in the video texture. This links the video frame with the associated video metadata.
The metadata is sent in a separate stream. See :ref:`video_metadata`.
The **Tag ID** should be encoded as a sequence of 5 bits at the bottom right of the video texture. 
This allows 32 different IDs or a maximum delay of 31 frames between the transmission of the metadata and video texture before a **Tag ID** is reused. 

Teleport also supports the sending of lighting information in the video texture.
Each frame, the cubemap can be used by the server application to generate a specular cubemap at lower mip levels for reflections and a diffuse cubemap for global illumination.
A cubemap containing the specular lighting of the scene should be written to the video texture at 6 different mip levels.
Each mip should be half the resolution of the previous mip, from 64x64 pixels down to 2x2.
A cubemap containing the diffuse lighting of the scene should be rendered to the video texture at 64x64 resolution..
The resolution and offsets of the lighting cubemaps can be communicated to the client in the **VideoConfig** structure of the **SetupCommand**.


A webcam image may also be sent to the client. The dimensions of the webcam image and offset may vary as long as they fit in the video texture.
A flag indicating if the webcam image is being streamed and the width, height and offset of the image can be communicated to the client in the **VideoConfig** structure of the **SetupCommand**.

The video texture should be in the following form:

+----------------------------------------------------------------------+
|                        Video   Texture  Layout                       |
|                                                                      |
+=======================+=======================+======================+
|                       |                       |                      |
|      Front Face       |      Back Face        |      Right Face      |
|                       |                       |                      |
+-----------------------+-----------------------+----------------------+
|                       |                       |                      |
|     Left Face         |      Top Face         |      Bottom Face     |
|                       |                       |                      |
+-----------------------+-----------+-----------+------+---------------+
|                                   | Specular Cubemap |               |
|           Depth Cubemap           +------------------+--------+------+        
+                                   | Diffuse Cubemap  | Webcam |      | 
|                                   |                  |        |Tag ID|
+-----------------------------------+------------------+--------+------+




where

.. list-table:: Video Texture Layout
   :widths: 5 5 10 10 30
   :header-rows: 1

   * - Offset X
     - Offset Y
     - Width
     - Heigth
     - Description
   * - 0
     - 0
     - 512
     - 512
     - Color Cubemap Front Face
   * - 512
     - 0
     - 512
     - 512
     - Color Cubemap Back Face
   * - 1024
     - 0
     - 512
     - 512
     - Color Cubemap Right Face
   * - 0
     - 512
     - 512
     - 512
     - Color Cubemap Left Face
   * - 512
     - 512
     - 512
     - 512
     - Color Cubemap Top Face
   * - 1024
     - 512
     - 512
     - 512
     - Color Cubemap Bottom Face
   * - 0
     - 1024
     - 256
     - 256
     - Depth Cubemap Front Face
   * - 256
     - 1024
     - 256
     - 256
     - Depth Cubemap Back Face
   * - 512
     - 1024
     - 256
     - 256
     - Depth Cubemap Right Face
   * - 0
     - 1280
     - 256
     - 256
     - Depth Cubemap Left Face
   * - 256
     - 1280
     - 512
     - 512
     - Depth Cubemap Top Face
   * - 512
     - 1280
     - 256
     - 256
     - Depth Cubemap Bottom Face
   * - 768
     - 1280
     - 126
     - 64
     - Specular Lighting Cubemap
   * - 768
     - 1406
     - 64
     - 64
     - Diffuse Lighting Cubemap
   * - 960
     - 1406
     - 128
     - 96
     - Webcam Texture
   * - 1516
     - 1532
     - 20
     - 4
     - Tag ID

Note: In the offsets above, higher X values go from left to right and higher Y values go from top to bottom.. 




Video Frame Structure
^^^^^^^^^^^^^^^^^^^^^
The video encoder should be configured to accept the **YUV 4:2:0** 12-bit pixel format as input for the video frame.
16-bit formats such as **YUV 4:4:4** are available but **YUV 4:2:0** minimizes decoding time and latency.
The video texture must therefore be converted to the the **YUV 4:2:0** format for processing by the video encoder.
The server must send the video encoder output to the client each frame.
The raw unmodified output must be sent as one large chunk or **payload** to the client.
The structure of the output depends on the video codec used. The server and client must use the same video codec and a software or hardware video encoder and decoder that supports it.
The server must tell the client what codec is being used in the **VideoConfig** structure of the **SetupCommand**. 
For HEVC/H264, the output is made up of multiple **NAL-units** such as **picture parameters** (VPS, SPS, PPS etc.) and **video coding layers** (**VCL**) containing the compressed data of the video texture.
Each frame has at least one **VCL** and may have **picture parameters** if the frame is an IDR frame or the video encoder is configured to send the **picture parameters** with every frame.
The video data should be transferred in accordance with the section of the protocol outlined in :ref:`data_transfer`.


Recovering from Corruption
^^^^^^^^^^^^^^^^^^^^^^^^^^
An **IDR frame** is a special type of **I-frame** or keyframe in HEVC/H264. 
It does not rely on any prior frames for decoding and subsequent frames will reference it until the next **I-frame**. 
The **IDR frame** will also include **picture parameters** added by the encoder for the decoder to process. 
This includes information such as the bitrate of the encoder, the texture resolution and pixel format etc. 
The video encoder will output an **IDR frame** as the very first frame and at periodic intervals determined by the encoder settings. 

To reduce latency, the video encoder should be configured to only send the first frame as an **IDR**. 
The encoder should only produce further **IDR frames** if requested by the client.
If the client receives a corrupted video frame and the following frame references it (**P-frame**), this will cause corrupted video. 
The stream will not recover because the encoder will not automatically send a new **IDR frame**. 
Therefore, the client must be able to identify if it has missed a video frame. 
To achieve this, the client has to keep count of the number of video frames received from the server. 
The client needs to compare this count with the **stream-payload-id** set by the server. 
If there is a mismatch between both values and the current video frame is not an **IDR frame** or the video frame has been corrupted during the transfer, the client must send a HTTP message to the server requesting am **IDR frame**. 
On receiving the HTTP message, the server must tell the video encoder to force an **IDR** for the next frame.
This allows the video stream to recover.
To understand how the **stream-payload-id** is managed and how the client determines if a payload is corrupted, see :ref:`data_transfer`.


Minimizing Latency
^^^^^^^^^^^^^^^^^^
The server must configure the video encoder to minimize latency. 
Different encoders may support different settings and the capabilities of some hardware encoders will depend on the the GPU and driver installed.
The server application must therefore query the capabilities of the encoder to determine the encoder settings supported.
The video decoder on the client will be informed of these settings via the **picture parameters** received with each **IDR frame**.

The following settings are recommended to minimize latency:

1. Ultra-low latency or low latency Tuning Info
2. Rate control mode of Constant Bit Rate (CBR)
3. Multi Pass – Quarter/Full (evaluate and decide)
4. Very low VBV buffer size (e.g. single frame = bitrate/framerate)
5. No B Frames - Just I and P frames
6. Infinite GOP length
7. Adaptive quantization (AQ) enabled
8. Long term reference pictures enabled
9. Intra refresh enabled
10. Non-reference P frames
11. The first frame should be the only **IDR** sent unless recovering from a lost frame.



Processing of video Frame on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
On receiving a non-corrupted video frame, the client must parse each individual NAL-unit for the video decoder to process.
For HEVC and H264 codecs, each NAL-unit is separated by a 3-byte ALU code with bytes 1 and 2 having a value of 0 and bytes 3 having a value of 1 (001). 
The client must implement a parser to avail of this to split up the NAL-units.
Video decoders usually output the decoded video data in the same **YUV** format used as input to the video encoder.    
When the decoder has finished decoding a frame, the client must convert the **YUV** texture to an **RGBA** texture.
The cubemap and assocated lighting information must then be extracted from this texture for rendering.


