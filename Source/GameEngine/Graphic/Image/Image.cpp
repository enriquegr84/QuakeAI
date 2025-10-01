// Copyright (C) 2002-2012 Nikolaus Gebhardt / Thomas Alten
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "Image.h"

// Blitter Operation
enum BlitterOperation
{
    BLITTER_INVALID = 0,
    BLITTER_COLOR,
    BLITTER_COLOR_ALPHA,
    BLITTER_TEXTURE,
    BLITTER_TEXTURE_ALPHA_BLEND,
    BLITTER_TEXTURE_ALPHA_COLOR_BLEND,
    BLITTER_TEXTURE_COMBINE_ALPHA
};

struct AbsRectangle
{
    int x0;
    int y0;
    int x1;
    int y1;
};


//! 2D Intersection test
inline bool Intersect(AbsRectangle& dest, const AbsRectangle& a, const AbsRectangle& b)
{
    dest.x0 = std::max(a.x0, b.x0);
    dest.y0 = std::max(a.y0, b.y0);
    dest.x1 = std::min(a.x1, b.x1);
    dest.y1 = std::min(a.y1, b.y1);
    return dest.x0 < dest.x1 && dest.y0 < dest.y1;
}

struct BlitJob
{
    AbsRectangle destiny;
    AbsRectangle source;

    unsigned int argb;

    void* src;
    void* dst;

    int width;
    int height;

    unsigned int srcPitch;
    unsigned int dstPitch;

    unsigned int srcPixelMul;
    unsigned int dstPixelMul;

    bool stretch;
    float xStretch;
    float yStretch;

    BlitJob() : stretch(false) {}
};
typedef void(*ExecuteBlit) (const BlitJob* job);

struct BlitterTable
{
    BlitterOperation operation;
    int destFormat;
    int sourceFormat;
    ExecuteBlit func;
};

//! a more useful memset for pixel
inline void Memset32(void* dest, const uint32_t value, uint32_t bytesize)
{
    uint32_t* d = (uint32_t*)dest;

    uint32_t i;

    // loops unrolled to reduce the number of increments by factor ~8.
    i = bytesize >> (2 + 3);
    while (i)
    {
        d[0] = value;
        d[1] = value;
        d[2] = value;
        d[3] = value;

        d[4] = value;
        d[5] = value;
        d[6] = value;
        d[7] = value;

        d += 8;
        i -= 1;
    }

    i = (bytesize >> 2) & 7;
    while (i)
    {
        d[0] = value;
        d += 1;
        i -= 1;
    }
}

/*
    return alpha in [0;256] Granularity from 32-Bit ARGB
    add highbit alpha ( alpha > 127 ? + 1 )
*/
inline uint32_t ExtractAlpha(const uint32_t c)
{
    return (c >> 24) + (c >> 31);
}

/*!
    Pixel = dest * ( 1 - alpha ) + source * alpha
    alpha [0;256]
*/
inline uint32_t PixelBlend32(const uint32_t c2, const uint32_t c1, uint32_t alpha)
{
    uint32_t srcRB = c1 & 0x00FF00FF;
    uint32_t srcXG = c1 & 0x0000FF00;

    uint32_t dstRB = c2 & 0x00FF00FF;
    uint32_t dstXG = c2 & 0x0000FF00;

    uint32_t rb = srcRB - dstRB;
    uint32_t xg = srcXG - dstXG;

    rb *= alpha;
    xg *= alpha;
    rb >>= 8;
    xg >>= 8;

    rb += dstRB;
    xg += dstXG;

    rb &= 0x00FF00FF;
    xg &= 0x0000FF00;

    return rb | xg;
}

