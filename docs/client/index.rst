Client
######

The "Reference" Teleport Client application is available for Windows PC (with optional XR support),
and Meta Quest headsets. It's available from https://teleportvr.io/downloads.

On launching the client, you will be presented with an address bar. In desktop mode, this will appear
at the top of the window, just as in a web browser.

.. figure:: DesktopClient.png
	:width: 600
	:alt: The desktop PC Client is shown, with an address bar and connect button at the top.
	
	The desktop PC Client has an address bar and connect button at the top.

In XR mode, it will float in front of you, atop a
virtual keyboard for typing. You can connect to any Teleport server by entering its address here and clicking
the connect button.

.. figure:: VRClient.png
	:width: 600
	:alt: The XR Client is shown, with an address bar and virtual keyboard in front of the viewer.
	
	The desktop PC Client has a virtual keyboard, with an address bar and connect button.

In XR mode, the address bar disappears when the connection is accepted. You can recall the address bar
by clicking the default "Menu" button for your XR device.

Most XR devices have one or two buttons that are reserved for special functions, like the Menu button or the "Meta" button on the Quest controller.
All other inputs, including headset and handset motion and other trackers, are mapped according to the Teleport
server's particular setup. Their functions will change, depending on the app you're connected to.

This is distinct from many other online XR systems. Two different Teleport servers are not two distinct
"levels", "worlds" or "locations" for a single application. They are two different apps.

Teleport URL's
--------------
A teleport URL is of the form:

``[PROTOCOL]://[DOMAIN:PORT]/[PATH]/[PARAMETERS]``

This is similar to an HTTP request, except that a Teleport URL doesn't refer to a "document" or "resource". Instead, it's a request for a real-time connection.
The [PROTOCOL] would be 'teleport'. [DOMAIN:PORT] would be a domain name of either two or three parts, or an IP address, optionally followed by a colon and a port number. The interpretation of the path and parameters are up to the server, but in general, the path should describe a location within the server's application, while the parameters should represent any other information the URL encodes.

Specifically, a Teleport URL would look like this:

``teleport://subdomain.domain.tld:port/path/?parameter=value``

For example:

`teleport://home.teleportvr.io <teleport://home.teleportvr.io>`_


