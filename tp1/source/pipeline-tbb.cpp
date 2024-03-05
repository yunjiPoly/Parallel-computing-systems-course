#include <stdio.h>
#include <tbb/pipeline.h>

extern "C" {
#include "filter.h"
#include "pipeline.h"
}

class LoadImage {
public:
    LoadImage( image_dir_t* image_dir );
    LoadImage( const LoadImage& f ) : image_dir(f.image_dir) { }
    ~LoadImage();
    image_t* operator()( tbb::flow_control& fc ) const;
private:
    image_dir_t* image_dir;
};


LoadImage::LoadImage( image_dir_t* image_dir_ ) :
image_dir(image_dir_) { }


LoadImage::~LoadImage() {
}


image_t* LoadImage::operator()( tbb::flow_control& fc ) const {
    image_t* image = image_dir_load_next(image_dir);
	if(image == NULL){
		fc.stop();
	}
    return image;
}

class ScaleImage {
public:
    image_t* operator()( image_t* image ) const;
};


image_t* ScaleImage::operator()( image_t* image ) const {
	return filter_scale_up(image, 3);
}

class VerticalFlipImage {
public:
    image_t* operator()( image_t* image ) const;
};


image_t* VerticalFlipImage::operator()( image_t* image ) const {
	return filter_vertical_flip(image);
}

class SaveImage {
    image_dir_t* image_dir;
public:
    SaveImage( image_dir_t* image_dir );
    void operator()( image_t* image ) const;
};


SaveImage::SaveImage( image_dir_t* image_dir_) :
    image_dir(image_dir_)
{}


void SaveImage::operator()( image_t* image ) const {
	image_dir_save(image_dir, image);
    printf(".");
    fflush(stdout);
    image_destroy(image);
}

int pipeline_tbb(image_dir_t* image_dir) {
    tbb::parallel_pipeline(
        96,
        tbb::make_filter<void, image_t*>(tbb::filter::serial_in_order, LoadImage(image_dir)) &
        tbb::make_filter<image_t*, image_t*>(tbb::filter::parallel, ScaleImage()) &
        tbb::make_filter<image_t*, image_t*>(tbb::filter::parallel, VerticalFlipImage()) &
        tbb::make_filter<image_t*, void>(tbb::filter::parallel, SaveImage(image_dir))
    );
    return 0;
}