/*!
    Pixel = dest * ( 1 - SourceAlpha ) + source * SourceAlpha
*/
inline uint32_t PixelBlend32(const uint32_t c2, const uint32_t c1)
{
    // alpha test
    uint32_t alpha = c1 & 0xFF000000;

    if (0 == alpha)
        return c2;

    if (0xFF000000 == alpha)
    {
        return c1;
    }

    alpha >>= 24;

    // add highbit alpha, if ( alpha > 127 ) alpha += 1;
    alpha += (alpha >> 7);

    uint32_t srcRB = c1 & 0x00FF00FF;
    uint32_t srcXG = c1 & 0x0000FF00;

    uint32_t dstRB = c2 & 0x00FF00FF;
    uint32_t dstXG = c2 & 0x0000FF00;

    uint32_t rb = srcRB - dstRB;
    uint32_t xg = srcXG - dstXG;

    rb *= alpha;
    xg *= alpha;
    rb >>= 8;
    xg >>= 8;

    rb += dstRB;
    xg += dstXG;

    rb &= 0x00FF00FF;
    xg &= 0x0000FF00;

    return (c1 & 0xFF000000) | rb | xg;
}

/*!
    Pixel =>
            color = dest * ( 1 - SourceAlpha ) + source * SourceAlpha,
            alpha = destAlpha * ( 1 - SourceAlpha ) + sourceAlpha

    where "1" means "full scale" (255)
*/
inline uint32_t PixelCombine32(const uint32_t c2, const uint32_t c1)
{
    // alpha test
    uint32_t alpha = c1 & 0xFF000000;

    if (0 == alpha)
        return c2;

    if (0xFF000000 == alpha)
    {
        return c1;
    }

    alpha >>= 24;

    // add highbit alpha, if ( alpha > 127 ) alpha += 1;
    // stretches [0;255] to [0;256] to avoid division by 255. use division 256 == shr 8
    alpha += (alpha >> 7);

    uint32_t srcRB = c1 & 0x00FF00FF;
    uint32_t srcXG = c1 & 0x0000FF00;

    uint32_t dstRB = c2 & 0x00FF00FF;
    uint32_t dstXG = c2 & 0x0000FF00;

    uint32_t rb = srcRB - dstRB;
    uint32_t xg = srcXG - dstXG;

    rb *= alpha;
    xg *= alpha;
    rb >>= 8;
    xg >>= 8;

    rb += dstRB;
    xg += dstXG;

    rb &= 0x00FF00FF;
    xg &= 0x0000FF00;

    uint32_t sa = c1 >> 24;
    uint32_t da = c2 >> 24;
    uint32_t blendAlphaFix8 = (sa * 256 + da * (256 - alpha)) >> 8;
    return blendAlphaFix8 | rb | xg ;
}

/*
    Pixel = c0 * (c1/255). c0 Alpha Retain
*/
inline uint32_t PixelMul32(const uint32_t c0, const uint32_t c1)
{
    return	(c0 & 0xFF000000) |
        ((((c0 & 0x00FF0000) >> 12) * ((c1 & 0x00FF0000) >> 12)) & 0x00FF0000) |
        ((((c0 & 0x0000FF00) * (c1 & 0x0000FF00)) >> 16) & 0x0000FF00) |
        ((((c0 & 0x000000FF) * (c1 & 0x000000FF)) >> 8) & 0x000000FF);
}

/*
    Pixel = c0 * (c1/255).
*/
inline uint32_t PixelMul32_2(const uint32_t c0, const uint32_t c1)
{
    return	((((c0 & 0xFF000000) >> 16) * ((c1 & 0xFF000000) >> 16)) & 0xFF000000) |
        ((((c0 & 0x00FF0000) >> 12) * ((c1 & 0x00FF0000) >> 12)) & 0x00FF0000) |
        ((((c0 & 0x0000FF00) * (c1 & 0x0000FF00)) >> 16) & 0x0000FF00) |
        ((((c0 & 0x000000FF) * (c1 & 0x000000FF)) >> 8) & 0x000000FF);
}

