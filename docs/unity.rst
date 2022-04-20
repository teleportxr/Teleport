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

The Teleport Inputs Panel in Unity Editor.


There are three elements to each input. The Name is arbitrary, but should be unique in the application. The type specifies what kind of input this is. A Float input is floating-point, while a boolean is either on or off, true or false. A state input is updated continuously, whereas an Event input is only updated when it changes. Finally, the input's *Path* is how the client knows what to map it to.

Teleport VR clients use `OpenXR interaction profiles<https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#semantic-path-interaction-profiles>`_ to know what inputs the XR hardware provides. Each input (buttons, triggers, poses etc) has a *path*.
When a Teleport client connects, it tries to match the path of each server-specified Teleport input to the OpenXR paths of its hardware. If any part of the OpenXR path matches the Teleport path, the mapping is made.
One or more Teleport inputs can be mapped to a single OpenXR input if the paths match.
Teleport input paths use `Regular Expression <https://en.wikipedia.org/wiki/Regular_expression>`_ syntax to match OpenXR paths.


.. doxygenclass:: teleport::Monitor
	:project: TeleportUnity
	:members:

.. doxygenclass:: teleport::Teleport_SessionComponent
	:project: TeleportUnity
	:members:
