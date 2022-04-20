Requests from Client to Server 
##############################

A client may send any of the following one or more times per frame:

  * DisplayInfo
  * ControllerPoses
  * OriginPose
  * Inputs
  * ResourceRequests
  * ResourceAcknowledgements
  * NodeUpdates

The **Client** sends all of its messages in its local units and Axes Standard. For example, if the **Client** works in metres and has Y vertical, positions and rotations sent to the server will be in this format.

The **Server** sends all of its commands to the **Client** in the **Client's** local units and standard.

DisplayInfo
-----------
Reference implementation: :cpp:struct:`avs::DisplayInfo`

.. list-table:: DisplayInfo
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 4
     - uint32_t
     - width
   * - 4
     - uint32_t
     - height
     
ControllerPoses
---------------
Reference implementation: :cpp:struct:`avs::ControllerPosesMessage`

.. list-table:: ControllerPoses
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 28
     - Pose
     - head pose
   * - 28
     - Pose
     - left controller pose
   * - 28
     - Pose
     - right controller pose
  
where Pose is:

.. list-table:: Pose
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 16
     - 4-vector of floats
     - orientation (quaternion)
   * - 12
     - 3-vector of floats
     - position


ResourceRequests
----------------
  
.. list-table:: ResourceRequests
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - uint64_t
     - number of resources requested = N
   * - 8*N
     - uid
     - resource uid's
  
ReceivedResources
-----------------
Reference implementation: :cpp:struct:`avs::ReceivedResourcesMessage`
  
.. list-table:: ReceivedResources
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - uint64_t
     - number of resources acknowledged = N
   * - 8*N
     - uid
     - resource uid's

NodeUpdates
-----------
Reference implementation: :cpp:struct:`avs::NodeStatusMessage`

.. list-table:: NodeUpdates
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - uint64_t
     - number of received nodes to report = N
   * - 8
     - uint64_t
     - number of lost nodes to report = M
   * - 8*N
     - uid
     - received node uid's
   * - 8*M
     - uid
     - lost node uid's

     

Video Keyframe Request
----------------------

.. list-table:: Video Keyframe Request
   :widths: 5 10 30
   :header-rows: 1

   * - Bytes
     - Type
     - Description
   * - 8
     - uint64_t
     - number of received nodes to report = N