static void ExecuteBlitTextureCopyXToX(const BlitJob* job)
{
    if (job->stretch)
    {
        const int wscale = ((int)floorf(((job->xStretch) * 262144.f) + 0.f));
        const int hscale = ((int)floorf(((job->yStretch) * 262144.f) + 0.f));

        int srcY = 0;

        if (job->srcPixelMul == 4)
        {
            const uint32_t* src = (uint32_t*)(job->src);
            uint32_t* dst = (uint32_t*)(job->dst);

            for (uint32_t dy = 0; dy < (uint32_t)job->height; ++dy, srcY += hscale)
            {
                src = (uint32_t*)((uint8_t*)(job->src) + job->srcPitch * ((srcY) >> 18));

                int srcX = 0;
                for (uint32_t dx = 0; dx < (uint32_t)job->width; ++dx, srcX += wscale)
                    dst[dx] = src[((srcX) >> 18)];
                dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
            }
        }
        else if (job->srcPixelMul == 2)
        {
            const uint16_t* src = (uint16_t*)(job->src);
            uint16_t* dst = (uint16_t*)(job->dst);

            for (uint32_t dy = 0; dy < (uint32_t)job->height; ++dy, srcY += hscale)
            {
                src = (uint16_t*)((uint8_t*)(job->src) + job->srcPitch * ((srcY) >> 18));

                int srcX = 0;
                for (uint32_t dx = 0; dx < (uint32_t)job->width; ++dx, srcX += wscale)
                    dst[dx] = src[((srcX) >> 18)];
                dst = (uint16_t*)((uint8_t*)(dst)+job->dstPitch);
            }
        }
    }
    else
    {
        const size_t widthPitch = job->width * job->dstPixelMul;
        const void* src = (void*)job->src;
        void* dst = (void*)job->dst;

        for (uint32_t dy = 0; dy < (uint32_t)job->height; ++dy)
        {
            memcpy(dst, src, widthPitch);

            src = (void*)((uint8_t*)(src)+job->srcPitch);
            dst = (void*)((uint8_t*)(dst)+job->dstPitch);
        }
    }
}

static void ExecuteBlitTextureBlend32To32(const BlitJob* job)
{
    const int wscale = ((int)floorf(((job->xStretch) * 262144.f) + 0.f));
    const int hscale = ((int)floorf(((job->yStretch) * 262144.f) + 0.f));

    int srcY = 0;

    const uint32_t* src = (uint32_t*)(job->src);
    uint32_t* dst = (uint32_t*)(job->dst);

    for (uint32_t dy = 0; dy < (uint32_t)job->height; ++dy, srcY += hscale)
    {
        src = (uint32_t*)((uint8_t*)(job->src) + job->srcPitch * ((srcY) >> 18));

        int srcX = 0;
        for (uint32_t dx = 0; dx < (uint32_t)job->width; ++dx, srcX += wscale)
            dst[dx] = PixelBlend32(dst[dx], src[((srcX) >> 18)]);
        dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
    }
}

static void ExecuteBlitTextureBlendColor32To32(const BlitJob* job)
{
    const int wscale = ((int)floorf(((job->xStretch) * 262144.f) + 0.f));
    const int hscale = ((int)floorf(((job->yStretch) * 262144.f) + 0.f));

    int srcY = 0;

    const uint32_t* src = (uint32_t*)(job->src);
    uint32_t* dst = (uint32_t*)(job->dst);

    for (uint32_t dy = 0; dy < (uint32_t)job->height; ++dy, srcY += hscale)
    {
        src = (uint32_t*)((uint8_t*)(job->src) + job->srcPitch * ((srcY) >> 18));

        int srcX = 0;
        for (uint32_t dx = 0; dx < (uint32_t)job->width; ++dx, srcX += wscale)
            dst[dx] = PixelBlend32(dst[dx], PixelMul32_2(src[((srcX) >> 18)], job->argb));
        dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
    }
}

static void ExecuteBlitColor32To32(const BlitJob * job)
{
    uint32_t* dst = (uint32_t*)job->dst;

    for (uint32_t dy = 0; dy != job->height; ++dy)
    {
        Memset32(dst, job->argb, job->srcPitch);
        dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
    }
}

