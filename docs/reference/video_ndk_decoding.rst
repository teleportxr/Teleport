.. _video_ndk_decoding:

###############################
Video Decoding with Android NDK
###############################

AMediaCodec Set Up
------------------

We use Android NDK's AMedia library to decode the incoming video stream from the server.
Firstly, we create an ``AMediaCodec`` by calling ``AMediaCodec_createDecoderByType()`` supplying a MIME type such as "video/avc" for H.264 or video/hevc" for HEVC.
Next, we config the ``AMediaCodec`` by calling ``AMediaCodec_configure()`` supplying both an ``AMediaFormat`` and an ``ANativeWindow``.
An ``AMediaFormat`` is created and specified with the follow parameters based on this Android Java documentation page (https://developer.android.com/reference/android/media/MediaFormat):

* AMEDIAFORMAT_KEY_MIME is the same as the MIME used at ``AMediaCodec`` creation.
* AMEDIAFORMAT_KEY_DURATION is set to INT64_MAX.
* AMEDIAFORMAT_KEY_WIDTH is set to the target texture's width.
* AMEDIAFORMAT_KEY_HEIGHT is set to the target texture's height.
* AMEDIAFORMAT_KEY_COLOR_FORMAT is set to COLOR_FormatYUV420Flexible. (This value was copied from %ANDROID_SDK_ROOT%\sources\android-29\android\media\MediaCodecInfo.java. Note: The ``AMediaCodec`` will select a vendor specific format ``OMX_QCOM_COLOR_FormatYUV420PackedSemiPlanar32m`` on Meta Quest and Meta Quest 2. This OMX value was copied from https://review.carbonrom.org/plugins/gitiles/CarbonROM/android_hardware_qcom_media/+/fa202b9b18f17f7835fd602db5fff530e61112b4/msmcobalt/mm-core/inc/OMX_QCOMExtns.h)

An ``AImageReader`` is created by calling ``AImageReader_newWithUsage()`` passing the target texture's width and height, the max number of images that are available to use, the internal the format that the ``AImageReader`` will store the ``AImage``, which is ``AIMAGE_FORMAT_YUV_420_888`` equivalent to ``VK_FORMAT_G8_B8R8_2PLANE_420_UNORM``, and the ``AHardwareBuffer_Usage`` stating CPU/GPU accesses and usages, which is ``AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE`` equivalent to ``VK_IMAGE_USAGE_SAMPLED_BIT``. From the ``AImageReader``, the ``ANativeWindow`` can be obtained by calling ``AImageReader_getWindow()``.

We set up the asynchrous callbacks for ``AMediaCodec`` using ``AMediaCodec_setAsyncNotifyCallback()`` and ``AMediaCodecOnAsyncNotifyCallback``:

* ``onAsyncInputAvailable()``.
* ``onAsyncOutputAvailable()``.
* ``onAsyncFormatChanged()``.
* ``onAsyncError()``.

We also set up the asynchrous callback for ``AImageReader`` using ``AImageReader_setImageListener()`` and ``AImageReader_ImageListener``:

* ``onAsyncImageAvailable()``.

Finally, we start are own thread, calling ``processBuffersOnThread()``, to process the incoming data stream from ``LibAVStream`` and passing the data into the ``AMediaCodec``.

Below is a flow chart showing the creation of the AMediaCodec.

.. image:: /images/reference/AMediaCodecSetUp.png
  :width: 800
  :alt: AMediaCodec Set Up.

Running the Decoder
-------------------

``LibAVStream`` will call ``Docode`` taking the video stream data, type and last packet information; combining all that with start codes and storing the data in the ``DataBuffer`` s.
Of primary interest from the ``AMediaCodec`` perspective is the functions ``onAsyncInputAvailable()`` and ``onAsyncOutputAvailable()``, which supply the input and output buffer IDs along with other metadata to the ``InputBuffer`` s and ``OutputBuffer`` s respectively. Later ``processBuffersOnThread()`` will to assemble enough data from the ``AMediaCodec`` decoder to process a frame.

``processBuffersOnThread()`` is looped, sequentially calling ``processInputBuffers()`` and ``processOutputBuffers()`` over and over. ``processInputBuffers()`` accumulates enough data from the ``DataBuffer`` s to assemble a frame of data, before calling ``AMediaCodec_queueInputBuffer()`` to the send the ``AMediaCodec``'s InputBuffer off to be decoded. ``AMediaCodec``'s InputBuffers are acquired from ``AMediaCodec_getInputBuffer`` via the input buffer IDs from the ``InputBuffer`` s. Once the ``AMediaCodec``is finished with a buffer, ``onAsyncOutputAvailable()`` is called, and it is stored in the ``OutputBuffer`` s, where ``processOutputBuffers()`` will call ``AMediaCodec_releaseOutputBuffer()`` freeing the buffer for reuse.

The final step happens in ``onAsyncImageAvailable()``, where the decoded image from the ``AMediaCodec`` is passed to the ``AImageReader`` for us to process into a ``VkImage``. See :ref:`video_vulkan_android_import_and_ycbcr`.

.. image:: /images/reference/AMediaCodecAndAImageReaderVideoDecoding.png
  :width: 800
  :alt: AMediaCodec and AImageReader Video Decoding.
