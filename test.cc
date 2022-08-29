#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstdarg>

#define OLC_PGE_APPLICATION
#include "olcPixelGameEngine.h"

#define DRIP_MIN  (10)
#define DRIP_MAX  (27)

/* 
 * Drop absorbption delay - change this to represent different materials
 */
#define SLOW_DROP   (2.5)
#define FAST_DROP   ((SLOW_DROP)/8.0)

#define SNELL_EFFECT    (0.65)

void trace (const char *fmt, ...) {
    va_list va;
    va_start (va, fmt);
    vfprintf (stderr, fmt, va);
    va_end (va);
}

struct HSL {
    float h, s, l;
};

float Distance (int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    return sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
}

struct HSL rgb2hsl(const olc::Pixel& px){
    float r = px.r / 255.0, g = px.g / 255.0, b = px.b / 255.0;
    float cmax = std::max(r, std::max(g, b)), 
          cmin = std::min(r, std::min(g, b));
    float h = 0.0, s = 0.0, l = (cmax + cmin) / 2.0;

    if(cmax == cmin){
        h = s = 0; // achromatic
    }else{
        float d = cmax - cmin;
        s = l > 0.5 ? d / (2 - cmax - cmin) : d / (cmax + cmin);
        if (cmax == r) {
            h = (g - b) / d + (g < b ? 6.0 : 0.0);
        } else if (cmax == g) {
            h = (b - r) / d + 2.0;
        } else if (cmax == b) {
            h = (r - g) / d + 4.0;
        }
        h /= 6.0;
    }

    return {h, s, l};
}

float hue2rgb (float p, float q, float t) {
    if (t < 0.0) t += 1;
    if (t > 1.0) t -= 1;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

olc::Pixel hsl2rgb(const HSL& hsl) {
    float h = hsl.h, s = hsl.s, l = hsl.l;
    float r = 0.0, g = 0.0, b = 0.0;

    if (s == 0.0) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        float p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0/3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0/3.0);
    }

    return olc::Pixel(roundf(r * 255), roundf(g * 255), roundf(b * 255));
}

olc::Pixel hsl2rgba(const HSL& hsl, float alpha) {
    float h = hsl.h, s = hsl.s, l = hsl.l;
    float r = 0.0, g = 0.0, b = 0.0;

    if (s == 0.0) {
        r = g = b = l; // achromatic
    } else {
        float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
        float p = 2 * l - q;
        r = hue2rgb(p, q, h + 1.0/3.0);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1.0/3.0);
    }

    return olc::Pixel(roundf(r * 255), roundf(g * 255), roundf(b * 255), roundf(alpha * 255));
}

struct RainState {

    RainState () : image_path_(""), orig_(NULL), mod_(NULL) {}
    RainState (const std::string& path) : image_path_(path) {
        orig_ = std::make_shared< olc::Sprite >(image_path_.c_str ());
        mod_ = std::make_shared< olc::Sprite >(image_path_.c_str ());
        wet_.resize (orig_->width * orig_->height, false);
    };

    const olc::Sprite* OriginalImage () const { return orig_.get(); }
    olc::Sprite* VisibleImage () const { return mod_.get(); }

    bool IsWet (int32_t w, int32_t h) const { 
        if (h >= orig_->height || w >= orig_->width) { return true; }
        return wet_[h * orig_->width + w]; 
    }
    void MakeWet (int32_t w, int32_t h) { 
        if (h >= orig_->height || w >= orig_->width) { return; }
        wet_[h * orig_->width + w] = true; 
    }

private:

    std::string image_path_;
    std::shared_ptr< olc::Sprite > orig_;
    std::shared_ptr< olc::Sprite > mod_;
    std::vector< bool > wet_; 
};

struct Drop {

    int32_t w, h;
    float radius;
    float duration, dt;
    bool absorbed;

    Drop (int32_t x, int32_t y, float r, float dur) : 
        w(x), h(y), radius(r),
        duration(dur), dt(0.0), absorbed(false) { }