static void ExecuteBlitColorAlpha32To32(const BlitJob* job)
{
    CONST uint32_t alpha = ExtractAlpha(job->argb);

    uint32_t* dst = (uint32_t*)job->dst;
    for (uint32_t dy = 0; dy != job->height; ++dy)
    {
        for (uint32_t dx = 0; dx != job->width; ++dx)
            dst[dx] = PixelBlend32(dst[dx], job->argb, alpha);

        dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
    }
}

/*!
    Combine alpha channels (increases alpha / reduces transparency)
*/
static void ExecuteBlitTextureCombineColor32To32(const BlitJob* job)
{
    uint32_t* src = (uint32_t*)(job->src);
    uint32_t* dst = (uint32_t*)(job->dst);

    for (uint32_t dy = 0; dy != (uint32_t)job->height; ++dy)
    {
        for (uint32_t dx = 0; dx != (uint32_t)job->width; ++dx)
            dst[dx] = PixelCombine32(dst[dx], PixelMul32_2(src[dx], job->argb));

        src = (uint32_t*)((uint8_t*)(src)+job->srcPitch);
        dst = (uint32_t*)((uint8_t*)(dst)+job->dstPitch);
    }
}


static const BlitterTable BlitTable[] =
{
    { BLITTER_TEXTURE, -2, -2, ExecuteBlitTextureCopyXToX },
    { BLITTER_TEXTURE_ALPHA_BLEND, DF_R8G8B8A8_UNORM, DF_R8G8B8A8_UNORM, ExecuteBlitTextureBlend32To32 },
    { BLITTER_TEXTURE_ALPHA_COLOR_BLEND, DF_R8G8B8A8_UNORM, DF_R8G8B8A8_UNORM, ExecuteBlitTextureBlendColor32To32 },
    { BLITTER_COLOR, DF_R8G8B8A8_UNORM, DF_UNKNOWN, ExecuteBlitColor32To32 },
    { BLITTER_COLOR_ALPHA, DF_R8G8B8A8_UNORM, DF_UNKNOWN, ExecuteBlitColorAlpha32To32 },
    { BLITTER_TEXTURE_COMBINE_ALPHA, DF_R8G8B8A8_UNORM, DF_R8G8B8A8_UNORM, ExecuteBlitTextureCombineColor32To32 },
    { BLITTER_INVALID, -1, -1, 0 }
};

static inline ExecuteBlit GetBlitter2(BlitterOperation operation, 
    const std::shared_ptr<Texture2> dest, const std::shared_ptr<Texture2> src)
{
    DFType sourceFormat = src ? src->GetFormat() : DF_UNKNOWN;
    DFType destFormat = dest ? dest->GetFormat() : DF_UNKNOWN;

    const BlitterTable* b = BlitTable;

    while (b->operation != BLITTER_INVALID)
    {
        if (b->operation == operation)
        {
            if ((b->destFormat == -1 || b->destFormat == destFormat) &&
                (b->sourceFormat == -1 || b->sourceFormat == sourceFormat))
                return b->func;
            else
                if (b->destFormat == -2 && (sourceFormat == destFormat))
                    return b->func;
        }
        b += 1;
    }
    return 0;
}


// bounce clipping to texture
inline void SetClip(AbsRectangle& out, const RectangleShape<2, int>* clip,
    const std::shared_ptr<Texture2> tex, int32_t passnative)
{
    if (clip && 0 == tex && passnative)
    {
        out.x0 = clip->GetVertice(RVP_UPPERLEFT)[0];
        out.x1 = clip->GetVertice(RVP_LOWERRIGHT)[0];
        out.y0 = clip->GetVertice(RVP_UPPERLEFT)[1];
        out.y1 = clip->GetVertice(RVP_LOWERRIGHT)[1];
        return;
    }

    const int32_t w = tex ? tex->GetDimension(0) : 0;
    const int32_t h = tex ? tex->GetDimension(1) : 0;
    if (clip)
    {
        out.x0 = std::clamp(clip->GetVertice(RVP_UPPERLEFT)[0], 0, w);
        out.x1 = std::clamp(clip->GetVertice(RVP_LOWERRIGHT)[0], out.x0, w);
        out.y0 = std::clamp(clip->GetVertice(RVP_UPPERLEFT)[1], 0, h);
        out.y1 = std::clamp(clip->GetVertice(RVP_LOWERRIGHT)[1], out.y0, h);
    }
    else
    {
        out.x0 = 0;
        out.y0 = 0;
        out.x1 = w;
        out.y1 = h;
    }
}


