'Spatial Browser Scripting Test!'

function recurseHierarchy(node)
{
	log('Node ' +node.nodeName);
	//for (let child in node.children)
	node.children.forEach(child => {
		recurseHierarchy(child);
	});
}

let rootnode = scene.getRootNode();
let node1 = scene.createNode(rootnode, "1");
let node11 = scene.createNode(node1, "11");
let node2 = scene.createNode(rootnode, "2");

rootnode.children.forEach(child => {
	log('Child name:'+ child.nodeName);
});

recurseHierarchy(rootnode);
log('end');

