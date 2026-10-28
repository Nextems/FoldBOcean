#pragma once
#include "tree.h"
#include <cstdint>
namespace tree {
constexpr std::uint64_t REVEAL_MS=450;
struct Reveal {std::string group;Point origin;std::uint64_t started;int depth;bool closing=false;float from=0;std::uint64_t duration=REVEAL_MS;};
inline float revealProgress(const Reveal& reveal,std::uint64_t now) {
    const float t=now<=reveal.started?0:std::min(1.0f,static_cast<float>(now-reveal.started)/reveal.duration);
    const float eased=t*t*(3-2*t);
    return reveal.closing?reveal.from*(1-eased):reveal.from+(1-reveal.from)*eased;
}
struct Frame {
    std::vector<Point> points;
    std::vector<float> nodeAlpha;
    std::vector<bool> interactive;
    std::vector<std::pair<Point,Point>> edges;
    std::vector<float> edgeAlpha;
};
inline Frame animationFrame(const Scene& scene,const std::vector<Reveal>& reveals,std::uint64_t now) {
    Frame frame;
    frame.points.reserve(scene.nodes.size());frame.nodeAlpha.reserve(scene.nodes.size());frame.interactive.reserve(scene.nodes.size());
    frame.edges.reserve(scene.edges.size());frame.edgeAlpha.reserve(scene.edges.size());
    for(const auto& node:scene.nodes){frame.points.push_back(node.point);frame.nodeAlpha.push_back(1);frame.interactive.push_back(true);}
    for(const auto& edge:scene.edges){frame.edges.emplace_back(edge.a,edge.b);frame.edgeAlpha.push_back(1);}
    // Apply descendants before their ancestors, so overlapping reveals compose correctly.
    int maximum=0;for(const auto& reveal:reveals)maximum=std::max(maximum,reveal.depth);
    for(int depth=maximum;depth>=0;--depth)for(const auto& reveal:reveals)if(reveal.depth==depth) {
        const float progress=revealProgress(reveal,now);
        const float travel=reveal.group.empty()?0.08f:0.14f;
        const float position=1-travel*(1-progress);
        const auto move=[&](Point p){return Point{reveal.origin.x+(p.x-reveal.origin.x)*position,reveal.origin.y+(p.y-reveal.origin.y)*position};};
        for(size_t i=0;i<scene.nodes.size();++i)if(contains(scene.nodes[i].chain,reveal.group)&&scene.nodes[i].item["id"]!=reveal.group) {
            frame.points[i]=move(frame.points[i]);frame.nodeAlpha[i]*=progress;
            if(reveal.closing||progress<1)frame.interactive[i]=false;
        }
        for(size_t i=0;i<scene.edges.size();++i)if(contains(scene.edges[i].chain,reveal.group)) {
            const auto& edge=scene.edges[i];
            if(!reveal.group.empty()&&edge.b.x==reveal.origin.x&&edge.b.y==reveal.origin.y)continue;
            frame.edges[i]={move(frame.edges[i].first),move(frame.edges[i].second)};frame.edgeAlpha[i]*=progress;
        }
    }
    return frame;
}
inline int hit(const Frame& frame,Point p) {
    for(int i=static_cast<int>(frame.points.size())-1;i>=0;--i)if(frame.interactive[i]&&std::abs(p.x-frame.points[i].x)<=HALF_WIDTH&&std::abs(p.y-frame.points[i].y)<=HALF_HEIGHT)return i;
    return -1;
}
inline bool inside(const Frame& frame,Point point,float margin) {
    for(size_t i=0;i<frame.points.size();++i)if(frame.nodeAlpha[i]>0.05f&&std::abs(point.x-frame.points[i].x)<=HALF_WIDTH+margin&&std::abs(point.y-frame.points[i].y)<=HALF_HEIGHT+margin)return true;
    for(size_t i=0;i<frame.edges.size();++i)if(frame.edgeAlpha[i]>0.05f&&segmentDistance(point,frame.edges[i].first,frame.edges[i].second)<=margin)return true;
    return false;
}
inline bool inside(const Scene& scene,const Frame& frame,Point point,float margin,const std::string& group) {
    for(size_t i=0;i<frame.points.size();++i)if(contains(scene.nodes[i].chain,group)&&frame.nodeAlpha[i]>0.05f&&std::abs(point.x-frame.points[i].x)<=HALF_WIDTH+8&&std::abs(point.y-frame.points[i].y)<=HALF_HEIGHT+8)return true;
    for(size_t i=0;i<frame.edges.size();++i)if(contains(scene.edges[i].chain,group)&&frame.edgeAlpha[i]>0.05f&&segmentDistance(point,frame.edges[i].first,frame.edges[i].second)<=margin)return true;
    return false;
}
}