/*!
    a generic 2D Blitter
*/
static int Blit(BlitterOperation operation,
    std::shared_ptr<Texture2> dest, const RectangleShape<2, int>* destClipping,
    const Vector2<int>* destPos, std::shared_ptr<Texture2> const src,
    const RectangleShape<2, int>* srcClipping, unsigned int argb)
{
    ExecuteBlit blitter = GetBlitter2(operation, dest, src);
    if (0 == blitter)
    {
        return 0;
    }

    // Clipping
    AbsRectangle srcClip;
    AbsRectangle destClip;
    AbsRectangle v;

    BlitJob job;

    SetClip(srcClip, srcClipping, src, 1);
    SetClip(destClip, destClipping, dest, 0);

    v.x0 = destPos ? (*destPos)[0] : 0;
    v.y0 = destPos ? (*destPos)[1] : 0;
    v.x1 = v.x0 + (srcClip.x1 - srcClip.x0);
    v.y1 = v.y0 + (srcClip.y1 - srcClip.y0);

    if (!Intersect(job.destiny, destClip, v))
        return 0;

    job.width = job.destiny.x1 - job.destiny.x0;
    job.height = job.destiny.y1 - job.destiny.y0;

    job.source.x0 = srcClip.x0 + (job.destiny.x0 - v.x0);
    job.source.x1 = job.source.x0 + job.width;
    job.source.y0 = srcClip.y0 + (job.destiny.y0 - v.y0);
    job.source.y1 = job.source.y0 + job.height;

    job.argb = argb;

    job.stretch = false;
    job.xStretch = 1.f;
    job.yStretch = 1.f;

    if (src)
    {
        unsigned int bpp = DataFormat::GetNumBytesPerStruct(src->GetFormat());
        job.srcPitch = src->GetWidth() * bpp;
        job.srcPixelMul = bpp;
        job.src = (void*)((uint8_t*)src->Get<BYTE>() +
            (job.source.y0 * job.srcPitch) + (job.source.x0 * job.srcPixelMul));
    }
    else
    {
        // use srcPitch for color operation on dest
        unsigned int bpp = DataFormat::GetNumBytesPerStruct(dest->GetFormat());
        job.srcPitch = job.width * bpp;
    }

    unsigned int bpp = DataFormat::GetNumBytesPerStruct(dest->GetFormat());
    job.dstPitch = dest->GetWidth() * bpp;
    job.dstPixelMul = bpp;
    job.dst = (void*)((uint8_t*)dest->Get<BYTE>() +
        (job.destiny.y0 * job.dstPitch) + (job.destiny.x0 * job.dstPixelMul));

    blitter(&job);
    return 1;
}


//! copies this surface into another at given position
void Image::CopyTo(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source, const Vector2<int>& pos)
{
    // Copy the pixels from the decoder to the texture.
    Blit(BLITTER_TEXTURE, target, 0, &pos, source, 0, 0);
}

//! copies this surface into another at given position
void Image::CopyTo(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source,
    const Vector2<int>& pos, const RectangleShape<2, int>& sourceRect, const RectangleShape<2, int>* clipRect)
{
    // Copy the pixels from the decoder to the texture.
    Blit(BLITTER_TEXTURE, target, clipRect, &pos, source, &sourceRect, 0);
}

