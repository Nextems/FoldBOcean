#pragma once

// One premultiplied DIB shared by GDI+ and UpdateLayeredWindow. Keep it only
// while the panel is visible, rather than allocate and copy two full frames.
class LayeredSurface {
    HDC dc_{};
    HBITMAP dib_{};
    HGDIOBJ previous_{};
    int width_=0,height_=0;
    std::unique_ptr<Gdiplus::Bitmap> image_;
public:
    LayeredSurface()=default;
    LayeredSurface(const LayeredSurface&)=delete;
    LayeredSurface& operator=(const LayeredSurface&)=delete;
    ~LayeredSurface(){reset();}
    void reset(){
        image_.reset();
        if(dc_&&previous_)SelectObject(dc_,previous_);
        if(dib_)DeleteObject(dib_);
        if(dc_)DeleteDC(dc_);
        dc_=nullptr;dib_=nullptr;previous_=nullptr;width_=height_=0;
    }
    bool ensure(int width,int height){
        if(image_&&width_==width&&height_==height)return true;
        reset();if(width<=0||height<=0)return false;
        dc_=CreateCompatibleDC(nullptr);if(!dc_)return false;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels{};dib_=CreateDIBSection(dc_,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!dib_||!pixels){reset();return false;}
        previous_=SelectObject(dc_,dib_);
        image_=std::make_unique<Gdiplus::Bitmap>(width,height,width*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels));
        if(image_->GetLastStatus()!=Gdiplus::Ok){reset();return false;}
        width_=width;height_=height;return true;
    }
    Gdiplus::Bitmap* image()const{return image_.get();}
    HDC dc()const{return dc_;}
    bool present(HWND window)const{
        if(!image_)return false;
        SIZE size{width_,height_};POINT source{};BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        return UpdateLayeredWindow(window,nullptr,nullptr,&size,dc_,&source,0,&blend,ULW_ALPHA)!=FALSE;
    }
};
