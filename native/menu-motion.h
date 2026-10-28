#pragma once
#include "radial.h"
namespace motion {
constexpr ULONGLONG DURATION=450,SINK_MS=400,RETURN_MS=350;
inline float progress(ULONGLONG started,ULONGLONG now,ULONGLONG duration=DURATION){const float t=now<=started?0:std::min(1.f,static_cast<float>(now-started)/duration);return t*t*(3-2*t);}
inline bool sunk(int depth,size_t count){return depth>0&&depth<=static_cast<int>(count)-4;}
inline float compactShift(int depth,size_t count,float step=6){const int hidden=std::max(0,static_cast<int>(count)-4);if(depth<=hidden||hidden==0)return 0;float inner=radial::radius(0,step)+10;for(int d=hidden+1;d<depth;++d)inner+=radial::thickness(d,step)+10;return inner-radial::innerRadius(depth,step);}
inline size_t parentKeep(int depth,bool openedFolder){return static_cast<size_t>(depth+(openedFolder?1:0));}
struct RingValue {float zoom=1,alpha=1,shift=0;};
struct RingTransition {RingValue from{.94f,0},to{1,1};ULONGLONG duration=DURATION;bool submerged=false;ULONGLONG alphaDuration=0;};
inline RingValue sample(const RingTransition& transition,ULONGLONG started,ULONGLONG now){const float t=progress(started,now,transition.duration),a=progress(started,now,transition.alphaDuration?transition.alphaDuration:transition.duration);return {transition.from.zoom+(transition.to.zoom-transition.from.zoom)*t,transition.from.alpha+(transition.to.alpha-transition.from.alpha)*a,transition.from.shift+(transition.to.shift-transition.from.shift)*t};}
inline bool retarget(RingTransition& transition,ULONGLONG& started,ULONGLONG now,bool submerged,float sinkZoom,float shift=0){const float zoom=submerged?sinkZoom:1.f;if(transition.submerged==submerged&&std::abs(transition.to.shift-shift)<.001f&&std::abs(transition.to.zoom-zoom)<.001f)return false;const auto current=sample(transition,started,now);const bool inward=submerged||(!transition.submerged&&shift<transition.to.shift);transition={current,{zoom,submerged?0.f:1.f,shift},inward?SINK_MS:RETURN_MS,submerged};started=now;return true;}
inline radial::Sector scaledRing(const radial::Sector& target,float zoom,float shift=0){auto frame=target;frame.inner=(frame.inner+shift)*zoom;frame.radius=(frame.radius+shift)*zoom;const float radius=std::hypot(target.label.x-target.center.x,target.label.y-target.center.y),labelRadius=(radius+shift)*zoom;frame.label={target.center.x+static_cast<float>(std::cos(target.angle))*labelRadius,target.center.y+static_cast<float>(std::sin(target.angle))*labelRadius};return frame;}
inline radial::Sector ringFrame(const radial::Sector& target,float progress){
    return scaledRing(target,.94f+.06f*progress);
}
}