//! copies this surface into another, using the alpha mask, an cliprect and a color to add with
void Image::CopyToWithAlpha(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source,
    const Vector2<int>& pos, const RectangleShape<2, int>& sourceRect, const SColor& color,
    const RectangleShape<2, int>* clipRect, bool combineAlpha)
{
    uint32_t argb;
    color.GetData(&argb);

    BlitterOperation op = combineAlpha ? BLITTER_TEXTURE_COMBINE_ALPHA :
        argb == 0xFFFFFFFF ? BLITTER_TEXTURE_ALPHA_BLEND : BLITTER_TEXTURE_ALPHA_COLOR_BLEND;
    Blit(op, target, clipRect, &pos, source, &sourceRect, argb);
}

//! copies this surface into another, scaling it to the target image size
// note: this is very very slow.
void Image::CopyToScaling(std::shared_ptr<Texture2> target, std::shared_ptr<Texture2> source)
{
	if (!target)
		return;

	if (target->GetWidth() == source->GetWidth() && target->GetHeight() == source->GetHeight())
	{
		CopyTo(target, source);
		return;
	}

    unsigned int width = target->GetWidth();
    unsigned int height = target->GetHeight();
    DFType format = target->GetFormat();
    if (!target || !width || !height)
        return;

    unsigned int bpp = DataFormat::GetNumBytesPerStruct(format);
    unsigned int pitch = width * bpp;

    unsigned int sourceBpp = DataFormat::GetNumBytesPerStruct(source->GetFormat());
    unsigned int sourcePitch = source->GetWidth() * sourceBpp;

    if (source->GetFormat() == format && source->GetWidth() == width && source->GetHeight() == height)
    {
        if (pitch == sourcePitch)
        {
            // Copy the pixels from the decoder to the texture.
            std::memcpy(target->Get<BYTE>(), source->Get<BYTE>(), target->GetNumBytes());
            return;
        }
        else
        {
            char* tgtpos = target->GetData();
            char* srcpos = source->GetData();
            const unsigned int bwidth = width * bpp;
            const unsigned int rest = pitch - bwidth;
            for (unsigned int y = 0; y < height; ++y)
            {
                // copy scanline
                memcpy(tgtpos, srcpos, bwidth);
                // clear pitch
                memset(tgtpos + bwidth, 0, rest);
                tgtpos += pitch;
                srcpos += sourcePitch;
            }
            return;
        }
    }

    // NOTE: Scaling is coded to keep the border pixels intact.
    // Alternatively we could for example work with first pixel being taken at half step-size.
    // Then we have one more step here and it would be:
    //     sourceXStep = (float)(Size.Width-1) / (float)(width);
    //     And sx would start at 0.5f + sourceXStep / 2.f;
    //     Similar for y.
    // As scaling is done without any antialiasing it doesn't matter too much which outermost pixels we use and keeping
    // border pixels intact is probably mostly better (with AA the other solution would be more correct).
    // This is however unnecessary (and unexpected) for scaling to integer multiples, so don't do it there.
    float sourceXStep, sourceYStep;
    float sourceXStart = 0.f, sourceYStart = 0.f;
    if (width % source->GetWidth() == 0)
        sourceXStep = (float)source->GetWidth() / (float)width;
    else
    {
        sourceXStep = width > 1 ? (float)(source->GetWidth() - 1) / (float)(width - 1) : 0.f;
        sourceXStart = 0.5f;	// for rounding to nearest pixel
    }
    if (height % source->GetHeight() == 0)
        sourceYStep = (float)source->GetHeight() / (float)height;
    else
    {
        sourceYStep = height > 1 ? (float)(source->GetHeight() - 1) / (float)(height - 1) : 0.f;
        sourceYStart = 0.5f;	// for rounding to nearest pixel
    }

    int yval = 0, syval = 0;
    float sy = sourceYStart;
    for (unsigned int y = 0; y < height; ++y)
    {
        float sx = sourceXStart;
        for (unsigned int x = 0; x < width; ++x)
        {
            //DF_R8G8B8A8_UNORM
            memcpy(target->Get<BYTE>() + yval + (x*bpp), source->Get<BYTE>() + syval + ((int)sx)*sourceBpp, 4);
            sx += sourceXStep;
        }
        sy += sourceYStep;
        syval = (unsigned int)(sy)*sourcePitch;
        yval += pitch;
    }
}