#include "ConnectionReport.h"
#include <sstream>

std::string RenderStatusLine(const ConnectionStatus &status)
{
	std::string line = "Status: " + status.state;
	if (!status.detail.empty())
		line += " (" + status.detail + ")";
	return line;
}

std::string RenderStatus(const ConnectionStatus &status)
{
	std::string result = RenderStatusLine(status) + "\n";
	if (!status.hasSession)
		return result;
	result += "Server: " + status.server + ":" + std::to_string(status.port) + "\n";
	result += "Latency: " + std::to_string(status.latencyMs) + " ms\n";
	result += "Inputs Available: " + std::to_string(status.inputsAvailable) + "\n";
	return result;
}

std::string RenderGeometrySummary(const GeometryReport &report)
{
	if (!report.hasCache)
		return "No geometry cache.\n";
	const GeometryCounts &c = report.counts;
	std::ostringstream	  o;
	o << "Nodes tracked:        " << c.nodes << " (" << c.nodesRemoved << " removed)\n";
	o << "Skeletons:            " << c.skeletons << "\n";
	o << "Resources received:   " << c.resourcesReceived << "\n";
	o << "Pointer resources:    " << c.pointers << " (URLs recorded, not downloaded)\n";
	o << "Referenced, unsent:   " << c.referencedUnsent << "\n";
	o << "Acks pending send:    " << c.pendingResourceAcks << " resources, " << c.pendingNodeAcks << " nodes\n";
	if (!report.unparsed.empty())
	{
		o << "Acknowledged without parsing:\n";
		for (const auto &u : report.unparsed)
			o << "  " << u.type << ": " << u.count << "\n";
	}
	return o.str();
}

std::string RenderGeometryNodes(const GeometryReport &report)
{
	if (!report.hasCache)
		return "No geometry cache.\n";
	if (report.nodes.empty())
		return "No nodes tracked.\n";
	std::ostringstream o;
	for (const auto &n : report.nodes)
	{
		o << n.uid << " \"" << n.name << "\" type=" << n.dataType;
		if (n.data)
			o << " data=" << n.data;
		if (n.parent)
			o << " parent=" << n.parent;
		if (n.skeleton)
			o << " skeleton=" << n.skeleton;
		if (n.materials)
			o << " materials=" << n.materials;
		if (n.animations)
			o << " animations=" << n.animations;
		if (!n.url.empty())
			o << " url=" << n.url;
		o << "\n";
	}
	return o.str();
}

std::string RenderGeometryResources(const GeometryReport &report)
{
	if (!report.hasCache)
		return "No geometry cache.\n";
	std::ostringstream o;
	if (report.pointers.empty())
	{
		o << "No pointer resources.\n";
	}
	else
	{
		o << "Pointer resources (not downloaded):\n";
		for (const auto &p : report.pointers)
			o << "  " << p.uid << " " << p.type << " " << p.url << "\n";
	}
	if (!report.referencedUnsent.empty())
	{
		o << "Referenced but never sent:\n  ";
		for (avs::uid u : report.referencedUnsent)
			o << u << " ";
		o << "\n";
	}
	return o.str();
}
