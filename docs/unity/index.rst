##################
Teleport Unity SDK
##################

The Teleport Unity SDK provides everything necessary to run a Teleport server as a Unity application. In typical usage, only the server need run Unity - the client
will be a standard Teleport client app such as the Teleport reference client.

Usage
*****

Installation
************
Get the Teleport Unity plugin from its Git repo at `git@github.com:simul/teleport-unity.git <https://github.com/simul/teleport-unity>`_.

You can install it as a Git submodule or copy the code directly, it should go in a subfolder of your Unity project's Assets folder, e.g. Assets/Teleport.

From your unity project, launch the Package Manager, and install the Core RP Library.

Setup
*****
If you've installed the Teleport Unity SDK as a prebuilt package, you're ready to go. But if you're building the Teleport C++ Server SDK from source, you'll need to follow the instructions in the Server section of this documentation.
Use CMakeGui to set:

* TELEPORT_SERVER to true
* TELEPORT_UNITY to true
* TELEPORT_UNITY_EDITOR_DIR to the folder where Unity.exe resides.
* TELEPORT_UNITY_PLUGINS_DIR to the folder Plugins/x86_64 in the Teleport Unity assets folder, so that the C++ SDK will build TeleportServer.dll to that directory.
* TELEPORT_UNITY_PROJECT_DIR to the root folder of your Unity project.

Then build the Teleport_Server_Unity.sln solution. You can run Unity as normal, or debug the C++ code by setting TeleportServer as the active C++ project in the solution, and launching it for debugging.

Configuration
*************
The global settings for the plugin are found in the *Edit* menu, under *Project Settings...*. In the Project Settings panel, select *Teleport VR*.

.. image:: /images/unity/ProjectSettings.png
  :width: 600
  :alt: Teleport has a page on the Project Settings panel.

**Teleport Project Settings panel**

Usage
*****
You can create a GameObject in Unity that has the Monitor component. If not, one will be created when you run the project.

When running, the server awaits connections via the Teleport protocol. When a client connects, the server creates a player instance.

Inputs
******
To show the Inputs Panel, select Inputs from the Teleport VR menu on the main menu bar.
Here, you can specify the inputs you want to receive from connected clients.

.. image:: /images/unity/InputsPanel.png
  :width: 600
  :alt: Teleport has a page on the Project Settings panel.

**Teleport Inputs Panel in Unity Editor**


There are three elements to each input. The Name is arbitrary, but should be unique in the application. The Type specifies what kind of input this is. A Float input is floating-point, while a boolean is either on or off, true or false. A state input is updated continuously, whereas an Event input is only updated when it changes. Finally, the input's *Path* is how the client knows what to map it to.

Teleport VR clients use `OpenXR interaction profiles <https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#semantic-path-interaction-profiles>`_ to know what inputs the XR hardware provides. Each input (buttons, triggers, poses etc) has a *path*.
When a Teleport client connects, it tries to match the path of each server-specified Teleport input to the OpenXR paths of its hardware. If any part of the OpenXR path matches the Teleport path, the mapping is made.
One or more Teleport inputs can be mapped to a single OpenXR input if the paths match.
Teleport input paths use `Regular Expression <https://en.wikipedia.org/wiki/Regular_expression>`_ syntax to match OpenXR paths.

For example, if the client recognizes your hardware as supporting the `Oculus Touch Controller Profile <https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#_oculus_touch_controller_profile>`_:

	/interaction_profiles/oculus/touch_controller

It will have the path:

	user/hand/left/input/x/click

to represent the "x" button on the left-hand controller. We might specify a control:

+-------------------------+-------------------+---------------------------+
|          Name           |        Type       |    Path                   |
|                         |                   |                           |
+=========================+===================+===========================+
| Toggle Onscreen Display |    Boolean Event  | left/input/[a|x]/click    |
+-------------------------+-------------------+---------------------------+

The syntax [a|x] means "either 'a' or 'x'", so the client will recognize this as a match, and map the "x" button on the client-side controller to the "Toggle Onscreen Display" boolean event that the server will receive.

Mapping types
-------------

Different control types can be mapped to each other. If a path match is found, it is possible for a boolean ("click") action, which is only on or off, to be mapped to an Analogue input.

+-------------------------+--------------------------+---------------------+-----------------+
|                         |        OpenXR Boolean    |      OpenXR float   |     OpenXR pose |
+=========================+==========================+=====================+=================+
| Teleport Boolean        |    Yes                   | Yes, client decides |      No         |
+-------------------------+--------------------------+---------------------+-----------------+
| Teleport Analogue       | Yes: false=0.0, true=1.0 | Yes                 |      No         |
+-------------------------+--------------------------+---------------------+-----------------+

The mapping of an OpenXR floating-point input to a Teleport Boolean input is determined by the client application. The usual method is by hysteresis, so when the control goes above a certain threshold, it will be considered to be "true", and when it goes below a lower threshold, it will revert to "false".
Teleport cannot map OpenXR Pose actions to inputs, these are handled differently.

Pose Mapping
------------
XR devices such as headsets, handsets and trackers report their state as a "pose", containing position and orientation in space. These poses can be mapped to spatial nodes using the Teleport Controller component in Unity.

.. image:: /images/unity/TeleportController.png
  :width: 600

**Teleport controller component in Unity Inspector**

The "Pose Regex Path" for a controller is matched client-side to an OpenXR path representing a pose state. When this mapping occurs, the object will be controlled directly by the tracked controller on the client. The Teleport_Controller component can be added to any child of a Teleport_SessionComponent Game Object.

Player Session Hierarchy
************************

Typically, the Unity Game Objects would be arranged as follows in Unity:

.. image:: /images/unity/PlayerHierarchy.png
  :width: 400

**Typical session and player hierarchy in Unity**

i.e. at the root, an object containing a Teleport_Session Component, which tracks client-specific session data. Below this, a Player object which may move in space. Below that, two controllers and a head tracking object.

* TeleportVR : Teleport_Session Component

  * Player

	* Left Hand Controller : Teleport_Controller Component
	* Right Hand Controller: Teleport_Controller Component
	* Head: Teleport_Head Component



.. doxygenclass:: teleport::Monitor
	:project: TeleportUnity
	:members:

.. doxygenclass:: teleport::Teleport_SessionComponent
	:project: TeleportUnity
	:members:
