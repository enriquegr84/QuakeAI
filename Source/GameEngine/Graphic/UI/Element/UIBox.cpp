// Copyright (C) 2002-2012 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "UIBox.h"

#include "Graphic/UI/UIEngine.h"

//! constructor
UIBox::UIBox(BaseUI* ui, int id,
    RectangleShape<2, int> rectangle,
    const std::array<SColor, 4>& colors,
    const std::array<SColor, 4>& borderColors,
    const std::array<int, 4>& borderWidths) 
    : BaseUIBox(id, rectangle), mUI(ui),
    mColors(colors), mBorderColors(borderColors), mBorderWidths(borderWidths)
{
    mBlendState = std::make_shared<BlendState>();
    mBlendState->mTarget[0].enable = true;
    mBlendState->mTarget[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->mTarget[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->mTarget[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    // Create a vertex buffer for a single triangle.
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    vformat.Bind(VA_COLOR, DF_R32G32B32A32_FLOAT, 0);

    std::shared_ptr<VertexBuffer> vbuffer = std::make_shared<VertexBuffer>(vformat, 4);
    std::shared_ptr<IndexBuffer> ibuffer = std::make_shared<IndexBuffer>(IP_TRISTRIP, 2);
    vbuffer->SetUsage(Resource::DYNAMIC_UPDATE);

    std::vector<std::string> path;
#if defined(_OPENGL_)
    path.push_back("Effects/ColorEffectVS.glsl");
    path.push_back("Effects/ColorEffectPS.glsl");
#else
    path.push_back("Effects/ColorEffectVS.hlsl");
    path.push_back("Effects/ColorEffectPS.hlsl");
#endif
    std::shared_ptr<ResHandle> resHandle =
        ResCache::Get()->GetHandle(&BaseResource(ToWideString(path.front())));

    const std::shared_ptr<ShaderResourceExtraData>& extra =
        std::static_pointer_cast<ShaderResourceExtraData>(resHandle->GetExtra());
    if (!extra->GetProgram())
        extra->GetProgram() = ProgramFactory::Get()->CreateFromFiles(path.front(), path.back(), "");

    mEffect = std::make_shared<ColorEffect>(
        ProgramFactory::Get()->CreateFromProgram(extra->GetProgram()));

    // Create the geometric object for drawing.
    mVisual = std::make_shared<Visual>(vbuffer, ibuffer, mEffect);
}

void UIBox::Draw( )
{
    if (!IsVisible())
        return;

    std::array<int, 4> negativeBorders = { 0, 0, 0, 0 };
    std::array<int, 4> positiveBorders = { 0, 0, 0, 0 };

    for (size_t i = 0; i <= 3; i++) {
        if (mBorderWidths[i] > 0)
            positiveBorders[i] = mBorderWidths[i];
        else
            negativeBorders[i] = mBorderWidths[i];
    }

    Vector2<int> upperLeft = mAbsoluteRect.GetVertice(RVP_UPPERLEFT);
    Vector2<int> lowerRight = mAbsoluteRect.GetVertice(RVP_LOWERRIGHT);

    Vector2<int> topLeftBorder = {
        upperLeft[0] - positiveBorders[3],
        upperLeft[1] - positiveBorders[0]
    };
    Vector2<int> topLeftRect = {
        upperLeft[0] - negativeBorders[3],
        upperLeft[1] - negativeBorders[0]
    };

    Vector2<int>  lowerRightBorder = {
        lowerRight[0] + positiveBorders[1],
        lowerRight[1] + positiveBorders[2]
    };
    Vector2<int>  lowerRightRect = {
        lowerRight[0] + negativeBorders[1],
        lowerRight[1] + negativeBorders[2]
    };

    RectangleShape<2, int> mainRect;
    mainRect.mExtent = lowerRightRect - topLeftRect;
    mainRect.mCenter = topLeftRect + mainRect.mExtent / 2;

    std::array<RectangleShape<2, int>, 4> borderRects;

    borderRects[0] = RectangleShape<2, int>();
    borderRects[0].mExtent[0] = lowerRightBorder[0] - topLeftBorder[0];
    borderRects[0].mExtent[1] = topLeftRect[1] - topLeftBorder[1];
    borderRects[0].mCenter = topLeftBorder + borderRects[0].mExtent / 2;

    borderRects[1] = RectangleShape<2, int>();
    borderRects[1].mExtent[0] = lowerRightBorder[0] - lowerRightRect[0];
    borderRects[1].mExtent[1] = lowerRightRect[1] - topLeftRect[1];
    borderRects[1].mCenter[0] = lowerRightRect[0] + borderRects[1].mExtent[0] / 2;
    borderRects[1].mCenter[1] = topLeftRect[1] + borderRects[1].mExtent[1] / 2;

    borderRects[2] = RectangleShape<2, int>();
    borderRects[2].mExtent[0] = lowerRightBorder[0] - topLeftBorder[0];
    borderRects[2].mExtent[1] = lowerRightBorder[1] - lowerRightRect[1];
    borderRects[2].mCenter[0] = topLeftBorder[0] + borderRects[2].mExtent[0] / 2;
    borderRects[2].mCenter[1] = lowerRightRect[1] + borderRects[2].mExtent[1] / 2;

    borderRects[3] = RectangleShape<2, int>();
    borderRects[3].mExtent[0] = topLeftRect[0] - topLeftBorder[0];
    borderRects[3].mExtent[1] = lowerRightRect[1] - topLeftRect[1];
    borderRects[3].mCenter[0] = topLeftBorder[0] + borderRects[3].mExtent[0] / 2;
    borderRects[3].mCenter[1] = topLeftRect[1] + borderRects[3].mExtent[1] / 2;

    Renderer* renderer = Renderer::Get();
    renderer->SetBlendState(mBlendState);

    const std::shared_ptr<BaseUISkin>& skin = mUI->GetSkin();
    skin->Draw2DRectangle(
        mColors[0], mColors[3], mVisual, mainRect, &mAbsoluteClippingRect);

    for (size_t i = 0; i <= 3; i++)
        skin->Draw2DRectangle(mBorderColors[i], mVisual, borderRects[i], &mAbsoluteClippingRect);

    renderer->SetDefaultBlendState();

    BaseUIElement::Draw();
}

