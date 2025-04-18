###################
Teleport NodeJS SDK
###################

Perhaps the simplest way to create a Teleport server is with npm and node.js. The Teleport NodeJS SDK is pre-alpha.

Getting Started
===============

Create Your Spatial App
-----------------------

A simple example of a Spatial Internet site is `teleport-nodejs-server-example <https://github.com/teleportxr/teleport-nodejs-server-example.git>`_. You can simply clone this repo and jump straight to :ref:`host_nodejs`.

Alternatively, follow the instructions below to create a spatial site from scratch.

Getting started
---------------

First you will need node and npm. Follow the instructions `here <https://docs.npmjs.com/downloading-and-installing-node-js-and-npm>`_ for your development platform.

Create a new directory for your project and navigate to it in a terminal.

.. code-block:: bash

   mkdir teleport-nodejs-example
   cd teleport-nodejs-example

Then run the following command to create a new package.json file:

.. code-block:: bash

   npm init -y

Now install the Teleport node server package:

.. code-block:: bash

   npm install teleportxr --save

Create a file called `server.js`, and add the following code to it:

.. code-block:: javascript

	const teleport_server	= require('teleportxr')
	const client_manager	= require('teleportxr/client/client_manager');
	const scene				= require("teleportxr/scene/scene");
	const resources			= require("teleportxr/scene/resources");
	const path				= require('path');

	const express = require('express');
	const http = require('http');
	const socketIo = require('socket.io');

	const WebSocketServer = require("ws");

	// Create a scene, so we can fill it with stuff.
	var sc					= new scene.Scene();

	// Load our scene.json into the scene.
	const assetsPath		= path.join(__dirname,'assets');
	sc.Load(path.join(assetsPath,'scene.json'));

	// The client manager stores the clients and their state.
	var cm					= client_manager.getInstance();
	
	// The client manager allows us to set callbacks for when client events happen.
	// This is our app's callback for when a new client is to be created.
	// It must return the origin uid for the client.
	function createNewClient(clientID) {
		var origin_uid		=sc.CreateNode("Player");
		return origin_uid;
	}
	cm.SetNewClientCallback(createNewClient);

	// Define a function to be called AFTER a client has been created.
	function onClientPostCreate(clientID) {
		var client			=cm.GetClient(clientID);
		client.SetScene(sc);
	}
	cm.SetClientPostCreationCallback(onClientPostCreate);

	// Having set up the callbacks, we start the server running.

	const signaling_port = process.env.PORT || 8080;
	const wss=teleport_server.initServer();

	// Create a simple http server for scene management and display.
	// This will be accessible at localhost:9000 via a browser.
	// The dashboard uses the writeState functions of the teleport server classes
	// to send html summaries of their state to the dashboard.
	const express_app = express();
	express_app.use(express.static('dashboard_public'));

	// Also serve any static 3D resources when requested.
	express_app.use(express.static('http_resources'));

	// Don't pass express_app to createServer - that would cause it to initalize before websockets is connected
	const http_server = express_app.listen(signaling_port);

	// Also mount the app here
	http_server.on('upgrade', function upgrade(request, socket, head) {
		const { pathname } = new URL(request.url, 'wss://base.url');
		 wss.handleUpgrade(request, socket, head, function done(ws) {
			wss.emit('connection', ws, request);
	  });
	});
	resources.Resource.SetDefaultPathRoot(`http://localhost:${signaling_port}`)
	console.log(`Dashboard: http://localhost:${signaling_port}`);

	
.. _host_nodejs:

Deploy the application
----------------------




`git@github.com:simul/teleport-nodejs.git <https://github.com/simul/teleport-nodejs>`_.