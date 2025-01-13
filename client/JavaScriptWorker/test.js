'Spatial Browser Scripting Test!'

function recurseHierarchy(node)
{
	log('Node '+node.uid.toString()+' '+node.nodeName);
	for(let child in node.children)
	{
		recurseHierarchy(child);
	}
}

let rootnode = scene.getRootNode();
let node1 = scene.createNode(rootnode, "1");
let node11 = scene.createNode(node1, "11");
let node2 = scene.createNode(rootnode, "2");
recurseHierarchy(rootnode);
log('end');