    /* 
     * Drop starts by reflecting more light and 'absorbs' into the 
     * image to reflect less over time
     *
     * This function returns true once the drop is completely absorbed
     * into the image
     */
    bool Step (float elapsed, RainState& state) {

        if (absorbed) { return true; }

        radius += 0.02;
        radius = std::min (radius, static_cast< float >(std::min (w - 1, h - 1)));

        /*
         * Setting the visible area is a rough approximation of 
         * Snell's Law. Not particularly realistic, but enough to get
         * testing underway
         */

        /* Map [SNELL_EFFECT -> 1.0] visibility range to total time remain */
        float drift = dt * ((1.0 - SNELL_EFFECT) / duration);
        float visible = (SNELL_EFFECT + drift) * radius;
        int32_t shine_x = w + radius / 4, shine_y = h - radius / 4;
        float shine_thresh = radius / 6.0;
        float complete = dt / duration;

        if (dt >= duration) { absorbed = true; }

        for (int32_t x = w - radius; x < w + radius; ++x) {
            for (int32_t y = h - radius; y < h + radius; ++y) {
                float d = Distance (w, h, x, y);
                if (d <= radius) {
                    if (absorbed) {
                        /* TODO: This is too hard a snap from 1.0 -> 0.85
                         * Need a new state or way to transition to this darkened
                         * color gradually
                         * NOTE: This should also then account for the perlin 
                         * noise effect of non-uniform outer circle edge
                         */
                        HSL hsl = rgb2hsl (state.OriginalImage ()->GetPixel (x, y));
                        hsl.l *= 0.85;
                        state.VisibleImage ()->SetPixel(x, y, hsl2rgb (hsl));
                    } else {
                        /* Get angle */
                        float theta = atan2(y - h, x - w);
                        float mag = visible * (d / radius); 
                        int32_t vx = w + mag * cos (theta);
                        int32_t vy = h + mag * sin (theta);
                        HSL hsl = rgb2hsl (state.OriginalImage ()->GetPixel (vx, vy));

                        // if within the 'reflection' area, raise liminosity
                        if (Distance (shine_x, shine_y, x, y) <= shine_thresh) {
                            /* For naturally lighter areas, this is a little heavy */
                            if (hsl.l < 0.8) {
                                hsl.l = std::min (1.0, hsl.l * (1.0 + (0.45 * (1 - complete))));
                            } else {
                                hsl.l = std::min (1.0, hsl.l * (1.0 + (0.1 * (1 - complete))));
                            }
                        } else {
                            hsl.l = std::min (1.0, hsl.l * (1.0 + (0.25 * (1 - complete))));
                        }
                    
                        if (d >= 0.7 * radius) { hsl.l = std::min (1.0, hsl.l * 1.025); }
                        if (d >= 0.8 * radius) { hsl.l = std::min (1.0, hsl.l * 1.025); }
                        if (d >= 0.9 * radius) { hsl.l = std::min (1.0, hsl.l * 1.025); }
                        if (d >= 0.95 * radius) { hsl.l = std::min (1.0, hsl.l * 1.025); }
                        state.VisibleImage ()->SetPixel(x, y, hsl2rgb (hsl));
                    }
                    state.MakeWet (x, y);
                }
            }
        }
        dt += elapsed;
        return false;
    }
};

class Weather : public olc::PixelGameEngine
{

    RainState state_;
    bool mod_;
    std::vector< Drop > drops;

    int32_t RandRange (int32_t low, int32_t high) {
        int32_t x = rand () % (high - low);
        return low + x;
    }

    Drop AddDrop (int32_t radius) {
        int32_t w = RandRange(0, ScreenWidth () - 1),
                h = RandRange(0, ScreenHeight () - 1);
        return AddDrop(w, h, radius);
    }

    Drop AddDrop (int32_t w, int32_t h, int32_t radius) {
        /*
         * Two cases: 
         *  1) Adding drop on an already absorbed location (or some portion of it)
         *  2) this is a previously untouched location
         *
         *  Case 1 requires an accellerated absorbtion rate as the tensile
         *  strength of the medium is much lower due to existing moisture.
         *
         *  Case 2 will absorb much slower
         */

        /* Clip radius to image bounds */
        radius = std::min (std::min (w - 1, h - 1), radius);

        trace ("Add Drop @ %d,%d\n", w, h);

        mod_ = true;
        /* Case 1 */
        /* TODO: Only loop over points in the circle */
        for (int32_t X = w - radius; X < w + radius; ++X) {
            for (int32_t Y = h - radius; Y < h + radius; ++Y) {
                if (Distance (w, h, X, Y) <= radius) {
                    if (state_.IsWet (X, Y)) {
                        return Drop (w, h, radius, FAST_DROP);
                    }
                }
            }
        }

        return Drop (w, h, radius, SLOW_DROP);
    }


public:
	Weather() {
		sAppName = "Weather";
        mod_ = false;
	}

    void Initialize(const std::string &image) {
        state_ = RainState(image);
    }

    int32_t Width () const { return state_.OriginalImage ()->width; }
    int32_t Height () const { return state_.OriginalImage ()->height; }

public:
	bool OnUserCreate() override {
        DrawSprite({0, 0}, state_.VisibleImage ());
		return true;
	}

	bool OnUserUpdate(float fElapsedTime) override {
        if (GetKey(olc::ESCAPE).bPressed) {
            return false;
        }
        if (GetKey(olc::D).bPressed) {
            drops.push_back (AddDrop (RandRange (DRIP_MIN, DRIP_MAX)));
        }
        if (! drops.empty ()) {
            std::vector< Drop >::iterator IT = drops.begin ();
            while (IT != drops.end ()) {
                if (IT->Step (fElapsedTime, state_)) {
                    IT = drops.erase (IT);
                } else {
                    ++IT;
                }
            }
            mod_ = true;
        }
        if (mod_) {
            Clear (olc::BLACK);
            DrawSprite({0, 0}, state_.VisibleImage ());
            mod_ = false;
        }
		return true;
	}
};


int main(int argc, char **argv)
{
	Weather demo;
    //if (1 == argc) {
        demo.Initialize("images/sunset.png");
    //} else {
        //demo.Initialize(argv[1]);
    //}
	if (demo.Construct(demo.Width (), demo.Height (), 1, 1))
		demo.Start();

	return 0;
}
