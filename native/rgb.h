#pragma once
bool rgbEnabled(const char* key){return configSection("rgb").value(key,false);}
bool rgbActive(){return webView()?(rgbEnabled("blocks")||rgbEnabled("branches")):rgbEnabled("radial");}
Color rgbColorAt(const std::string& id,double milliseconds,double speed,BYTE alpha=255){
    unsigned int hash=2166136261u;for(const unsigned char c:id)hash=(hash^c)*16777619u;
    const double hue=std::fmod(milliseconds*.024*speed+hash%360,360.0),x=1-std::abs(std::fmod(hue/60,2.0)-1);double r=0,g=0,b=0;
    if(hue<60){r=1;g=x;}else if(hue<120){r=x;g=1;}else if(hue<180){g=1;b=x;}else if(hue<240){g=x;b=1;}else if(hue<300){r=x;b=1;}else{r=1;b=x;}
    return Color(alpha,static_cast<BYTE>(std::round((.35+.65*r)*255)),static_cast<BYTE>(std::round((.35+.65*g)*255)),static_cast<BYTE>(std::round((.35+.65*b)*255)));
}
Color animatedTint(const char* key,const std::string& id,Color fallback){if(!rgbEnabled(key))return fallback;return rgbColorAt(id,static_cast<double>(GetTickCount64()),configSection("rgb").value("speed",1.0),fallback.GetA());}
