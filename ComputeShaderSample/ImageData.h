#ifndef IMAGEDATA_H_
#define IMAGEDATA_H_

#include <string>

class ImageData {
public: 
    int width;
    int height;
    int original_channels;
    int desired_channels;
    int level;
    // in bytes
    int size;
    //pixels
    unsigned char* pixels;
    explicit ImageData();
    explicit ImageData(const std::string& filename);
    explicit ImageData(const ImageData& to_copy);
    ImageData& operator= (const ImageData& rhs);
    bool save(const std::string& filename);
    std::string print() const;
    ~ImageData();
};

#endif // HEADER_H_
